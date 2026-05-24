/*
 * main.c — NOISYLESS HeatMap Node Radar 60GHz firmware.
 *
 * ESP32-S3 + LD6001A (UART 115200 bauds).
 * Roles: SLAVE (default) = radar read → parse → ESP-NOW to master.
 *        MASTER (elected)   = collect all nodes → fusion → MQTT.
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
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "driver/gpio.h"
#include "driver/adc.h"

/* -- TODO: uncomment when components are ready --
#include "ld6001a.h"
#include "mesh.h"
#include "config.h"
*/

#define TAG "MAIN"

/* ── Hardware pins ── */
#define LED_RED    GPIO_NUM_48
#define LED_GREEN  GPIO_NUM_47
#define LED_BLUE   GPIO_NUM_38
#define BAT_ADC    ADC1_CHANNEL_0  /* GPIO1 */

#define UART_NUM_RADAR   UART_NUM_2
#define RADAR_TX_PIN    GPIO_NUM_17
#define RADAR_RX_PIN    GPIO_NUM_18

/* ── Globals ── */
static SemaphoreHandle_t sensor_sem = NULL;

/* ── Sensor task (Core 0) ── */

static void sensor_task(void *arg) {
    ESP_LOGI(TAG, "Sensor task started");

    /* TODO: ld6001a_init(UART_NUM_RADAR, RADAR_TX_PIN, RADAR_RX_PIN, 115200) */

    for (;;) {
        xSemaphoreTake(sensor_sem, pdMS_TO_TICKS(2000));

        /* TODO: read raw radar frame via UART */
        /*
        ld6001a_frame_t frame;
        if (ld6001a_read_frame(&frame) != ESP_OK) {
            continue;
        }
        */

        /* TODO: parse targets, build mesh payload, send via ESP-NOW */
        /*
        mesh_radar_t payload;
        payload.count = frame.target_count;
        for (int i = 0; i < frame.target_count; i++) {
            payload.targets[i].x_cm = frame.targets[i].x;
            payload.targets[i].y_cm = frame.targets[i].y;
            payload.targets[i].z_cm = frame.targets[i].z;
            payload.targets[i].v     = frame.targets[i].velocity;
        }
        mesh_send_radar_payload(my_mac(), get_battery_mv(), &payload);
        */

        ESP_LOGI(TAG, "sensor_task: tick");
        vTaskDelay(pdMS_TO_TICKS(1000));
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

        /* LED indication */
        /* TODO: battery level, mesh role, error status */
        gpio_set_level(LED_GREEN, (bat > 3200) ? 0 : 1);
        gpio_set_level(LED_RED,   (bat < 3000) ? 0 : 1);

        ESP_LOGI(TAG, "Health: bat=%dmV", bat);

        /* Critical battery → deep sleep */
        if (bat < 3000) {
            ESP_LOGW(TAG, "Battery critical — entering deep sleep 10min");
            gpio_set_level(LED_RED, 0);
            vTaskDelay(pdMS_TO_TICKS(1000));
            esp_deep_sleep(600 * 1000000ULL);
        }

        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

/* ── Master task (Core 0, conditionally started) ── */

static void master_task(void *arg) {
    ESP_LOGI(TAG, "Master task started (waiting for role)");

    for (;;) {
        /* TODO: Only run if mesh_get_role() == ROLE_MASTER */
        /*
        if (mesh_get_role() != ROLE_MASTER) {
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        }
        */

        /* TODO: collect radar payloads from all slaves, spatial fusion */

        /* TODO: publish heatmap JSON via MQTT */
        ESP_LOGI(TAG, "master_task: tick");

        vTaskDelay(pdMS_TO_TICKS(5000));
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

/* ── Timer callbacks ── */

static void sensor_wake_callback(void *arg) {
    xSemaphoreGive(sensor_sem);
}

/* ── App main ── */

void app_main(void) {
    ESP_LOGI(TAG, "NOISYLESS HeatMap — Node Radar 60GHz v0.1.0");
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
    esp_netif_init();
    esp_event_loop_create_default();

    /* TODO: config_init(); config_load_*(); */

    /* Init mutexes & semaphores */
    sensor_sem = xSemaphoreCreateBinary();

    /* TODO: mesh_init(); mesh_set_master_callback(master_cb); */

    /* Start tasks */
    xTaskCreatePinnedToCore(sensor_task, "sensor", 4096, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(health_task, "health", 2048, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(master_task, "master", 8192, NULL, 2, NULL, 0);

    /* Sensor timer: wake sensor_task every 1000ms */
    esp_timer_handle_t sensor_timer;
    const esp_timer_create_args_t st_args = {
        .callback = sensor_wake_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "sensor_timer",
    };
    esp_timer_create(&st_args, &sensor_timer);
    esp_timer_start_periodic(sensor_timer, 1000 * 1000); /* 1s */

    ESP_LOGI(TAG, "App main running");
    gpio_set_level(LED_GREEN, 1); /* Boot complete */

    /* Idle loop */
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
