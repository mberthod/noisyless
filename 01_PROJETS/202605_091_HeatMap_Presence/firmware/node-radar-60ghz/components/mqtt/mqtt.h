/**
 * @file mqtt.h
 * @brief MQTT communication client interface.
 * @details Handles connecting, publishing heatmap data as JSON payload.
 * @author Hermes Agent
 * @date 2026-05-24
 * @license NOISYLESS/Proprietary
 */

#ifndef MQTT_H
#define MQTT_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initializes the MQTT client connection and associated resources.
 * Must be called before any publishing attempts.
 * @param broker_ip IP address of the MQTT broker.
 * @param port The MQTT port (e.g., 8883 for TLS).
 * @param client_id Client identifier.
 * @return true if initialization was successful, false otherwise.
 */
bool mqtt_init(const char *broker_ip, int port, const char *client_id);

/**
 * @brief Publishes the consolidated heatmap data payload as JSON.
 * This function assumes the JSON payload has already been constructed by the master node's logic.
 *
 * @param json_payload The pre-formatted JSON string content.
 * @return true if the publish operation was initiated successfully, false otherwise.
 */
bool mqtt_publish_heatmap(const char *json_payload);

/**
 * @brief Disconnects the MQTT client and cleans up resources.
 */
void mqtt_deinit(void);

#endif // MQTT_H
