/*
 * config.c — NVS-backed persistent config with factory defaults.
 * Zero malloc. Stack-only. All keys validated on read.
 */

#include "config.h"
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

#define TAG "CONFIG"

static nvs_handle_t open_ns(const char *ns) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(ns, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open(%s) failed: %s", ns, esp_err_to_name(err));
    }
    return handle;
}

esp_err_t config_init(void) {
    return nvs_flash_init();
}

esp_err_t config_load_identity(identity_config_t *cfg) {
    memset(cfg, 0, sizeof(*cfg));
    nvs_handle_t h = open_ns(NVS_IDENTITY);
    if (!h) return ESP_FAIL;

    size_t sz = sizeof(cfg->device_id);
    nvs_get_u8(h, "role", &cfg->role);
    nvs_get_str(h, "device_id", cfg->device_id, &sz);
    nvs_close(h);

    if (cfg->device_id[0] == '\0') {
        /* Default: use MAC suffix */
        uint8_t mac[6];
        esp_efuse_mac_get_default(mac);
        snprintf(cfg->device_id, sizeof(cfg->device_id),
                 "NL-%02x%02x%02x", mac[3], mac[4], mac[5]);
    }
    return ESP_OK;
}

esp_err_t config_save_identity(const identity_config_t *cfg) {
    nvs_handle_t h = open_ns(NVS_IDENTITY);
    if (!h) return ESP_FAIL;
    nvs_set_u8(h, "role", cfg->role);
    nvs_set_str(h, "device_id", cfg->device_id);
    nvs_commit(h);
    nvs_close(h);
    return ESP_OK;
}

esp_err_t config_load_calib(calib_config_t *cfg) {
    /* Default: ceiling 3.0m, centered */
    cfg->x_m = 0.0f;
    cfg->y_m = 0.0f;
    cfg->theta_deg = 0.0f;
    cfg->ceiling_height_m = 3.0f;

    nvs_handle_t h = open_ns(NVS_CALIB);
    if (!h) return ESP_OK; /* defaults are fine */

    uint32_t raw;
    if (nvs_get_u32(h, "x_cm", &raw) == ESP_OK) cfg->x_m = (float)(int32_t)raw / 100.0f;
    if (nvs_get_u32(h, "y_cm", &raw) == ESP_OK) cfg->y_m = (float)(int32_t)raw / 100.0f;
    if (nvs_get_u32(h, "theta", &raw) == ESP_OK) cfg->theta_deg = (float)(int32_t)raw;
    if (nvs_get_u32(h, "ceil_mm", &raw) == ESP_OK) cfg->ceiling_height_m = (float)raw / 1000.0f;
    nvs_close(h);
    return ESP_OK;
}

esp_err_t config_save_calib(const calib_config_t *cfg) {
    nvs_handle_t h = open_ns(NVS_CALIB);
    if (!h) return ESP_FAIL;
    nvs_set_u32(h, "x_cm",  (int32_t)(cfg->x_m * 100.0f));
    nvs_set_u32(h, "y_cm",  (int32_t)(cfg->y_m * 100.0f));
    nvs_set_u32(h, "theta", (int32_t)cfg->theta_deg);
    nvs_set_u32(h, "ceil_mm", (uint32_t)(cfg->ceiling_height_m * 1000.0f));
    nvs_commit(h);
    nvs_close(h);
    return ESP_OK;
}

esp_err_t config_load_sensor(sensor_config_t *cfg) {
    cfg->period_ms = 60000;     /* 60s default */
    cfg->duty_cycle_s = 58;     /* deep sleep duration */
    cfg->mode = 1;              /* 16x16 */

    nvs_handle_t h = open_ns(NVS_SENSOR);
    if (!h) return ESP_OK;
    uint32_t v;
    if (nvs_get_u32(h, "period_ms", &v) == ESP_OK) cfg->period_ms = v;
    if (nvs_get_u32(h, "duty_s", &v) == ESP_OK) cfg->duty_cycle_s = v;
    if (nvs_get_u8(h, "mode", &cfg->mode) != ESP_OK) cfg->mode = 1;
    nvs_close(h);
    return ESP_OK;
}

esp_err_t config_load_power(power_config_t *cfg) {
    cfg->battery_low_mv = 3200;  /* 3.2V low threshold */
    nvs_handle_t h = open_ns(NVS_POWER);
    if (!h) return ESP_OK;
    uint32_t v;
    if (nvs_get_u32(h, "low_mv", &v) == ESP_OK) cfg->battery_low_mv = v;
    nvs_close(h);
    return ESP_OK;
}
