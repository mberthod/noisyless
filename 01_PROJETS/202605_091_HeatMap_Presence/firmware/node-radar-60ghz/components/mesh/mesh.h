/**
 * @file mesh.h
 * @brief ESP-NOW Mesh Networking layer for NOISYLESS HeatMap.
 * @details Manages ESP-NOW connectivity, peer tracking, and radar data transmission.
 * @author Hermes Agent / ELEC-CORE
 * @date 2026-05-24
 */

#ifndef MESH_H
#define MESH_H

#include <stdint.h>
#include <stdbool.h>
#include "ms72sf1.h"

/* ── Mesh Types ── */
#define MESH_TYPE_DATA_TOF     0x01
#define MESH_TYPE_HEARTBEAT    0x02
#define MESH_TYPE_ELECTION     0x03
#define MESH_TYPE_HEATMAP      0x04
#define MESH_TYPE_ACK          0x05
#define MESH_TYPE_DATA_RADAR   0x06

/* ── Limits ── */
#define MESH_MAX_PEERS          20
#define ESP_NOW_MAX_PAYLOAD     250

/* ── Radar target (compact binary for mesh transport) ── */
typedef struct __attribute__((packed)) {
    int16_t x_cm;
    int16_t y_cm;
    int16_t z_cm;
    int16_t v_cm_s;
    uint8_t quality;
} mesh_radar_target_t;

/* ── Public API ── */

/**
 * @brief Initialize ESP-NOW mesh networking.
 * @param initial_role true for MASTER, false for SLAVE.
 * @return true on success.
 */
bool mesh_init(bool initial_role);

/**
 * @brief Send radar frame payload over ESP-NOW to all peers.
 * @param frame Pointer to filled ms72sf1_frame_t.
 * @return true if send initiated.
 */
bool mesh_send_radar_payload(const ms72sf1_frame_t *frame);

/**
 * @brief Callback type for master receiving ESP-NOW data.
 * @param payload Raw binary payload.
 * @param size Payload size in bytes.
 */
typedef void (*mesh_master_callback_t)(const void *payload, size_t size);

/**
 * @brief Register callback invoked when master receives a frame.
 * @param callback Function to call on receive.
 * @return true on success.
 */
bool mesh_set_master_callback(mesh_master_callback_t callback);

/**
 * @brief Clean up mesh resources.
 */
void mesh_deinit(void);

#endif /* MESH_H */
