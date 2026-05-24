#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_now.h"

/* ── Frame types ── */
#define MESH_TYPE_DATA_TOF    0x01
#define MESH_TYPE_HEARTBEAT   0x02
#define MESH_TYPE_ELECTION    0x03
#define MESH_TYPE_HEATMAP     0x04
#define MESH_TYPE_ACK         0x05

/* ── Timing ── */
#define MESH_HEARTBEAT_MS      5000
#define MESH_MASTER_TIMEOUT_MS 15000
#define MESH_BOOT_LISTEN_MS    2000

/* ── Limits ── */
#define MESH_MAX_PEERS         20
#define ESP_NOW_MAX_PAYLOAD    250

/* ── Node roles ── */
typedef enum {
    ROLE_UNKNOWN = 0,
    ROLE_SLAVE,
    ROLE_MASTER
} mesh_role_t;

/* ── Peer info ── */
typedef struct {
    uint8_t mac[6];
    mesh_role_t role;
    bool alive;
    uint32_t last_heartbeat_ms;
    uint16_t battery_mv;
} mesh_peer_t;

/* ── DATA_TOF payload (binary, max 41 bytes) ── */
typedef struct __attribute__((packed)) {
    uint8_t  type;           /* MESH_TYPE_DATA_TOF */
    uint8_t  mac[6];
    uint8_t  battery_mv;     /* mV / 100  (3.7V → 37) */
    uint16_t seq_num;
    uint8_t  cluster_count;  /* 0-5 */
    /* clusters follow inline: 6 bytes each */
    /* { x_cm: int16, y_cm: int16, cells: uint8, conf: uint8 } */
} mesh_data_tof_t;

typedef struct __attribute__((packed)) {
    int16_t x_cm;       /* centroid X relative to node pose */
    int16_t y_cm;
    uint8_t cells;
    uint8_t confidence;  /* 0-15 from TMF8829 */
} mesh_cluster_t;

/* ── HEARTBEAT payload ── */
typedef struct __attribute__((packed)) {
    uint8_t  type;           /* MESH_TYPE_HEARTBEAT */
    uint8_t  mac[6];
    uint8_t  role;           /* ROLE_MASTER or ROLE_SLAVE */
    uint8_t  battery_mv;
    uint8_t  master_flag;    /* 1 if sender is current master */
} mesh_heartbeat_t;

/* ── ELECTION payload ── */
typedef struct __attribute__((packed)) {
    uint8_t type;            /* MESH_TYPE_ELECTION */
    uint8_t mac[6];
} mesh_election_t;

/* ── Public API ── */

esp_err_t mesh_init(void);
esp_err_t mesh_deinit(void);

/* Called every 5s by the heartbeat timer */
void mesh_heartbeat_tick(void);

/* Send ToF clusters to master (slave only) */
esp_err_t mesh_send_tof_clusters(const uint8_t *mac, uint16_t battery_mv,
                                 uint16_t seq, uint8_t cluster_count,
                                 const mesh_cluster_t *clusters);

/* Return current role */
mesh_role_t mesh_get_role(void);

/* Return master MAC (NULL if no master) */
const uint8_t *mesh_get_master_mac(void);

/* Callback: called when master has received enough data for fusion */
typedef void (*mesh_master_callback_t)(void);
void mesh_set_master_callback(mesh_master_callback_t cb);

/* Get peer list for debugging */
uint8_t mesh_get_peer_count(void);
const mesh_peer_t *mesh_get_peers(void);

/* On ESP-NOW receive — called from event handler */
void mesh_on_recv(const uint8_t *mac, const uint8_t *data, int len);
