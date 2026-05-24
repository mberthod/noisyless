/*
 * main.c — NOISYLESS HeatMap Node ToF firmware.
 *
 * ESP32-S3-MINI-1 + TMF8829.
 * Roles: SLAVE (default) = ranging → cluster → ESP-NOW to master.
 *        MASTER (elected) = collect all nodes → fusion → MQTT.
 *
 * One binary for all roles. Role determined by mesh election.
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/adc.h"

#include "tmf8829.h"
#include "cluster.h"
#include "mesh.h"
#include "config.h"

#define TAG "MAIN"

/* ── Hardware pins ── */
#define LED_RED    GPIO_NUM_48
#define LED_GREEN  GPIO_NUM_47
#define LED_BLUE   GPIO_NUM_38   /* SPICLK on some boards — verify */
#define BAT_ADC    ADC1_CHANNEL_0  /* GPIO1 */

/* ── Globals ── */
static identity_config_t   id_cfg;
static calib_config_t      calib_cfg;
static sensor_config_t     sensor_cfg;
static power_config_t      power_cfg;
static SemaphoreHandle_t   sensor_sem = NULL;
static uint16_t            seq_num = 0;

/* ── Sensor task (Core 0) ── */

static void sensor_task(void *arg) {
    ESP_LOGI(TAG, "Sensor task started");

    /* Init TMF8829 */
    if (tmf8829_init() != ESP_OK) {
        ESP_LOGE(TAG, "TMF8829 init failed — sensor task exiting");
        vTaskDelete(NULL);
        return;
    }

    /* Convert result distances to grid for clustering */
    uint16_t grid[TOF_GRID_SIZE][TOF_GRID_SIZE];
    uint8_t  conf[TOF_GRID_SIZE][TOF_GRID_SIZE];

    for (;;) {
        /* Wait for wake signal (timer or external) */
        xSemaphoreTake(sensor_sem, pdMS_TO_TICKS(sensor_cfg.period_ms * 2));

        /* Wake sensor from standby */
        tmf8829_wake();
        vTaskDelay(pdMS_TO_TICKS(5));

        /* Start ranging + read */
        tmf8829_start_ranging();
        tmf8829_result_t result;
        esp_err_t err = tmf8829_read_result(&result);

        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Read failed: %s", esp_err_to_name(err));
            tmf8829_standby();
            continue;
        }

        if (result.valid_zones == 0) {
            /* No targets — send empty cluster payload */
            seq_num++;
            mesh_cluster_t empty = {0};
            mesh_send_tof_clusters(my_mac(), get_battery_mv(),
                                   seq_num, 1, &empty);
            tmf8829_standby();
            continue;
        }

        /* Map result to 16x16 grid. Fill missing cells with max range. */
        memset(grid, 0, sizeof(grid));
        memset(conf, 0, sizeof(conf));
        for (int k = 0; k < 256 && k < 256; k++) {
            uint8_t row = k / 16;
            uint8_t col = k % 16;
            if (k < result.valid_zones) {
                grid[row][col] = result.distances_mm[k];
                conf[row][col] = result.confidence[k];
            }
        }

        /* Cluster */
        cluster_result_t clusters = find_clusters_4way(grid, conf);

        /* Transform local coords → world via node calibration */
        mesh_cluster_t mesh_clusters[MAX_CLUSTERS];
        for (uint8_t c = 0; c < clusters.count; c++) {
            float x = clusters.clusters[c].x_world_m;
            float y = clusters.clusters[c].y_world_m;
            /* Rotate by theta */
            float rad = calib_cfg.theta_deg * 3.14159f / 180.0f;
            float rx = x * cosf(rad) - y * sinf(rad);
            float ry = x * sinf(rad) + y * cosf(rad);
            mesh_clusters[c].x_cm  = (int16_t)((rx + calib_cfg.x_m) * 100.0f);
            mesh_clusters[c].y_cm  = (int16_t)((ry + calib_cfg.y_m) * 100.0f);
            mesh_clusters[c].cells = clusters.clusters[c].cells;
            mesh_clusters[c].confidence = clusters.clusters[c].avg_conf;
        }

        /* Send to master via ESP-NOW */
        if (clusters.count > 0) {
            seq_num++;
            mesh_send_tof_clusters(my_mac(), get_battery_mv(),
                                   seq_num, clusters.count, mesh_clusters);
        }

        /* Standby → deep sleep if battery */
        tmf8829_standby();
        esp_deep_sleep(sensor_cfg.duty_cycle_s * 1000000ULL);
    }
}

/* ── Health task (Core 0) ── */

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
        bool low = (bat < power_cfg.battery_low_mv);

        /* LED indication */
        gpio_set_level(LED_GREEN, low ? 0 : 1);
        gpio_set_level(LED_RED,  low ? 1 : 0);
        gpio_set_level(LED_BLUE, (mesh_get_role() == ROLE_MASTER) ? 1 : 0);

        ESP_LOGI(TAG, "Health: bat=%dmV, role=%d, peers=%d, seq=%d",
                 bat, mesh_get_role(), mesh_get_peer_count(), seq_num);

        /* Deep sleep if battery critically low */
        if (bat < 3000) {
            ESP_LOGW(TAG, "Battery critical — entering deep sleep 10min");
            gpio_set_level(LED_RED, 1);
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_deep_sleep(600 * 1000000ULL);
        }

        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

/* ── Master data store (used by mesh.c callback) ── */
/* Simple ring buffer of last received ToF payloads from slaves */

#define MASTER_BUF_SIZE 20

typedef struct {
    mesh_data_tof_t header;
    mesh_cluster_t  clusters[5];
} master_entry_t;

static master_entry_t master_buf[MASTER_BUF_SIZE];
static uint8_t        master_buf_idx = 0;
static SemaphoreHandle_t master_mutex = NULL;

void master_store_tof_payload(const uint8_t *data, int len) {
    if (!master_mutex) return;
    xSemaphoreTake(master_mutex, pdMS_TO_TICKS(10));

    master_entry_t *entry = &master_buf[master_buf_idx];
    memset(entry, 0, sizeof(*entry));
    memcpy(&entry->header, data, len < sizeof(mesh_data_tof_t) ? len : sizeof(mesh_data_tof_t));
    int cluster_bytes = len - sizeof(mesh_data_tof_t);
    if (cluster_bytes > 0) {
        memcpy(entry->clusters, data + sizeof(mesh_data_tof_t),
               cluster_bytes < (int)sizeof(entry->clusters) ? cluster_bytes : sizeof(entry->clusters));
    }
    master_buf_idx = (master_buf_idx + 1) % MASTER_BUF_SIZE;

    xSemaphoreGive(master_mutex);
}

/* ── Master task (Core 0, conditionally started) ── */

static void master_callback(void) {
    /* Called by mesh when this node becomes master */
    ESP_LOGI(TAG, "MASTER callback — starting fusion");
    /* The master_task is always created but sleeps until activated */
}

static void master_task(void *arg) {
    ESP_LOGI(TAG, "Master task started (waiting for role)");

    for (;;) {
        /* Only run if I'm master */
        if (mesh_get_role() != ROLE_MASTER) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }

        /* Collect all entries from the ring buffer within fusion window (2s) */
        xSemaphoreTake(master_mutex, pdMS_TO_TICKS(100));
        int count = 0;
        /* Quick scan: count recent entries */
        master_entry_t local_buf[MASTER_BUF_SIZE];
        for (int i = 0; i < MASTER_BUF_SIZE; i++) {
            if (master_buf[i].header.cluster_count > 0) {
                memcpy(&local_buf[count], &master_buf[i], sizeof(master_entry_t));
                count++;
            }
        }
        xSemaphoreGive(master_mutex);

        if (count == 0) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        /* Simple fusion: union of all clusters in world coords.
         * Advanced: spatial dedup, weight by confidence, DB merging.
         * V1: just count total clusters → publish via MQTT. */
        uint16_t total_clusters = 0;
        for (int i = 0; i < count; i++) {
            total_clusters += local_buf[i].header.cluster_count;
        }

        ESP_LOGI(TAG, "Master fusion: %d nodes → %d clusters", count, total_clusters);

        /* TODO P3: MQTT publish heatmap JSON */
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* ── LED init ── */

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

/* ── Timer callbacks (C-compatible) ── */

static void sensor_wake_callback(void *arg) {
    xSemaphoreGive(sensor_sem);
}

static void heartbeat_callback(void *arg) {
    mesh_heartbeat_tick();
}

/* ── App main ── */

void app_main(void) {
    ESP_LOGI(TAG, "NOISYLESS HeatMap — Node ToF v1.0.0");
    ESP_LOGI(TAG, "ESP-IDF %s, FreeRTOS", esp_get_idf_version());

    /* Init hardware */
    led_init();

    /* Load config */
    config_init();
    config_load_identity(&id_cfg);
    config_load_calib(&calib_cfg);
    config_load_sensor(&sensor_cfg);
    config_load_power(&power_cfg);

    ESP_LOGI(TAG, "Device: %s, period: %lums", id_cfg.device_id, sensor_cfg.period_ms);

    /* Init mutexes */
    master_mutex = xSemaphoreCreateMutex();
    sensor_sem = xSemaphoreCreateBinary();

    /* Init mesh */
    if (mesh_init() != ESP_OK) {
        ESP_LOGE(TAG, "Mesh init failed — aborting");
        return;
    }
    mesh_set_master_callback(master_callback);

    /* Start tasks */
    xTaskCreatePinnedToCore(sensor_task, "sensor", 4096, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(health_task, "health", 2048, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(master_task, "master", 8192, NULL, 2, NULL, 0);

    /* Sensor timer: wake sensor_task every period_ms */
    esp_timer_handle_t sensor_timer;
    const esp_timer_create_args_t st_args = {
        .callback = sensor_wake_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "sensor_timer",
    };
    esp_timer_create(&st_args, &sensor_timer);
    esp_timer_start_periodic(sensor_timer, sensor_cfg.period_ms * 1000);

    ESP_LOGI(TAG, "App main running");
    gpio_set_level(LED_GREEN, 1); /* Boot complete */

    /* Idle loop */
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
