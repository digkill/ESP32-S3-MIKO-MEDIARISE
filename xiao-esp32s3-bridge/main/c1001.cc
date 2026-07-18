#include "c1001.h"
#include "config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char* TAG = "C1001";

/*
 * DFRobot SEN0623 / C1001 protocol (DFRobot_HumanDetection library v1.0):
 *
 * Send frame:
 *   [0x53][0x59][CON][CMD][LEN_H][LEN_L][DATA...][SUM][0x54][0x43]
 *   SUM = low 8 bits of sum of all preceding bytes
 *
 * begin(): getData(0x01, 0x83, 1, {0x0f}) after 10 s boot wait
 * presence: getData(0x80, 0x81, 1, {0x0f}) → data[0]
 * motion:   getData(0x80, 0x82, 1, {0x0f}) → data[0]
 * range:    getData(0x80, 0x83, 1, {0x0f}) → data[0] (0–100)
 */

#define FRAME_HEAD0  0x53
#define FRAME_HEAD1  0x59
#define FRAME_TAIL0  0x54
#define FRAME_TAIL1  0x43
#define TIME_OUT_MS  5000

#define RX_BUF_SIZE 256

static bool s_ok = false;

static uint8_t frame_sum(const uint8_t* buf, size_t len) {
    uint16_t sum = 0;
    for (size_t i = 0; i < len; i++) sum += buf[i];
    return (uint8_t)(sum & 0xff);
}


static int get_data(uint8_t con, uint8_t cmd, uint16_t len, const uint8_t* send_data,
                    uint8_t* ret_data, size_t ret_cap) {
    uint8_t cmd_buf[32];
    int cmd_len = 0;
    cmd_buf[cmd_len++] = FRAME_HEAD0;
    cmd_buf[cmd_len++] = FRAME_HEAD1;
    cmd_buf[cmd_len++] = con;
    cmd_buf[cmd_len++] = cmd;
    cmd_buf[cmd_len++] = (uint8_t)((len >> 8) & 0xff);
    cmd_buf[cmd_len++] = (uint8_t)(len & 0xff);
    for (uint16_t i = 0; i < len; i++) cmd_buf[cmd_len++] = send_data[i];
    cmd_buf[cmd_len] = frame_sum(cmd_buf, cmd_len);
    cmd_len++;
    cmd_buf[cmd_len++] = FRAME_TAIL0;
    cmd_buf[cmd_len++] = FRAME_TAIL1;

    enum {
        ST_WHITE = 0, ST_HEAD, ST_CONFIG, ST_CMD, ST_LEN_H, ST_LEN_L, ST_DATA, ST_SUM, ST_END_H, ST_END_L
    } state = ST_WHITE;

    int64_t deadline = esp_timer_get_time() + (int64_t)TIME_OUT_MS * 1000;
    int64_t resend_at = 0;
    uint16_t resp_len = 0;
    uint16_t count = 0;

    while (esp_timer_get_time() < deadline) {
        if (resend_at == 0 || esp_timer_get_time() >= resend_at) {
            uint8_t drain[32];
            while (uart_read_bytes(C1001_UART_NUM, drain, sizeof(drain), 0) > 0) {}
            uart_write_bytes(C1001_UART_NUM, cmd_buf, cmd_len);
            resend_at = esp_timer_get_time() + 1000 * 1000;
            count = 0;
            state = ST_WHITE;
        }

        uint8_t b;
        if (uart_read_bytes(C1001_UART_NUM, &b, 1, pdMS_TO_TICKS(50)) != 1) {
            continue;
        }

        switch (state) {
        case ST_WHITE:
            if (b == FRAME_HEAD0) {
                ret_data[0] = b;
                state = ST_HEAD;
            }
            break;
        case ST_HEAD:
            if (b == FRAME_HEAD1) {
                ret_data[1] = b;
                state = ST_CONFIG;
            } else {
                state = ST_WHITE;
            }
            break;
        case ST_CONFIG:
            if (b == con) {
                ret_data[2] = b;
                state = ST_CMD;
            } else {
                state = ST_WHITE;
            }
            break;
        case ST_CMD:
            if (b == cmd) {
                ret_data[3] = b;
                state = ST_LEN_H;
            } else {
                state = ST_WHITE;
            }
            break;
        case ST_LEN_H:
            if (b == ret_data[3]) {
                state = ST_WHITE;
            } else {
                ret_data[4] = b;
                resp_len = (uint16_t)b << 8;
                state = ST_LEN_L;
            }
            break;
        case ST_LEN_L:
            if (b == ret_data[4]) {
                state = ST_WHITE;
            } else {
                ret_data[5] = b;
                resp_len |= b;
                count = 0;
                state = ST_DATA;
            }
            break;
        case ST_DATA:
            if (count < resp_len) {
                if ((6 + count) < ret_cap) ret_data[6 + count] = b;
                count++;
            } else if (b == frame_sum(ret_data, 6 + resp_len)) {
                ret_data[6 + resp_len] = b;
                state = ST_END_H;
            } else {
                state = ST_WHITE;
            }
            break;
        case ST_END_H:
            ret_data[7 + resp_len] = b;
            state = ST_END_L;
            break;
        case ST_END_L:
            ret_data[8 + resp_len] = b;
            return 0;
        default:
            state = ST_WHITE;
            break;
        }
    }

    return 2;
}

static bool c1001_begin(void) {
    // DFRobot library waits 10 s inside begin() for sensor self-test.
    vTaskDelay(pdMS_TO_TICKS(10000));
    uint8_t data = 0x0f;
    uint8_t buf[16];
    return get_data(0x01, 0x83, 1, &data, buf, sizeof(buf)) == 0;
}

static bool c1001_config_falling_mode(void) {
    const uint8_t mode = 0x01; // eFallingMode
    uint8_t data = 0x0f;
    uint8_t buf[16];

    if (get_data(0x02, 0xA8, 1, &data, buf, sizeof(buf)) != 0) {
        return false;
    }
    if (buf[6] == mode) {
        return true;
    }

    uint8_t cmd[] = {0x53, 0x59, 0x02, 0x08, 0x00, 0x01, mode, 0, 0x54, 0x43};
    cmd[7] = frame_sum(cmd, 7);
    uart_flush_input(C1001_UART_NUM);
    uart_write_bytes(C1001_UART_NUM, cmd, sizeof(cmd));
    vTaskDelay(pdMS_TO_TICKS(10000));

    if (get_data(0x02, 0xA8, 1, &data, buf, sizeof(buf)) != 0) {
        return false;
    }
    return buf[6] == mode;
}

static bool c1001_config_led(uint8_t led_cmd, uint8_t on) {
    uint8_t buf[16];
    return get_data(0x01, led_cmd, 1, &on, buf, sizeof(buf)) == 0;
}

static bool c1001_sensor_restart(void) {
    uint8_t data = 0x0f;
    uint8_t buf[16];
    if (get_data(0x01, 0x02, 1, &data, buf, sizeof(buf)) != 0) {
        return false;
    }
    vTaskDelay(pdMS_TO_TICKS(10000));
    return true;
}

static bool c1001_set_unmanned_time(uint32_t sec) {
    uint8_t buf[16];
    uint8_t payload[4] = {
        (uint8_t)((sec >> 24) & 0xff),
        (uint8_t)((sec >> 16) & 0xff),
        (uint8_t)((sec >> 8) & 0xff),
        (uint8_t)(sec & 0xff),
    };
    return get_data(0x80, 0x12, 4, payload, buf, sizeof(buf)) == 0;
}

#if !CONFIG_IDF_TARGET_ESP32
#include "driver/usb_serial_jtag.h"
#include "hal/usb_serial_jtag_ll.h"
#include "esp_private/esp_gpio_reserve.h"
#endif

static bool c1001_uses_usb_jtag_pins(int tx_gpio, int rx_gpio) {
    return tx_gpio == 43 || tx_gpio == 44 || rx_gpio == 43 || rx_gpio == 44;
}

static void c1001_release_usb_jtag_pins(void) {
#if !CONFIG_IDF_TARGET_ESP32
    if (!c1001_uses_usb_jtag_pins(C1001_TX_GPIO, C1001_RX_GPIO)) {
        return;
    }
    ESP_LOGW(TAG, "Releasing D6/D7 (GPIO43/44) from USB Serial/JTAG for C1001 UART");
    usb_serial_jtag_driver_uninstall();
    usb_serial_jtag_ll_phy_enable_pad(false);
    esp_gpio_revoke(BIT64(43) | BIT64(44));
#endif
}

bool c1001_init(void) {
    c1001_release_usb_jtag_pins();

    uart_config_t cfg = {};
    cfg.baud_rate  = C1001_UART_BAUD;
    cfg.data_bits  = UART_DATA_8_BITS;
    cfg.parity     = UART_PARITY_DISABLE;
    cfg.stop_bits  = UART_STOP_BITS_1;
    cfg.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
    cfg.source_clk = UART_SCLK_DEFAULT;

    if (uart_driver_install(C1001_UART_NUM, RX_BUF_SIZE * 2, 0, 0, nullptr, 0) != ESP_OK ||
        uart_param_config(C1001_UART_NUM, &cfg) != ESP_OK ||
        uart_set_pin(C1001_UART_NUM, C1001_TX_GPIO, C1001_RX_GPIO,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(TAG, "UART init failed");
        return false;
    }

    ESP_LOGI(TAG, "waiting 3 s for sensor boot...");
    vTaskDelay(pdMS_TO_TICKS(3000));

    bool alive = false;
    for (int attempt = 1; attempt <= 5 && !alive; attempt++) {
        ESP_LOGI(TAG, "begin() attempt %d (library waits up to 10 s)...", attempt);
        if (c1001_begin()) {
            ESP_LOGI(TAG, "UART communication OK (attempt %d)", attempt);
            alive = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (!alive) {
        // Try swapped TX/RX once — common wiring mistake.
        ESP_LOGW(TAG, "no reply on TX=GPIO%d RX=GPIO%d — trying swapped pins",
                 C1001_TX_GPIO, C1001_RX_GPIO);
        uart_set_pin(C1001_UART_NUM, C1001_RX_GPIO, C1001_TX_GPIO,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        for (int attempt = 1; attempt <= 3 && !alive; attempt++) {
            if (c1001_begin()) {
                ESP_LOGI(TAG, "UART OK with swapped pins TX=GPIO%d RX=GPIO%d",
                         C1001_RX_GPIO, C1001_TX_GPIO);
                alive = true;
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    if (!alive) {
        ESP_LOGE(TAG, "init failed — check 3V3/5V, GND, crossed UART: sensor RX<-D6, sensor TX->D7");
        return false;
    }

    ESP_LOGI(TAG, "configuring falling mode...");
    if (!c1001_config_falling_mode()) {
        ESP_LOGW(TAG, "falling mode setup timed out; continuing with presence queries");
    } else {
        if (!c1001_config_led(0x03, 1)) {
            ESP_LOGW(TAG, "HP LED config skipped");
        }
        if (!c1001_config_led(0x04, 0)) {
            ESP_LOGW(TAG, "FALL LED config skipped");
        }
        c1001_set_unmanned_time(1);
        ESP_LOGI(TAG, "restarting sensor (~10 s)...");
        if (!c1001_sensor_restart()) {
            ESP_LOGW(TAG, "sensor restart timed out; continuing");
        }
    }

    s_ok = true;
    ESP_LOGI(TAG, "OK TX=GPIO%d RX=GPIO%d", C1001_TX_GPIO, C1001_RX_GPIO);
    return true;
}

bool c1001_is_ok(void) { return s_ok; }

bool c1001_query(uint8_t* presence, uint8_t* motion, uint16_t* range_cm) {
    if (!s_ok) return false;

    uint8_t data = 0x0f;
    uint8_t buf[16];
    bool ok = true;

    if (presence) {
        if (get_data(0x80, 0x81, 1, &data, buf, sizeof(buf)) == 0) {
            *presence = buf[6];
        } else {
            ok = false;
        }
    }
    if (motion) {
        if (get_data(0x80, 0x82, 1, &data, buf, sizeof(buf)) == 0) {
            *motion = buf[6];
        } else {
            ok = false;
        }
    }
    if (range_cm) {
        if (get_data(0x80, 0x83, 1, &data, buf, sizeof(buf)) == 0) {
            *range_cm = buf[6];
        } else {
            ok = false;
        }
    }
    return ok;
}
