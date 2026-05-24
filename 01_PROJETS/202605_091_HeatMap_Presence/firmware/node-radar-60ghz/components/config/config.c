/**
 * @file config.c
 * @brief Configuration management via NVS for node-radar-60ghz.
 * @author Hermes Agent / ELEC-CORE
 * @date 2026-05-24
 */

#include "config.h"
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"

#define TAG "CONFIG"

static nvs_handle_t identity_handle;
static nvs_handle_t calib_handle;
static nvs_handle_t sensor_handle;
static nvs_handle_t power_handle;
static nvs_handle_t mesh_handle;

esp_err_t config_init(void) {
    esp_err_t err;

    err = nvs_open(NVS_IDENTITY, NVS_READWRITE, &identity_handle);
    if (err != ESP_OK) { ESP_LOGE(TAG, "nvs_open identity: %s", esp_err_to_name(err)); return err; }

    err = nvs_open(NVS_CALIB, NVS_READWRITE, &calib_handle);
    if (err != ESP_OK) { ESP_LOGE(TAG, "nvs_open calib: %s", esp_err_to_name(err)); return err; }

    err = nvs_open(NVS_SENSOR, NVS_READWRITE, &sensor_handle);
    if (err != ESP_OK) { ESP_LOGE(TAG, "nvs_open sensor: %s", esp_err_to_name(err)); return err; }

    err = nvs_open(NVS_POWER, NVS_READWRITE, &power_handle);
    if (err != ESP_OK) { ESP_LOGE(TAG, "nvs_open power: %s", esp_err_to_name(err)); return err; }

    err = nvs_open(NVS_MESH, NVS_READWRITE, &mesh_handle);
    if (err != ESP_OK) { ESP_LOGE(TAG, "nvs_open mesh: %s", esp_err_to_name(err)); return err; }

    ESP_LOGI(TAG, "Configuration system initialized");
    return ESP_OK;
}

esp_err_t config_load_identity(identity_config_t *cfg) {
    size_t size = sizeof(cfg->device_id);
    nvs_get_u8(identity_handle, "role", &cfg->role);
    nvs_get_str(identity_handle, "devid", cfg->device_id, &size);
    return ESP_OK;
}

esp_err_t config_save_identity(const identity_config_t *cfg) {
    nvs_set_u8(identity_handle, "role", cfg->role);
    nvs_set_str(identity_handle, "devid", cfg->device_id);
    nvs_commit(identity_handle);
    return ESP_OK;
}

esp_err_t config_load_calib(calib_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    nvs_get_i32(calib_handle, "x_mm",  (int32_t*)&cfg->x_m);   /* stored as mm*1000 int */
    nvs_get_i32(calib_handle, "y_mm",  (int32_t*)&cfg->y_m);
    nvs_get_i32(calib_handle, "z_mm",  (int32_t*)&cfg->z_m);
    nvs_get_i32(calib_handle, "theta", (int32_t*)&cfg->theta_deg);
    return ESP_OK;
}

esp_err_t config_save_calib(const calib_config_t *cfg) {
    nvs_set_i32(calib_handle, "x_mm",  (int32_t)cfg->x_m);
    nvs_set_i32(calib_handle, "y_mm",  (int32_t)cfg->y_m);
    nvs_set_i32(calib_handle, "z_mm",  (int32_t)cfg->z_m);
    nvs_set_i32(calib_handle, "theta", (int32_t)cfg->theta_deg);
    nvs_commit(calib_handle);
    return ESP_OK;
}

esp_err_t config_load_sensor(sensor_config_t *cfg) {
    cfg->period_ms = 1000;
    cfg->mode      = 1;
    nvs_get_u32(sensor_handle, "period_ms", &cfg->period_ms);
    nvs_get_u8(sensor_handle,  "mode",      &cfg->mode);
    return ESP_OK;
}

esp_err_t config_load_power(power_config_t *cfg) {
    cfg->battery_low_mv = 3000;
    nvs_get_u32(power_handle, "bat_low_mv", &cfg->battery_low_mv);
    return ESP_OK;
}
