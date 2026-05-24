/*
 * power.h — Battery management, deep sleep, watchdog for node ToF.
 *
 * Strategy:
 *   - Normal cycle: wake from RTC → measure → mesh TX → deep sleep
 *   - Battery < low_threshold: deep sleep 10 min between cycles
 *   - Battery < critical: shutdown (RTC-only, manual wake needed)
 *   - HW watchdog: 10s timeout on any task
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* ── Battery ── */
uint16_t power_get_battery_mv(void);
bool     power_is_battery_low(void);
bool     power_is_battery_critical(void);

/* ── Deep sleep ── */
void power_deep_sleep_seconds(uint32_t seconds);
void power_deep_sleep_minutes(uint32_t minutes);
void power_shutdown(void);          /* RTC only — needs physical wake */

/* ── Wake reason ── */
typedef enum {
    WAKE_UNKNOWN = 0,
    WAKE_TIMER,       /* RTC timer expired */
    WAKE_GPIO,        /* External pin */
    WAKE_BOOT,        /* Cold boot or reset */
} wake_reason_t;

wake_reason_t power_get_wake_reason(void);

/* ── Watchdog ── */
esp_err_t power_watchdog_init(uint32_t timeout_seconds);
void      power_watchdog_feed(void);
