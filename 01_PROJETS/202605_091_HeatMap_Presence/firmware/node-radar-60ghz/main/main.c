/*
 * main.c — NOISYLESS HeatMap Node Radar 60GHz firmware.
 *
 * ESP32-S3 + MinewSemi MS72SF1 (UART 115200 bauds).
 * Roles: SLAVE (default) = radar read → parse → ESP-NOW to master.
 *        MASTER (elected)   = collect all nodes → fusion → MQTT.
 *
 * One binary for all roles. Role determined by mesh election.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "driver/adc.h"

#include "ms72sf1.h"
#include "mesh.h"
#include "config.h"
#include "mqtt.h"

#define TAG "MAIN"

/* ── Hardware pins ── */
#define LED_RED    GPIO_NUM_48
#define LED_GREEN  GPIO_NUM_47
#define LED_BLUE   GPIO_NUM_38
#define BAT_ADC    ADC1_CHANNEL_0  /* GPIO1 */

#define UART_NUM_RADAR   UART_NUM_2
#define RADAR_TX_PIN     GPIO_NUM_17
#define RADAR_RX_PIN     GPIO_NUM_18

/* ── MQTT config ── */
#define MQTT_BROKER_IP  "192.168.0.176"
#define MQTT_PORT       8883

/* ── Timing ── */
#define SENSOR_PERIOD_MS    1000
#define HEALTH_PERIOD_MS    30000
#define MASTER_FUSION_MS    5000
#define CRITICAL_BAT_SLEEP_S 600

/* ── Globals ── */
static SemaphoreHandle_t sensor_sem = NULL;
static bool master_mode = false;

/* ── Forward declarations ── */
static void sensor_task(void *arg);
static void health_task(void *arg);
static void master_task(void *arg);
static void sensor_wake_callback(void *arg);
static void master_recv_callback(const void *payload, size_t size);
static uint16_t get_battery_mv(void);
static void led_init(void);

/* ══════════════════════════════════════════════════════════════
 * Sensor task (Core 0)
 * ══════════════════════════════════════════════════════════════ */

static void sensor_task(void *arg) {
    ESP_LOGI(TAG, "Sensor task started");

    if (!ms72sf1_init()) {
        ESP_LOGE(TAG, "MS72SF1 init failed — sensor task aborting");
        vTaskDelete(NULL);
        return;
    }

    for (;;) {
        xSemaphoreTake(sensor_sem, pdMS_TO_TICKS(SENSOR_PERIOD_MS * 2));

        ms72sf1_frame_t frame;
        if (!ms72sf1_read_frame(&frame)) {
            continue;
        }

        /* Send via ESP-NOW to mesh master */
        if (frame.target_count > 0) {
            mesh_send_radar_payload(&frame);
            ESP_LOGD(TAG, "Radar frame: %d targets sent", frame.target_count);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ══════════════════════════════════════════════════════════════
 * Health task (Core 0)
 * ══════════════════════════════════════════════════════════════ */

static uint16_t get_battery_mv(void) {
    int raw = adc1_get_raw(BAT_ADC);
    /* Voltage divider 1:2 → ADC = Vbat * (100k / (100k + 100k)) */
    return (uint16_t)(raw * 3.3f / 4095.0f * 2.0f * 1000.0f);
}

static void health_task(void *arg) {
    /* Init ADC */
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(BAT_ADC, ADC_ATTEN_DB_11);

    for (;;) {
        uint16_t bat = get_battery_mv();

        /* LED indication */
        gpio_set_level(LED_GREEN, (bat > 3200) ? 0 : 1);
        gpio_set_level(LED_RED,   (bat < 3000) ? 0 : 1);

        ESP_LOGI(TAG, "Health: bat=%dmV, role=%s",
                 bat, master_mode ? "MASTER" : "SLAVE");

        /* Critical battery → deep sleep */
        if (bat < 3000) {
            ESP_LOGW(TAG, "Battery critical (%dmV) — deep sleep %ds",
                     bat, CRITICAL_BAT_SLEEP_S);
            gpio_set_level(LED_RED, 0);
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_deep_sleep(CRITICAL_BAT_SLEEP_S * 1000000ULL);
        }

        vTaskDelay(pdMS_TO_TICKS(HEALTH_PERIOD_MS));
    }
}

/* ══════════════════════════════════════════════════════════════
 * Master task (Core 0) — only runs if this node is elected master
 * ══════════════════════════════════════════════════════════════ */

#define MASTER_PAYLOAD_BUF_SIZE  4096
#define MASTER_MAX_SLAVES        20

typedef struct {
    uint8_t mac[6];
    uint16_t battery_mv;
    ms72sf1_frame_t last_frame;
    uint32_t last_update_ms;
} slave_data_t;

static slave_data_t slave_data[MASTER_MAX_SLAVES];
static uint8_t slave_count = 0;

static slave_data_t *find_slave(const uint8_t *mac) {
    for (int i = 0; i < slave_count; i++) {
        if (memcmp(slave_data[i].mac, mac, 6) == 0) {
            return &slave_data[i];
        }
    }
    if (slave_count < MASTER_MAX_SLAVES) {
        slave_data_t *s = &slave_data[slave_count++];
        memcpy(s->mac, mac, 6);
        return s;
    }
    return NULL;
}

static void master_recv_callback(const void *payload, size_t size) {
    if (size < 12) return; /* Need at least type + mac + count */

    const uint8_t *p = (const uint8_t *)payload;

    uint8_t type = p[0];
    if (type != MESH_TYPE_DATA_RADAR) return;

    const uint8_t *src_mac = p + 1;
    uint8_t target_count = p[12];

    slave_data_t *slave = find_slave(src_mac);
    if (!slave) return;

    slave->last_update_ms = esp_timer_get_time() / 1000;
    slave->last_frame.target_count = target_count;

    const mesh_radar_target_t *targets = (const mesh_radar_target_t *)(p + 13);
    for (int i = 0; i < target_count && i < MS72SF1_MAX_TARGETS; i++) {
        slave->last_frame.targets[i].x_m = targets[i].x_cm / 100.0f;
        slave->last_frame.targets[i].y_m = targets[i].y_cm / 100.0f;
        slave->last_frame.targets[i].z_m = targets[i].z_cm / 100.0f;
        slave->last_frame.targets[i].q   = targets[i].quality;
    }
}

static void build_heatmap_json(char *buf, size_t buf_size) {
    int offset = snprintf(buf, buf_size,
        "{\"schema\":\"nl.telemetry.v1\",\"model\":\"nl-map-out\","
        "\"duid\":\"radar-master\",\"ts\":%lld,\"metrics\":{\"targets\":[",
        (long long)(esp_timer_get_time() / 1000));

    bool first = true;
    for (int s = 0; s < slave_count; s++) {
        ms72sf1_frame_t *f = &slave_data[s].last_frame;
        for (int i = 0; i < f->target_count; i++) {
            ms72sf1_target_t *t = &f->targets[i];
            if (offset + 120 >= (int)buf_size) break;
            if (!first) buf[offset++] = ','; else first = false;
            offset += snprintf(buf + offset, buf_size - offset,
                "{\"id\":%d,\"x\":%.2f,\"y\":%.2f,\"z\":%.2f,"
                "\"vx\":%.2f,\"vy\":%.2f,\"q\":%d}",
                (int)t->id, t->x_m, t->y_m, t->z_m,
                t->vx, t->vy, (int)t->q);
        }
    }

    snprintf(buf + offset, buf_size - offset,
        "],\"count\":%d},\"health\":{\"battery_mv\":%d,\"rssi\":-60,\"fw\":\"1.0.0\"}}",
        slave_count, get_battery_mv());
}

static void master_task(void *arg) {
    ESP_LOGI(TAG, "Master task started");

    if (!mqtt_init(MQTT_BROKER_IP, MQTT_PORT, "radar-master")) {
        ESP_LOGE(TAG, "MQTT init failed — master task aborting");
        vTaskDelete(NULL);
        return;
    }

    /* Wait for MQTT connection */
    vTaskDelay(pdMS_TO_TICKS(3000));

    for (;;) {
        if (!master_mode) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        /* Build heatmap JSON */
        char json_buf[MASTER_PAYLOAD_BUF_SIZE];
        build_heatmap_json(json_buf, sizeof(json_buf));

        /* Publish to MQTT */
        mqtt_publish_heatmap(json_buf);

        ESP_LOGI(TAG, "Master fusion: %d slaves, payload=%d bytes",
                 slave_count, strlen(json_buf));

        vTaskDelay(pdMS_TO_TICKS(MASTER_FUSION_MS));
    }
}

/* ══════════════════════════════════════════════════════════════
 * LED init (active low)
 * ══════════════════════════════════════════════════════════════ */

static void led_init(void) {
    gpio_reset_pin(LED_RED);
    gpio_reset_pin(LED_GREEN);
    gpio_reset_pin(LED_BLUE);
    gpio_set_direction(LED_RED,   GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_GREEN, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_BLUE,  GPIO_MODE_OUTPUT);
    /* Active low — all off */
    gpio_set_level(LED_RED,   1);
    gpio_set_level(LED_GREEN, 1);
    gpio_set_level(LED_BLUE,  1);
    /* Blink green on boot */
    gpio_set_level(LED_GREEN, 0);
}

/* ══════════════════════════════════════════════════════════════
 * Timer callback
 * ══════════════════════════════════════════════════════════════ */

static void sensor_wake_callback(void *arg) {
    xSemaphoreGive(sensor_sem);
}

/* ══════════════════════════════════════════════════════════════
 * App main
 * ══════════════════════════════════════════════════════════════ */

void app_main(void) {
    ESP_LOGI(TAG, "NOISYLESS HeatMap — Node Radar 60GHz v1.0.0");
    ESP_LOGI(TAG, "ESP-IDF %s, FreeRTOS", esp_get_idf_version());

    /* Init hardware */
    led_init();

    /* Init NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Init TCP/IP stack */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Load config */
    config_init();

    identity_config_t identity = {0};
    config_load_identity(&identity);

    calib_config_t calib = {0};
    config_load_calib(&calib);

    sensor_config_t sensor_cfg = {0};
    config_load_sensor(&sensor_cfg);

    power_config_t power_cfg = {0};
    config_load_power(&power_cfg);

    ESP_LOGI(TAG, "Config loaded — role=%d, z=%.1fm, period=%lums",
             identity.role, calib.z_m, sensor_cfg.period_ms);

    /* Init mesh */
    master_mode = (identity.role == 2);
    mesh_init(master_mode);
    mesh_set_master_callback(master_recv_callback);

    /* Init semaphore for sensor timing */
    sensor_sem = xSemaphoreCreateBinary();

    /* Start tasks */
    xTaskCreatePinnedToCore(sensor_task, "sensor", 4096, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(health_task, "health", 2048, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(master_task, "master", 8192, NULL, 2, NULL, 0);

    /* Sensor timer: wake sensor_task periodically */
    esp_timer_handle_t sensor_timer;
    const esp_timer_create_args_t st_args = {
        .callback = sensor_wake_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "sensor_timer",
    };
    esp_timer_create(&st_args, &sensor_timer);
    esp_timer_start_periodic(sensor_timer, SENSOR_PERIOD_MS * 1000);

    ESP_LOGI(TAG, "App main running — boot complete");
    gpio_set_level(LED_GREEN, 1); /* Boot complete */

    /* Idle loop */
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
