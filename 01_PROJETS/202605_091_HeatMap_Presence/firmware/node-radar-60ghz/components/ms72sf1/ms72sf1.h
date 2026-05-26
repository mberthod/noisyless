/**
 * @file ms72sf1.h
 * @brief Driver for the MinewSemi MS72SF1 radar sensor.
 * @details Implements initialization, reading the binary TLV frame, and cleanup.
 * @author Hermes Agent
 * @date 2026-05-24
 * @license NOISYLESS/Proprietary
 */

#ifndef MS72SF1_H
#define MS72SF1_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_log.h"

// Maximum number of targets supported (as per specifications)
#define MS72SF1_MAX_TARGETS 10

/**
 * @brief Structure representing a single tracked target.
 * All values are float32 (4 bytes).
 */
typedef struct {
    float q;      ///< Quality score (0.0 to 100.0)
    float id;     ///< Unique ID of the target
    float x_m;    ///< Position X in meters
    float y_m;    ///< Position Y in meters
    float z_m;    ///< Position Z in meters
    float vx;     ///< Velocity X in m/s
    float vy;     ///< Velocity Y in m/s
    float vz;     ///< Velocity Z in m/s
} ms72sf1_target_t;

/**
 * @brief Structure representing the full radar frame from the sensor.
 * The first byte holds the count, followed by the array of target data.
 */
typedef struct {
    uint8_t target_count;
    ms72sf1_target_t targets[MS72SF1_MAX_TARGETS];
} ms72sf1_frame_t;

/**
 * @brief Initializes the MS72SF1 driver, including AT command setup.
 * @param uart_handle The handle for the UART peripheral.
 * @return true if initialization was successful, false otherwise.
 */
bool ms72sf1_init(void);

/**
 * @brief Reads a single complete radar frame from the sensor.
 * This function handles the UART protocol (Header -> Length -> Frame -> TLVs).
 *
 * @param frame Pointer to the structure to populate with the received frame data.
 * @return true if a full frame was successfully read, false otherwise.
 */
bool ms72sf1_read_frame(ms72sf1_frame_t* frame);

/**
 * @brief Deinitializes the MS72SF1 driver.
 * Cleans up resources (e.g., sending AT+STOP).
 */
void ms72sf1_deinit(void);

#endif // MS72SF1_H
