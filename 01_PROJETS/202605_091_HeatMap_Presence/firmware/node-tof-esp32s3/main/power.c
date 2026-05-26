/*
 * power.c — Power management implementation.
 * Target: 1×18650 Li-Ion (3000 mAh), ~380 days autonomy at 60s intervals.
 */

#include "power.h"
#include "config.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "driver/adc.h"
#include "driver/rtc_io.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_task_wdt.h"

#define TAG "POWER"
#define BAT_ADC          ADC1_CHANNEL_0  /* GPIO1 */
#define BAT_DIVIDER_RATIO 2.0f           /* 1:2 voltage divider */
#define CRITICAL_MV      3000
#define LOW_MV           3200

static uint16_t last_battery_mv = 3700;

uint16_t power_get_battery_mv(void) {
    int raw = adc1_get_raw(BAT_ADC);
    float voltage = (float)raw * 3.3f / 4095.0f;
    last_battery_mv = (uint16_t)(voltage * BAT_DIVIDER_RATIO * 1000.0f);
    return last_battery_mv;
}

bool power_is_battery_low(void) {
    return power_get_battery_mv() < LOW_MV;
}

bool power_is_battery_critical(void) {
    return power_get_battery_mv() < CRITICAL_MV;
}

void power_deep_sleep_seconds(uint32_t seconds) {
    ESP_LOGI(TAG, "Entering deep sleep for %lu s", seconds);
    esp_sleep_enable_timer_wakeup(seconds * 1000000ULL);
    esp_deep_sleep_start();
    /* Never returns */
}

void power_deep_sleep_minutes(uint32_t minutes) {
    power_deep_sleep_seconds(minutes * 60);
}

void power_shutdown(void) {
    ESP_LOGW(TAG, "CRITICAL SHUTDOWN — battery below %dmV", CRITICAL_MV);
    /* No timer — only wake on button or power cycle */
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    esp_deep_sleep_start();
}

wake_reason_t power_get_wake_reason(void) {
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    switch (cause) {
    case ESP_SLEEP_WAKEUP_TIMER: return WAKE_TIMER;
    case ESP_SLEEP_WAKEUP_GPIO:  return WAKE_GPIO;
    default:                     return (cause == ESP_SLEEP_WAKEUP_UNDEFINED) ? WAKE_BOOT : WAKE_UNKNOWN;
    }
}

esp_err_t power_watchdog_init(uint32_t timeout_seconds) {
    esp_task_wdt_config_t cfg = {
        .timeout_ms = timeout_seconds * 1000,
        .idle_core_mask = 0,
        .trigger_panic = true,
    };
    esp_err_t err = esp_task_wdt_init(&cfg);
    if (err == ESP_OK) {
        esp_task_wdt_add(NULL);  /* add current task (main) */
        ESP_LOGI(TAG, "Watchdog initialized (%lu s)", timeout_seconds);
    }
    return err;
}

void power_watchdog_feed(void) {
    esp_task_wdt_reset();
}
