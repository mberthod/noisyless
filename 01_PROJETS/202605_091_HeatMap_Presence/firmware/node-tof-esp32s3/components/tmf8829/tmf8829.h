#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define TMF8829_I2C_ADDR        0x29
#define TMF8829_I2C_TIMEOUT_MS  100
#define TMF8829_READY_TIMEOUT_MS 67
#define TMF8829_MODE_16x16      1
#define TMF8829_ITERATIONS       500
#define TMF8829_ZONE_COUNT      256

typedef struct {
    uint16_t distances_mm[256];
    uint8_t  confidence[256];
    uint32_t timestamp_ms;
    uint8_t  valid_zones;
} tmf8829_result_t;

esp_err_t tmf8829_init(void);
esp_err_t tmf8829_start_ranging(void);
esp_err_t tmf8829_read_result(tmf8829_result_t *result);
esp_err_t tmf8829_standby(void);
esp_err_t tmf8829_wake(void);
esp_err_t tmf8829_power_down(void);
bool tmf8829_is_busy(void);
