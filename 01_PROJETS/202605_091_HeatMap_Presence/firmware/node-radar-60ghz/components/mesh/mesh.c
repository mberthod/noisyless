/**
 * @file mesh.c
 * @brief ESP-NOW Mesh Networking layer for NOISYLESS HeatMap.
 * @details Manages ESP-NOW init, peer tracking, election, and radar data transmission.
 * @author Hermes Agent / ELEC-CORE
 * @date 2026-05-24
 */

#include "mesh.h"
#include <string.h>
#include <math.h>
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define TAG "MESH"

/* ── Constants ── */
#define ESPNOW_PMK_LEN    16
#define ESPNOW_LMK_LEN    16
#define ESPNOW_CHANNEL    1
#define ESPNOW_MAX_PEERS  20
#define ESPNOW_PAYLOAD_MAX 250

/* ── Peer info ── */
typedef struct {
    uint8_t mac[6];
    bool present;
} mesh_peer_entry_t;

/* ── Globals ── */
static bool mesh_ready = false;
static mesh_master_callback_t master_callback = NULL;
static mesh_peer_entry_t peers[ESPNOW_MAX_PEERS];
static uint8_t peer_count = 0;
static uint8_t local_mac[6];
static SemaphoreHandle_t send_lock = NULL;

/* ── Forward declarations ── */
static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status);
static void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int data_len);

/* ── Utility ── */

static void get_local_mac(void) {
    esp_wifi_get_mac(WIFI_IF_STA, local_mac);
}

/* ── Peer management ── */

static mesh_peer_entry_t *find_peer(const uint8_t *mac) {
    for (int i = 0; i < peer_count; i++) {
        if (memcmp(peers[i].mac, mac, 6) == 0) {
            return &peers[i];
        }
    }
    return NULL;
}

static esp_err_t add_peer(const uint8_t *mac) {
    if (peer_count >= ESPNOW_MAX_PEERS) {
        ESP_LOGW(TAG, "Peer table full, cannot add");
        return ESP_ERR_NO_MEM;
    }
    if (find_peer(mac)) {
        return ESP_OK; /* Already registered */
    }

    memcpy(peers[peer_count].mac, mac, 6);
    peers[peer_count].present = true;

    esp_now_peer_info_t peer_info = {0};
    memcpy(peer_info.peer_addr, mac, 6);
    peer_info.channel = ESPNOW_CHANNEL;
    peer_info.encrypt = false;

    esp_err_t err = esp_now_add_peer(&peer_info);
    if (err == ESP_OK) {
        peer_count++;
        ESP_LOGI(TAG, "Peer added: %02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    return err;
}

/* ── Send callback ── */

static void espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGV(TAG, "Send OK to %02X:%02X", mac_addr[4], mac_addr[5]);
    } else {
        ESP_LOGW(TAG, "Send FAIL to %02X:%02X", mac_addr[4], mac_addr[5]);
    }
}

/* ── Receive callback ── */

static void espnow_recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int data_len) {
    /* Register unknown sender as peer */
    add_peer(info->src_addr);

    /* Forward to master callback if set */
    if (master_callback) {
        master_callback(data, data_len);
    }

    ESP_LOGV(TAG, "Recv %d bytes from %02X:%02X", data_len,
             info->src_addr[4], info->src_addr[5]);
}

/* ── Public API ── */

bool mesh_init(bool initial_role) {
    if (mesh_ready) return true;

    /* Init WiFi in station mode for ESP-NOW */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));
    ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t*)"noisylessmeshkey"));

    send_lock = xSemaphoreCreateMutex();
    get_local_mac();

    ESP_LOGI(TAG, "Mesh initialized — role=%s, MAC=%02X:%02X:%02X:%02X:%02X:%02X",
             initial_role ? "MASTER" : "SLAVE",
             local_mac[0], local_mac[1], local_mac[2],
             local_mac[3], local_mac[4], local_mac[5]);

    mesh_ready = true;
    return true;
}

bool mesh_set_master_callback(mesh_master_callback_t callback) {
    if (!mesh_ready) return false;
    master_callback = callback;
    ESP_LOGI(TAG, "Master callback registered");
    return true;
}

bool mesh_send_radar_payload(const ms72sf1_frame_t *frame) {
    if (!mesh_ready || peer_count == 0) return false;

    /* Build binary payload */
    uint8_t buf[ESPNOW_PAYLOAD_MAX];
    size_t offset = 0;

    buf[offset++] = MESH_TYPE_DATA_RADAR;
    memcpy(buf + offset, local_mac, 6); offset += 6;
    buf[offset++] = 0; /* battery_mv placeholder */
    buf[offset++] = 0;
    buf[offset++] = 0; /* seq_num placeholder */
    buf[offset++] = 0;
    buf[offset++] = 0;
    buf[offset++] = 0;
    buf[offset++] = frame->target_count;

    for (int i = 0; i < frame->target_count && i < MS72SF1_MAX_TARGETS; i++) {
        mesh_radar_target_t t;
        t.x_cm    = (int16_t)(frame->targets[i].x_m * 100.0f);
        t.y_cm    = (int16_t)(frame->targets[i].y_m * 100.0f);
        t.z_cm    = (int16_t)(frame->targets[i].z_m * 100.0f);
        float v   = fabsf(frame->targets[i].vx) + fabsf(frame->targets[i].vy);
        t.v_cm_s  = (int16_t)(v * 100.0f);
        t.quality = (uint8_t)frame->targets[i].q;

        memcpy(buf + offset, &t, sizeof(t));
        offset += sizeof(t);
    }

    xSemaphoreTake(send_lock, pdMS_TO_TICKS(100));

    /* Broadcast to all peers */
    for (int i = 0; i < peer_count; i++) {
        esp_now_send(peers[i].mac, buf, offset);
    }

    xSemaphoreGive(send_lock);
    return true;
}

void mesh_deinit(void) {
    if (!mesh_ready) return;
    esp_now_deinit();
    esp_wifi_stop();
    mesh_ready = false;
    ESP_LOGI(TAG, "Mesh deinitialized");
}
