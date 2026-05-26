/**
 * @file ms72sf1.c
 * @brief Driver for MinewSemi MS72SF1 60GHz mmWave radar sensor.
 * @details UART 115200 bauds, binary TLV protocol.
 * @author Hermes Agent / ELEC-CORE
 * @date 2026-05-24
 */

#include "ms72sf1.h"
#include <string.h>
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "RADAR-MS72SF1"

/* ── Protocol constants ── */
#define FRAME_HEADER_SIZE 8
#define FRAME_LENGTH_SIZE 4
#define FRAME_NUM_SIZE    4
#define TLV_HEADER_SIZE   2
#define MAX_FRAME_SIZE    512

static const uint8_t EXPECTED_HEADER[8] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};

static uart_port_t uart_port = UART_NUM_2;
static bool initialized = false;

/* ── UART helpers ── */

static esp_err_t uart_read_byte(uint8_t *b) {
    return uart_read_bytes(uart_port, b, 1, pdMS_TO_TICKS(50));
}

static esp_err_t uart_read_bytes_sync(uint8_t *buf, size_t len) {
    return uart_read_bytes(uart_port, buf, len, pdMS_TO_TICKS(200));
}

/* ── Frame parser ── */

static bool parse_targets(const uint8_t *data, uint16_t len, ms72sf1_frame_t *frame) {
    uint8_t count = len / 32;
    if (count > MS72SF1_MAX_TARGETS) {
        count = MS72SF1_MAX_TARGETS;
    }

    frame->target_count = count;

    for (int i = 0; i < count; i++) {
        const uint8_t *p = data + (i * 32);
        float *dest = (float*)&frame->targets[i];
        memcpy(&dest[0], p + 0,  4); /* Q */
        memcpy(&dest[1], p + 4,  4); /* ID */
        memcpy(&dest[2], p + 8,  4); /* X */
        memcpy(&dest[3], p + 12, 4); /* Y */
        memcpy(&dest[4], p + 16, 4); /* Z */
        memcpy(&dest[5], p + 20, 4); /* Vx */
        memcpy(&dest[6], p + 24, 4); /* Vy */
        memcpy(&dest[7], p + 28, 4); /* Vz */
    }

    return true;
}

/* ── Public API ── */

bool ms72sf1_init(void) {
    if (initialized) return true;

    uart_config_t uart_cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(uart_port, 1024, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(uart_port, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(uart_port, 17, 18, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    /* Wait 500ms for sensor boot */
    vTaskDelay(pdMS_TO_TICKS(500));

    /* Send AT init sequence */
    const char *init_cmds[] = {
        "AT+DEBUG=3\r\n",
        "AT+START\r\n",
    };

    for (int i = 0; i < 2; i++) {
        uart_write_bytes(uart_port, init_cmds[i], strlen(init_cmds[i]));
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    /* Flush any boot noise */
    uart_flush(uart_port);

    ESP_LOGI(TAG, "Initialized (UART %d, 115200 bauds)", uart_port);
    initialized = true;
    return true;
}

bool ms72sf1_read_frame(ms72sf1_frame_t *frame) {
    if (!initialized) return false;

    memset(frame, 0, sizeof(*frame));

    /* Read header */
    uint8_t header[FRAME_HEADER_SIZE];
    if (uart_read_bytes_sync(header, FRAME_HEADER_SIZE) != ESP_OK) {
        return false;
    }

    if (memcmp(header, EXPECTED_HEADER, FRAME_HEADER_SIZE) != 0) {
        /* Sync lost — flush and retry */
        uart_flush(uart_port);
        return false;
    }

    /* Read length + frame number */
    uint8_t len_frame[FRAME_LENGTH_SIZE + FRAME_NUM_SIZE];
    if (uart_read_bytes_sync(len_frame, sizeof(len_frame)) != ESP_OK) {
        return false;
    }

    uint32_t total_len = len_frame[0] | (len_frame[1] << 8) | (len_frame[2] << 16) | (len_frame[3] << 24);
    uint32_t frame_num = len_frame[4] | (len_frame[5] << 8) | (len_frame[6] << 16) | (len_frame[7] << 24);

    /* Compute TLV payload size */
    uint16_t tlv_size = total_len - FRAME_HEADER_SIZE - FRAME_LENGTH_SIZE - FRAME_NUM_SIZE;
    if (tlv_size == 0 || tlv_size > MAX_FRAME_SIZE) {
        return false;
    }

    /* Read TLV payload */
    uint8_t tlv_buf[MAX_FRAME_SIZE];
    if (uart_read_bytes_sync(tlv_buf, tlv_size) != ESP_OK) {
        return false;
    }

    /* Parse TLVs */
    uint16_t offset = 0;
    while (offset + TLV_HEADER_SIZE <= tlv_size) {
        uint8_t  tlv_type = tlv_buf[offset];
        uint16_t tlv_len  = tlv_buf[offset + 1];

        offset += TLV_HEADER_SIZE;

        if (tlv_type == 2 && tlv_len > 0) {
            /* Type 2 = tracks */
            uint16_t data_len = (tlv_len < (tlv_size - offset)) ? tlv_len : (tlv_size - offset);
            if (!parse_targets(&tlv_buf[offset], data_len, frame)) {
                ESP_LOGW(TAG, "Failed to parse %d bytes of track data", data_len);
            }
        }

        offset += tlv_len;
    }

    ESP_LOGV(TAG, "Frame #%ld: %d targets", frame_num, frame->target_count);
    return true;
}

void ms72sf1_deinit(void) {
    if (!initialized) return;

    uart_write_bytes(uart_port, "AT+STOP\r\n", 10);
    vTaskDelay(pdMS_TO_TICKS(100));
    uart_driver_delete(uart_port);
    ESP_LOGI(TAG, "Deinitialized");
    initialized = false;
}
