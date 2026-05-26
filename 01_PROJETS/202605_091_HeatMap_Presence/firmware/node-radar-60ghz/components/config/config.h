/**
 * @file config.h
 * @brief Configuration management via NVS for node-radar-60ghz.
 * @author Hermes Agent / ELEC-CORE
 * @date 2026-05-24
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* NVS namespaces */
#define NVS_IDENTITY "identity"
#define NVS_CALIB    "calib"
#define NVS_SENSOR   "sensor"
#define NVS_POWER    "power"
#define NVS_MESH     "mesh"

typedef struct {
    uint8_t  role;           /* 0=auto, 1=slave, 2=master */
    char     device_id[20];
} identity_config_t;

typedef struct {
    float    x_m;
    float    y_m;
    float    z_m;            /* Mounting height */
    float    theta_deg;
} calib_config_t;

typedef struct {
    uint32_t period_ms;
    uint8_t  mode;           /* 0=presence, 1=tracking */
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

#endif /* CONFIG_H */
