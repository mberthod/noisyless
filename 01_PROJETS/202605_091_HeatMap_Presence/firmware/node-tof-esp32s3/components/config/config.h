#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* NVS namespaces */
#define NVS_IDENTITY    "identity"
#define NVS_CALIB       "calib"
#define NVS_SENSOR      "sensor"
#define NVS_POWER       "power"
#define NVS_MESH        "mesh"

typedef struct {
    uint8_t  role;           /* 0=auto, 1=slave, 2=master */
    char     device_id[20];
} identity_config_t;

typedef struct {
    float    x_m;            /* node position in world */
    float    y_m;
    float    theta_deg;      /* orientation */
    float    ceiling_height_m;
} calib_config_t;

typedef struct {
    uint32_t period_ms;      /* sensor reading interval */
    uint32_t duty_cycle_s;   /* deep sleep duration */
    uint8_t  mode;           /* 0=8x8, 1=16x16 */
} sensor_config_t;

typedef struct {
    uint32_t battery_low_mv;
} power_config_t;

esp_err_t config_init(void);
esp_err_t config_load_identity(identity_config_t *cfg);
esp_err_t config_save_identity(const identity_config_t *cfg);
esp_err_t config_load_calib(calib_config_t *cfg);
esp_err_t config_save_calib(const calib_config_t *cfg);
esp_err_t config_load_sensor(sensor_config_t *cfg);
esp_err_t config_load_power(power_config_t *cfg);
