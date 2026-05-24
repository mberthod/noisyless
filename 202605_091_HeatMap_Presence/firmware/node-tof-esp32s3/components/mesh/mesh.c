/*
 * mesh.c — ESP-NOW mesh layer for NOISYLESS HeatMap
 *
 * Protocol:
 *   1. Boot: listen 2s. If no master heartbeat → run election.
 *   2. Election: lowest MAC wins (bully algorithm).
 *   3. Master: broadcasts heartbeat every 5s.
 *   4. Slaves: forward ToF cluster payloads to master.
 *   5. Master timeout >15s → re-election.
 *
 * Memory: bounded. No malloc after init. Stack-safe.
 */

#include "mesh.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#define TAG "MESH"

/* ── Static state ── */
static uint8_t            my_mac[6];
static mesh_role_t        my_role = ROLE_UNKNOWN;
static uint8_t            master_mac[6];
static bool               has_master = false;
static uint32_t           last_master_heartbeat_ms = 0;
static uint32_t           election_phase_start_ms = 0;
static bool               election_running = false;
static mesh_peer_t        peers[MESH_MAX_PEERS];
static uint8_t            peer_count = 0;
static mesh_master_callback_t master_cb = NULL;

/* ── helpers ── */

static uint32_t now_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static bool mac_eq(const uint8_t *a, const uint8_t *b) {
    return memcmp(a, b, 6) == 0;
}

static bool mac_lt(const uint8_t *a, const uint8_t *b) {
    /* Lexicographic compare */
    for (int i = 0; i < 6; i++) {
        if (a[i] < b[i]) return true;
        if (a[i] > b[i]) return false;
    }
    return false; /* equal */
}

static mesh_peer_t *find_peer(const uint8_t *mac) {
    for (int i = 0; i < peer_count; i++) {
        if (mac_eq(peers[i].mac, mac)) return &peers[i];
    }
    return NULL;
}

static mesh_peer_t *add_peer(const uint8_t *mac) {
    if (peer_count >= MESH_MAX_PEERS) return NULL;
    mesh_peer_t *p = &peers[peer_count];
    memcpy(p->mac, mac, 6);
    p->role = ROLE_UNKNOWN;
    p->alive = true;
    p->last_heartbeat_ms = now_ms();
    p->battery_mv = 0;

    /* Add to ESP-NOW peer list */
    esp_now_peer_info_t peer_info = {0};
    memcpy(peer_info.peer_addr, mac, 6);
    peer_info.channel = 1;
    peer_info.ifidx = WIFI_IF_STA;
    peer_info.encrypt = false;
    esp_now_add_peer(&peer_info);

    peer_count++;
    return p;
}

static void broadcast(const uint8_t *data, size_t len) {
    /* ESP-NOW broadcast — use broadcast MAC */
    uint8_t bc_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    esp_now_send(bc_mac, data, len);
}

static void send_to(const uint8_t *mac, const uint8_t *data, size_t len) {
    esp_now_send(mac, data, len);
}

/* ── Election ── */

static void start_election(void) {
    if (election_running) return;
    ESP_LOGI(TAG, "Starting master election...");
    election_running = true;
    election_phase_start_ms = now_ms();

    /* Send my MAC as election broadcast */
    mesh_election_t elec = {.type = MESH_TYPE_ELECTION};
    memcpy(elec.mac, my_mac, 6);
    broadcast((uint8_t *)&elec, sizeof(elec));

    my_role = ROLE_SLAVE; /* default to slave, will become master if I win */
}

static void election_timeout_check(void) {
    if (!election_running) return;

    if ((now_ms() - election_phase_start_ms) > 500) {
        /* Election settled: check if I'm the lowest MAC among known peers */
        bool i_win = true;
        for (int i = 0; i < peer_count; i++) {
            if (mac_lt(peers[i].mac, my_mac)) {
                i_win = false;
                break;
            }
        }

        election_running = false;
        if (i_win) {
            my_role = ROLE_MASTER;
            memcpy(master_mac, my_mac, 6);
            has_master = true;
            ESP_LOGI(TAG, "Election won — I am MASTER");
            if (master_cb) master_cb();
        } else {
            /* Find master: lowest MAC among peers */
            const uint8_t *winner = peers[0].mac;
            for (int i = 1; i < peer_count; i++) {
                if (mac_lt(peers[i].mac, winner)) {
                    winner = peers[i].mac;
                }
            }
            memcpy(master_mac, winner, 6);
            has_master = true;
            my_role = ROLE_SLAVE;
            ESP_LOGI(TAG, "Election lost — master is %02x:%02x:%02x:%02x:%02x:%02x",
                     winner[0], winner[1], winner[2], winner[3], winner[4], winner[5]);
        }
    }
}

/* ── ESP-NOW callbacks ── */

static void on_send(const uint8_t *mac, esp_now_send_status_t status) {
    /* Silent — failures handled at protocol level */
}

void mesh_on_recv(const uint8_t *mac, const uint8_t *data, int len) {
    if (len < 1) return;

    uint8_t type = data[0];

    switch (type) {
    case MESH_TYPE_HEARTBEAT: {
        mesh_heartbeat_t *hb = (mesh_heartbeat_t *)data;
        mesh_peer_t *peer = find_peer(mac);
        if (!peer) peer = add_peer(mac);
        if (!peer) break;

        peer->alive = true;
        peer->last_heartbeat_ms = now_ms();
        peer->battery_mv = hb->battery_mv * 100;
        peer->role = (hb->role == ROLE_MASTER) ? ROLE_MASTER : ROLE_SLAVE;

        if (hb->master_flag) {
            memcpy(master_mac, mac, 6);
            has_master = true;
            last_master_heartbeat_ms = now_ms();
        }
        break;
    }

    case MESH_TYPE_ELECTION: {
        mesh_election_t *elec = (mesh_election_t *)data;
        mesh_peer_t *peer = find_peer(elec->mac);
        if (!peer) peer = add_peer(elec->mac);

        /* If I see a MAC lower than mine, I lose */
        if (mac_lt(elec->mac, my_mac)) {
            election_running = false;
            election_timeout_check();
        }
        break;
    }

    case MESH_TYPE_DATA_TOF:
        /* Forward to master processing if I'm master */
        if (my_role == ROLE_MASTER && master_cb) {
            /* store in master buffer (handled in main.c master_task) */
            extern void master_store_tof_payload(const uint8_t *data, int len);
            master_store_tof_payload(data, len);
        }
        break;

    default:
        break;
    }
}

/* ── Public API ── */

esp_err_t mesh_init(void) {
    /* Read MAC */
    esp_efuse_mac_get_default(my_mac);
    ESP_LOGI(TAG, "My MAC: %02x:%02x:%02x:%02x:%02x:%02x",
             my_mac[0], my_mac[1], my_mac[2], my_mac[3], my_mac[4], my_mac[5]);

    /* Init NVS (required by ESP-NOW) */
    nvs_flash_init();

    /* Init WiFi in STA mode (required for ESP-NOW) */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    /* Init ESP-NOW */
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(on_send));
    ESP_ERROR_CHECK(esp_now_register_recv_cb((esp_now_recv_cb_t)mesh_on_recv));

    /* Boot phase: listen 2s, then elect */
    vTaskDelay(pdMS_TO_TICKS(MESH_BOOT_LISTEN_MS));
    start_election();

    ESP_LOGI(TAG, "Mesh initialized, role=%d", my_role);
    return ESP_OK;
}

void mesh_heartbeat_tick(void) {
    election_timeout_check();

    /* Master timeout check (slave only) */
    if (my_role == ROLE_SLAVE && has_master) {
        if ((now_ms() - last_master_heartbeat_ms) > MESH_MASTER_TIMEOUT_MS) {
            ESP_LOGW(TAG, "Master timeout — re-election");
            has_master = false;
            start_election();
        }
    }

    /* Send heartbeat */
    mesh_heartbeat_t hb = {
        .type = MESH_TYPE_HEARTBEAT,
        .role = (uint8_t)my_role,
        .battery_mv = 0,  /* filled by caller with ADC reading */
        .master_flag = (my_role == ROLE_MASTER) ? 1 : 0,
    };
    memcpy(hb.mac, my_mac, 6);
    broadcast((uint8_t *)&hb, sizeof(hb));

    /* Clean stale peers (30s no heartbeat) */
    for (int i = 0; i < peer_count; ) {
        if ((now_ms() - peers[i].last_heartbeat_ms) > 30000) {
            esp_now_del_peer(peers[i].mac);
            memmove(&peers[i], &peers[i + 1], (peer_count - i - 1) * sizeof(mesh_peer_t));
            peer_count--;
        } else {
            i++;
        }
    }
}

esp_err_t mesh_send_tof_clusters(const uint8_t *mac, uint16_t battery_mv,
                                 uint16_t seq, uint8_t cluster_count,
                                 const mesh_cluster_t *clusters)
{
    if (!has_master) return ESP_ERR_INVALID_STATE;
    if (cluster_count > 5) cluster_count = 5;

    uint8_t buf[sizeof(mesh_data_tof_t) + 5 * sizeof(mesh_cluster_t)];
    mesh_data_tof_t *data = (mesh_data_tof_t *)buf;
    data->type = MESH_TYPE_DATA_TOF;
    memcpy(data->mac, my_mac, 6);
    data->battery_mv = (uint8_t)(battery_mv / 100);
    data->seq_num = seq;
    data->cluster_count = cluster_count;

    mesh_cluster_t *dst = (mesh_cluster_t *)(buf + sizeof(mesh_data_tof_t));
    memcpy(dst, clusters, cluster_count * sizeof(mesh_cluster_t));

    send_to(master_mac, buf, sizeof(mesh_data_tof_t) + cluster_count * sizeof(mesh_cluster_t));
    return ESP_OK;
}

mesh_role_t mesh_get_role(void)     { return my_role; }
const uint8_t *mesh_get_master_mac(void) { return has_master ? master_mac : NULL; }
void mesh_set_master_callback(mesh_master_callback_t cb) { master_cb = cb; }
uint8_t mesh_get_peer_count(void)   { return peer_count; }
const mesh_peer_t *mesh_get_peers(void) { return peers; }
