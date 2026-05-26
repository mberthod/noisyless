/**
 * @file mqtt.c
 * @brief MQTT client for NOISYLESS HeatMap master node.
 * @details Publishes heatmap JSON to broker.
 * @author Hermes Agent / ELEC-CORE
 * @date 2026-05-24
 */

#include "mqtt.h"
#include <string.h>
#include "esp_log.h"
#include "mqtt_client.h"

#define TAG "MQTT"

#define MQTT_TOPIC_HEATMAP "noisyless/heatmap"

static esp_mqtt_client_handle_t client = NULL;
static bool connected = false;

/* ── Event handler ── */

static void mqtt_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data) {
    esp_mqtt_event_handle_t event = event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to broker");
            connected = true;
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Disconnected from broker");
            connected = false;
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGV(TAG, "Message published (msg_id=%d)", event->msg_id);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT error");
            connected = false;
            break;

        default:
            break;
    }
}

/* ── Public API ── */

bool mqtt_init(const char *broker_ip, int port, const char *client_id) {
    if (client) return true;

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = NULL,
        .broker.address.hostname = broker_ip,
        .broker.address.port = port,
        .broker.address.transport = MQTT_TRANSPORT_OVER_TCP,
        .credentials.client_id = client_id,
        .session.keepalive = 60,
        .session.disable_clean_session = false,
    };

    client = esp_mqtt_client_init(&cfg);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return false;
    }

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);

    esp_err_t err = esp_mqtt_client_start(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "MQTT client started — broker=%s:%d, id=%s",
             broker_ip, port, client_id);
    return true;
}

bool mqtt_publish_heatmap(const char *json_payload) {
    if (!client || !connected) {
        ESP_LOGW(TAG, "Not connected, cannot publish");
        return false;
    }

    int msg_id = esp_mqtt_client_publish(client, MQTT_TOPIC_HEATMAP,
                                          json_payload, 0, 1, 0);
    if (msg_id < 0) {
        ESP_LOGE(TAG, "Publish failed");
        return false;
    }

    ESP_LOGI(TAG, "Heatmap published (%d bytes, msg_id=%d)",
             strlen(json_payload), msg_id);
    return true;
}

void mqtt_deinit(void) {
    if (client) {
        esp_mqtt_client_stop(client);
        esp_mqtt_client_destroy(client);
        client = NULL;
    }
    connected = false;
    ESP_LOGI(TAG, "MQTT deinitialized");
}
