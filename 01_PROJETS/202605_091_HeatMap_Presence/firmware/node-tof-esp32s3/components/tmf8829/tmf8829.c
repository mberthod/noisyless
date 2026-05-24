/*
 * TMF8829 dToF Sensor Driver — I²C
 * Based on ams-OSRAM DS001140 rev 2-00
 *
 * Register map:
 *   0xE0  ENABLE        Write 0x01 to power on
 *   0x00  CMD_STAT      Write 0x80 = START. Read = status bits
 *   0x01  APP0          Bit 0 = READY. Read = application result pending.
 *   0x14  MODE          0=8x8, 1=16x16
 *   0x15  ITERATION     Number of iterations per ranging
 *   0x20  RESULT_NUM    Number of measurement results available
 *   0x24  RESULT        First byte of measurement result buffer
 */

#include "tmf8829.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "TMF8829"
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_SCL  GPIO_NUM_9
#define I2C_MASTER_SDA  GPIO_NUM_8
#define I2C_MASTER_FREQ  400000

static bool initialized = false;

/* ── register helpers ── */

static esp_err_t reg_write(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_write_to_device(I2C_MASTER_NUM, TMF8829_I2C_ADDR,
                                      buf, 2, pdMS_TO_TICKS(TMF8829_I2C_TIMEOUT_MS));
}

static esp_err_t reg_read(uint8_t reg, uint8_t *val) {
    return i2c_master_write_read_device(I2C_MASTER_NUM, TMF8829_I2C_ADDR,
                                        &reg, 1, val, 1,
                                        pdMS_TO_TICKS(TMF8829_I2C_TIMEOUT_MS));
}

static esp_err_t reg_read_multi(uint8_t reg, uint8_t *buf, size_t len) {
    return i2c_master_write_read_device(I2C_MASTER_NUM, TMF8829_I2C_ADDR,
                                        &reg, 1, buf, len,
                                        pdMS_TO_TICKS(200));
}

/* ── public API ── */

esp_err_t tmf8829_init(void) {
    if (initialized) return ESP_OK;

    ESP_LOGI(TAG, "Initializing I²C...");
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA,
        .scl_io_num = I2C_MASTER_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ,
        .clk_flags = 0,
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_MASTER_NUM, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_MASTER_NUM, I2C_MODE_MASTER,
                                        0, 0, 0));

    /* Power on sensor */
    vTaskDelay(pdMS_TO_TICKS(10));
    ESP_ERROR_CHECK(reg_write(0xE0, 0x01));
    vTaskDelay(pdMS_TO_TICKS(5));

    /* CPU reset */
    ESP_ERROR_CHECK(reg_write(0x00, 0x40));
    vTaskDelay(pdMS_TO_TICKS(2));

    /* Check boot: APP0 should read 0 */
    uint8_t app0_status;
    esp_err_t err = reg_read(0x01, &app0_status);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TMF8829 not responding on I²C 0x29. Check wiring.");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "TMF8829 detected, APP0=0x%02X", app0_status);

    /* Configure: 16x16 mode, 500 iterations */
    ESP_ERROR_CHECK(reg_write(0x14, TMF8829_MODE_16x16));
    ESP_ERROR_CHECK(reg_write(0x15, 244));  // low byte iterations
    ESP_ERROR_CHECK(reg_write(0x16, 1));    // high byte iterations (256 + 244 = 500)

    initialized = true;
    ESP_LOGI(TAG, "TMF8829 initialized (16x16, 500 iter)");
    return ESP_OK;
}

esp_err_t tmf8829_start_ranging(void) {
    if (!initialized) return ESP_ERR_INVALID_STATE;

    /* Write START command: 0x80 to CMD_STAT */
    return reg_write(0x00, 0x80);
}

esp_err_t tmf8829_read_result(tmf8829_result_t *result) {
    if (!initialized || !result) return ESP_ERR_INVALID_STATE;

    /* 1. Wait for APP0 bit 0 (READY) */
    TickType_t start = xTaskGetTickCount();
    uint8_t app0;
    do {
        esp_err_t err = reg_read(0x01, &app0);
        if (err != ESP_OK) return err;
        if (!(app0 & 0x01)) {   /* bit 0 = 0 → still ranging, wait */
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    } while (!(app0 & 0x01) &&
             ((xTaskGetTickCount() - start) < pdMS_TO_TICKS(TMF8829_READY_TIMEOUT_MS)));

    if (!(app0 & 0x01)) {
        ESP_LOGW(TAG, "TMF8829 ranging timeout (APP0=0x%02X)", app0);
        return ESP_ERR_TIMEOUT;
    }

    /* 2. Read RESULT_NUM → how many zones returned */
    uint8_t result_num;
    ESP_ERROR_CHECK(reg_read(0x20, &result_num));
    if (result_num == 0) {
        result->valid_zones = 0;
        return ESP_OK;
    }

    /* 3. Read result buffer. Each zone = 2 bytes: distance_mm (uint16)
     *    + confidence in low nibble of byte 1. Total: min(256, result_num) zones. */
    uint8_t raw_buf[512];  /* max 256 zones × 2 bytes */
    size_t to_read = (result_num > 256) ? 512 : (size_t)result_num * 2;
    ESP_ERROR_CHECK(reg_read_multi(0x24, raw_buf, to_read));

    /* 4. Decode */
    uint8_t valid = 0;
    for (int i = 0; i < (to_read >> 1); i++) {
        uint16_t dist = raw_buf[i * 2] | ((uint16_t)raw_buf[i * 2 + 1] << 8);
        uint8_t conf = raw_buf[i * 2 + 1] & 0x0F;

        /* Filter: ignore 0 (no target) and >5m (datasheet range) */
        if (dist > 0 && dist <= 5000) {
            result->distances_mm[valid] = dist;
            result->confidence[valid] = conf;
            valid++;
        }
    }

    result->valid_zones = valid;
    result->timestamp_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    return ESP_OK;
}

esp_err_t tmf8829_standby(void) {
    return reg_write(0xE0, 0x02);  /* EN bit 1 = standby */
}

esp_err_t tmf8829_wake(void) {
    esp_err_t err = reg_write(0xE0, 0x01);  /* EN bit 0 = active */
    vTaskDelay(pdMS_TO_TICKS(2));
    return err;
}

esp_err_t tmf8829_power_down(void) {
    return reg_write(0xE0, 0x00);  /* full power down */
}
