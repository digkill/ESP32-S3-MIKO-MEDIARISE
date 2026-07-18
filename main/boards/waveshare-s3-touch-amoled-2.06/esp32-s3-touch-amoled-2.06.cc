#include "wifi_board.h"
#include "display/lcd_display.h"
#include "esp_lcd_sh8601.h"

#include "codecs/box_audio_codec.h"
#include "application.h"
#include "button.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "config.h"
#include "assets/lang_config.h"
#include "power_save_timer.h"
#include "axp2101.h"
#include "i2c_device.h"
#include "servo_controller.h"
#include "homebot_ble_service.h"
#include "cat_display.h"
#include "startup_media.h"
#include <wifi_station.h>
#include <qmi8658.h>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_system.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_touch.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include <driver/sdmmc_host.h>
#include <esp_vfs_fat.h>
#include <esp_lvgl_port.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sdmmc_cmd.h>
#include "settings.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <vector>
#include <mutex>
#include <cstring>

#define TAG "WaveshareAMOLED2_06"

#define FT3168_I2C_ADDRESS       0x38
#define FT3168_REG_FINGER_NUM    0x02
#define FT3168_REG_X1_POSH       0x03
#define FT3168_REG_X1_POSL       0x04
#define FT3168_REG_Y1_POSH       0x05
#define FT3168_REG_Y1_POSL       0x06
#define FT3168_REG_DEVICE_ID     0xA0
#define FT3168_REG_POWER_MODE    0xA5
#define FT3168_POWER_MONITOR     0x01
#define FT3168_MAX_POINTS        5

// ── AXP2101 ──────────────────────────────────────────────────────────────────

class Pmic : public Axp2101 {
public:
    Pmic(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : Axp2101(i2c_bus, addr) {
        WriteReg(0x22, 0b110);
        WriteReg(0x27, 0x10);
        WriteReg(0x80, 0x01);
        WriteReg(0x90, 0x00);
        WriteReg(0x91, 0x00);
        WriteReg(0x82, (3300-1500)/100);
        WriteReg(0x92, (3300-500)/100);
        WriteReg(0x93, (3300-500)/100);
        WriteReg(0x90, 0x03);
        WriteReg(0x64, 0x02);
        WriteReg(0x61, 0x02);
        WriteReg(0x62, 0x0A);
        WriteReg(0x63, 0x01);
    }

    esp_err_t GetBatteryStatus(int& level, bool& charging, bool& discharging) {
        uint8_t status = 0;
        esp_err_t ret = ReadByte(0x01, status);
        if (ret != ESP_OK) {
            return ret;
        }

        uint8_t capacity = 0;
        ret = ReadByte(0xA4, capacity);
        if (ret != ESP_OK) {
            return ret;
        }

        const int current_direction = (status & 0b01100000) >> 5;
        charging = current_direction == 1;
        discharging = current_direction == 2;
        level = capacity;
        return ESP_OK;
    }

private:
    esp_err_t ReadByte(uint8_t reg, uint8_t& value) {
        return i2c_master_transmit_receive(i2c_device_, &reg, 1, &value, 1, 100);
    }
};

// ── SH8601 init sequence ──────────────────────────────────────────────────────

#define LCD_OPCODE_WRITE_CMD (0x02ULL)

static const sh8601_lcd_init_cmd_t vendor_specific_init[] = {
    {0x11, (uint8_t []){0x00}, 0, 120},
    {0xC4, (uint8_t []){0x80}, 1, 0},
    {0x44, (uint8_t []){0x01, 0xD1}, 2, 0},
    {0x35, (uint8_t []){0x00}, 1, 0},
    {0x53, (uint8_t []){0x20}, 1, 10},
    {0x63, (uint8_t []){0xFF}, 1, 10},
    {0x51, (uint8_t []){0x00}, 1, 10},
    {0x2A, (uint8_t []){0x00,0x16,0x01,0xAF}, 4, 0},
    {0x2B, (uint8_t []){0x00,0x00,0x01,0xF5}, 4, 0},
    {0x29, (uint8_t []){0x00}, 0, 10},
    {0x51, (uint8_t []){0xFF}, 1, 0},
};

// ── CustomLcdDisplay — pads status bar for rounded corners ───────────────────

class CustomLcdDisplay : public SpiLcdDisplay {
public:
    static void rounder_event_cb(lv_event_t* e) {
        lv_area_t* area = (lv_area_t*)lv_event_get_param(e);
        area->x1 = (area->x1 >> 1) << 1;
        area->y1 = (area->y1 >> 1) << 1;
        area->x2 = ((area->x2 >> 1) << 1) + 1;
        area->y2 = ((area->y2 >> 1) << 1) + 1;
    }

    CustomLcdDisplay(esp_lcd_panel_io_handle_t io,
                     esp_lcd_panel_handle_t panel,
                     int w, int h,
                     int ox, int oy,
                     bool mx, bool my, bool sxy)
        : SpiLcdDisplay(io, panel, w, h, ox, oy, mx, my, sxy)
    {
        DisplayLockGuard lock(this);
        lv_obj_set_style_pad_left(status_bar_,  LV_HOR_RES * 0.1, 0);
        lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES * 0.1, 0);
        lv_display_add_event_cb(display_, rounder_event_cb,
                                LV_EVENT_INVALIDATE_AREA, NULL);
    }

};

// ── Backlight via SH8601 cmd 0x51 ────────────────────────────────────────────

class CustomBacklight : public Backlight {
public:
    CustomBacklight(esp_lcd_panel_io_handle_t panel_io)
        : Backlight(), panel_io_(panel_io) {}

protected:
    esp_lcd_panel_io_handle_t panel_io_;

    void SetBrightnessImpl(uint8_t brightness) override {
        auto display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        uint8_t data[1] = { (uint8_t)((255u * brightness) / 100u) };
        int cmd = 0x51;
        cmd &= 0xFF;
        cmd <<= 8;
        cmd |= (int)(LCD_OPCODE_WRITE_CMD << 24);
        esp_lcd_panel_io_tx_param(panel_io_, cmd, data, sizeof(data));
    }
};

// ── CatDisplayAdapter ────────────────────────────────────────────────────────
// Wraps CatDisplay (LVGL canvas) and implements the Display interface.
// The underlying CustomLcdDisplay owns LVGL; this adapter routes
// voice-assistant state into the cat animation.

class CatDisplayAdapter : public Display {
public:
    CatDisplayAdapter(CatDisplay* cat, CustomLcdDisplay* lvgl_display)
        : cat_(cat), lvgl_display_(lvgl_display)
    {
        width_  = DISPLAY_WIDTH;
        height_ = DISPLAY_HEIGHT;
    }

    // ── Display interface ──

    void SetStatus(const char* status) override {
        if (cat_) cat_->SetStateFromStatus(status);
    }

    void SetEmotion(const char* emotion) override {
        if (cat_) cat_->SetStateFromEmotion(emotion);
    }

    void SetChatMessage(const char* role, const char* content) override {
        if (content && *content)
            ESP_LOGI(TAG, "Chat [%s]: %.80s", role ? role : "?", content);
    }

    // Called periodically by the application — ticks the cat animation
    void UpdateStatusBar(bool update_all = false) override {
        if (!cat_) return;
        int level = 0;
        bool charging = false;
        bool discharging = false;
        if (Board::GetInstance().GetBatteryLevel(level, charging, discharging)) {
            cat_->SetBatteryStatus(level, charging);
            // Low-battery chime: fire once when discharging below the threshold,
            // re-arm only after the level recovers (hysteresis) to avoid spam.
            if (charging || level >= kLowBatteryClearPct) {
                low_batt_warned_ = false;
            } else if (!low_batt_warned_ && level <= kLowBatteryWarnPct) {
                low_batt_warned_ = true;
                Application::GetInstance().PlaySound(Lang::Sounds::OGG_LOW_BATTERY);
            }
        }
        cat_->Update();
    }

    void SetPowerSaveMode(bool on) override {
        if (!cat_) return;
        if (on) {
            cat_->SetState(CatDisplay::State::SLEEPING);
            cat_->Redraw();
        } else {
            cat_->SetState(CatDisplay::State::IDLE);
            cat_->Redraw();
        }
    }

    // ── Locking — use LVGL port mutex directly ──
    bool Lock(int timeout_ms = 0) override {
        return lvgl_port_lock(timeout_ms <= 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms));
    }
    void Unlock() override {
        lvgl_port_unlock();
    }

private:
    static constexpr int kLowBatteryWarnPct  = 20;  // chime at/below this while discharging
    static constexpr int kLowBatteryClearPct  = 25;  // re-arm once level recovers to this

    CatDisplay*       cat_          = nullptr;
    CustomLcdDisplay* lvgl_display_ = nullptr;
    bool              low_batt_warned_ = false;
};

// ── Board class ───────────────────────────────────────────────────────────────

class WaveshareEsp32s3TouchAMOLED2inch06 : public WifiBoard {
private:
    i2c_master_bus_handle_t   i2c_bus_         = nullptr;
    std::mutex                i2c_mutex_;
    esp_lcd_panel_io_handle_t panel_io_        = nullptr;
    esp_lcd_panel_handle_t    panel_handle_    = nullptr;
    Pmic*                     pmic_            = nullptr;
    Button                    boot_button_;
    CustomLcdDisplay*         lvgl_display_    = nullptr;
    CatDisplay*               cat_display_     = nullptr;
    CatDisplayAdapter*        display_adapter_ = nullptr;
    CustomBacklight*          backlight_       = nullptr;
    PowerSaveTimer*           power_save_timer_= nullptr;
    ServoController*          servo_controller_= nullptr;
    HomeBotBleService         homebot_ble_;
    esp_lcd_touch_handle_t    touch_           = nullptr;
    esp_timer_handle_t        touch_timer_     = nullptr;
    TaskHandle_t              touch_task_      = nullptr;
    StackType_t*              touch_stack_     = nullptr;
    StaticTask_t*             touch_tcb_       = nullptr;
    esp_lcd_panel_io_handle_t touch_io_        = nullptr;
    i2c_master_dev_handle_t   ft3168_dev_      = nullptr;
    bool                      touch_ready_     = false;
    bool                      touch_woke_from_sleep_ = false;
    bool                      last_touch_pressed_ = false;
    uint16_t                  last_touch_x_    = 0;
    uint16_t                  last_touch_y_    = 0;
    uint8_t                   last_touch_points_ = 0;
    esp_err_t                 last_touch_status_ = ESP_ERR_INVALID_STATE;
    std::mutex                touch_state_mutex_;
    sdmmc_card_t*             sdcard_          = nullptr;
    bool                      sdcard_mounted_  = false;
    qmi8658_dev_t             imu_             = {};
    bool                      imu_ready_       = false;
    std::mutex                imu_mutex_;
    bool                      vibration_ready_ = false;
    TaskHandle_t              motion_task_     = nullptr;
    int64_t                   last_shake_us_   = 0;
    TaskHandle_t              serial_console_task_ = nullptr;

    // ── Power save ──

    void InitializePowerSaveTimer() {
        constexpr int kSecondsToDim   = 60;
        constexpr int kSecondsToSleep = 30 * 60;
        power_save_timer_ = new PowerSaveTimer(-1, kSecondsToSleep, -1, kSecondsToDim);
        power_save_timer_->OnEnterDimMode([this]() {
            GetBacklight()->SetBrightness(30);
            GetDisplay()->SetPowerSaveMode(true);
        });
        power_save_timer_->OnExitDimMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnEnterSleepMode([this]() {
            GetBacklight()->SetBrightness(5);
            if (servo_controller_) {
                servo_controller_->SetPowerSaveMode(true);
            }
        });
        power_save_timer_->OnExitSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();
            if (servo_controller_) {
                servo_controller_->SetPowerSaveMode(false);
            }
            Application::GetInstance().PlaySound(Lang::Sounds::OGG_VIBRATION);
        });
        power_save_timer_->SetEnabled(true);
    }

    // ── I2C / AXP2101 ──

    void InitializeCodecI2c() {
        i2c_master_bus_config_t cfg = {
            .i2c_port   = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .flags      = { .enable_internal_pullup = 1 },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &i2c_bus_));
    }

    bool ProbeI2cDevice(uint8_t addr) {
        if (!i2c_bus_) {
            return false;
        }
        return i2c_master_probe(i2c_bus_, addr, pdMS_TO_TICKS(100)) == ESP_OK;
    }

    void LogI2cScan(const char* reason) {
        std::ostringstream oss;
        oss << reason << " I2C scan:";
        bool found = false;
        for (uint8_t addr = 0x08; addr < 0x78; ++addr) {
            if (ProbeI2cDevice(addr)) {
                char buf[8];
                snprintf(buf, sizeof(buf), " 0x%02X", addr);
                oss << buf;
                found = true;
            }
        }
        if (!found) {
            oss << " none";
        }
        ESP_LOGW(TAG, "%s", oss.str().c_str());
    }

    void InitializeAxp2101() {
        ESP_LOGI(TAG, "Init AXP2101");
        pmic_ = new Pmic(i2c_bus_, 0x34);
    }

    // ── SPI ──

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.sclk_io_num    = EXAMPLE_PIN_NUM_LCD_PCLK;
        buscfg.data0_io_num   = EXAMPLE_PIN_NUM_LCD_DATA0;
        buscfg.data1_io_num   = EXAMPLE_PIN_NUM_LCD_DATA1;
        buscfg.data2_io_num   = EXAMPLE_PIN_NUM_LCD_DATA2;
        buscfg.data3_io_num   = EXAMPLE_PIN_NUM_LCD_DATA3;
        buscfg.max_transfer_sz= DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
        buscfg.flags          = SPICOMMON_BUSFLAG_QUAD;
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    // ── Display — Phase 1: hardware panel only (no LVGL) ────────────────────
    // After this call panel_handle_ is ready and the display shows whatever
    // is drawn via esp_lcd_panel_draw_bitmap.  LVGL is NOT started yet, so
    // it is safe to draw a splash image directly to the panel.

    void InitializeLcdPanel() {
        // Panel IO (QSPI)
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = EXAMPLE_PIN_NUM_LCD_CS;
        io_config.dc_gpio_num = static_cast<gpio_num_t>(-1);
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 32;
        io_config.lcd_param_bits = 8;
        io_config.flags.quad_mode = true;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io_));

        // Panel driver
        const sh8601_vendor_config_t vendor_config = {
            .init_cmds      = vendor_specific_init,
            .init_cmds_size = sizeof(vendor_specific_init) /
                              sizeof(vendor_specific_init[0]),
            .flags = { .use_qspi_interface = 1 },
        };
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST;
        panel_config.rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config  = (void*)&vendor_config;

        ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(panel_io_, &panel_config, &panel_handle_));
        esp_lcd_panel_handle_t panel = panel_handle_;
        esp_lcd_panel_set_gap(panel, 0x16, 0);
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, false);
        if (DISPLAY_SWAP_XY) {
            esp_lcd_panel_swap_xy(panel, true);
        }
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);
    }

    // ── Display — Phase 2: LVGL + CatDisplay + backlight ────────────────────
    // Requires panel_handle_ and panel_io_ to already be set up by
    // InitializeLcdPanel().

    void InitializeDisplay() {
        esp_lcd_panel_handle_t panel = panel_handle_;

        // LVGL display in the SH8601 native orientation. This panel driver
        // does not support hardware swap_xy.
        lvgl_display_ = new CustomLcdDisplay(
            panel_io_, panel,
            DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);

        // Cat animation — LVGL canvas covering the full physical screen.
        lv_obj_t* canvas_obj = nullptr;
        if (lvgl_port_lock(1000)) {
            const size_t buf_bytes = (size_t)DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);
            uint16_t* canvas_buf = (uint16_t*)heap_caps_malloc(buf_bytes,
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (canvas_buf) {
                canvas_obj = lv_canvas_create(lv_scr_act());
                lv_canvas_set_buffer(canvas_obj, canvas_buf,
                                     DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                     LV_COLOR_FORMAT_RGB565);
                lv_obj_set_pos(canvas_obj, 0, 0);
            }
            lvgl_port_unlock();
        }

        cat_display_ = new CatDisplay(DISPLAY_WIDTH, DISPLAY_HEIGHT, canvas_obj);
        if (!cat_display_->Init()) {
            ESP_LOGE(TAG, "CatDisplay init failed");
            delete cat_display_;
            cat_display_ = nullptr;
        }

        // Adapter routes voice-assistant calls to cat
        display_adapter_ = new CatDisplayAdapter(cat_display_, lvgl_display_);

        // Backlight
        backlight_ = new CustomBacklight(panel_io_);
        backlight_->RestoreBrightness();
    }

    // ── Touch ──

    esp_err_t Ft3168ReadReg(uint8_t reg, uint8_t* data, size_t len) {
        if (!ft3168_dev_) {
            return ESP_ERR_INVALID_STATE;
        }
        std::lock_guard<std::mutex> lock(i2c_mutex_);
        return i2c_master_transmit_receive(ft3168_dev_, &reg, 1, data, len, pdMS_TO_TICKS(50));
    }

    esp_err_t Ft3168ReadReg8WithStop(uint8_t reg, uint8_t* value) {
        if (!ft3168_dev_) {
            return ESP_ERR_INVALID_STATE;
        }
        std::lock_guard<std::mutex> lock(i2c_mutex_);
        esp_err_t ret = i2c_master_transmit(ft3168_dev_, &reg, 1, pdMS_TO_TICKS(100));
        if (ret != ESP_OK) {
            return ret;
        }
        return i2c_master_receive(ft3168_dev_, value, 1, pdMS_TO_TICKS(100));
    }

    esp_err_t Ft3168WriteReg(uint8_t reg, uint8_t value) {
        if (!ft3168_dev_) {
            return ESP_ERR_INVALID_STATE;
        }
        const uint8_t data[] = {reg, value};
        std::lock_guard<std::mutex> lock(i2c_mutex_);
        return i2c_master_transmit(ft3168_dev_, data, sizeof(data), pdMS_TO_TICKS(50));
    }

    void LogI2cProbe(const char* reason) {
        if (!i2c_bus_) {
            return;
        }

        char found[160] = {};
        size_t offset = 0;
        for (uint8_t addr = 0x08; addr < 0x78; ++addr) {
            esp_err_t ret = i2c_master_probe(i2c_bus_, addr, pdMS_TO_TICKS(20));
            if (ret == ESP_OK && offset < sizeof(found)) {
                int written = snprintf(found + offset, sizeof(found) - offset,
                                       "%s0x%02X", offset ? " " : "", addr);
                if (written > 0) {
                    offset += std::min<size_t>(written, sizeof(found) - offset);
                }
            }
        }
        ESP_LOGI(TAG, "I2C probe %s: %s", reason, offset ? found : "no ACK");
    }

    void ConfigureTouchIntInput() {
        if (TOUCH_INT_PIN == GPIO_NUM_NC) {
            return;
        }
        gpio_config_t int_cfg = {
            .pin_bit_mask = BIT64(TOUCH_INT_PIN),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_NEGEDGE,
        };
        gpio_config(&int_cfg);
    }

    void WakeFt3168I2c() {
        if (TOUCH_INT_PIN == GPIO_NUM_NC) {
            return;
        }

        // FT3168 can NACK I2C in Hibernate. The Waveshare/FT3168 wake sequence is
        // to let the host hold INT low for at least 5 ms, then release it.
        gpio_config_t int_wake_cfg = {
            .pin_bit_mask = BIT64(TOUCH_INT_PIN),
            .mode = GPIO_MODE_OUTPUT_OD,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&int_wake_cfg);
        gpio_set_level(TOUCH_INT_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(8));
        gpio_set_level(TOUCH_INT_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(10));
        ConfigureTouchIntInput();
    }

    bool IsFt3168SleepError(esp_err_t ret) {
        return ret == ESP_ERR_INVALID_RESPONSE || ret == ESP_ERR_TIMEOUT;
    }

    void NormalizeTouchCoordinates(uint16_t* x, uint16_t* y) {
        uint16_t nx = *x;
        uint16_t ny = *y;

        if (DISPLAY_SWAP_XY) {
            std::swap(nx, ny);
        }

        nx = std::min<uint16_t>(nx, DISPLAY_WIDTH - 1);
        ny = std::min<uint16_t>(ny, DISPLAY_HEIGHT - 1);

        if (DISPLAY_MIRROR_X) {
            nx = (DISPLAY_WIDTH - 1) - nx;
        }
        if (DISPLAY_MIRROR_Y) {
            ny = (DISPLAY_HEIGHT - 1) - ny;
        }

        *x = nx;
        *y = ny;
    }

    void DisableTouchAfterI2cFailure(esp_err_t failure) {
        ESP_LOGW(TAG, "Touch disabled after I2C failure: %s", esp_err_to_name(failure));
        touch_ready_ = false;
        if (ft3168_dev_) {
            i2c_master_bus_rm_device(ft3168_dev_);
            ft3168_dev_ = nullptr;
        }
        const esp_err_t reset_status = i2c_master_bus_reset(i2c_bus_);
        if (reset_status != ESP_OK) {
            ESP_LOGW(TAG, "I2C bus recovery after touch failure failed: %s",
                     esp_err_to_name(reset_status));
        }
    }

    esp_err_t ReadFt3168Touch(uint16_t* x, uint16_t* y, uint8_t* point_count) {
        uint8_t reg = FT3168_REG_FINGER_NUM;
        uint8_t data[5] = {};
        esp_err_t ret = ESP_FAIL;
        for (int attempt = 0; attempt < 3; ++attempt) {
            memset(data, 0, sizeof(data));
            {
                std::lock_guard<std::mutex> lock(i2c_mutex_);
                ret = i2c_master_transmit_receive(ft3168_dev_, &reg, 1, data, sizeof(data), pdMS_TO_TICKS(100));
            }
            if (ret == ESP_OK) {
                break;
            }
            if (!IsFt3168SleepError(ret)) {
                break;
            }
            vTaskDelay(pdMS_TO_TICKS(3));
        }
        if (ret != ESP_OK) {
            return ret;
        }

        // data layout from reg 0x02: [finger_num][x1h][x1l][y1h][y1l]
        uint8_t fingers = data[0] & 0x0f;
        if (fingers == 0 || fingers > FT3168_MAX_POINTS) {
            *x = 0;
            *y = 0;
            *point_count = 0;
            return ESP_OK;
        }

        *x = ((uint16_t)(data[1] & 0x0f) << 8) | data[2];
        *y = ((uint16_t)(data[3] & 0x0f) << 8) | data[4];
        NormalizeTouchCoordinates(x, y);
        *point_count = fingers;
        return ESP_OK;
    }

    void InitializeTouch() {
        ESP_LOGI(TAG, "Init FT5x06-compatible touch at 0x%02X", FT3168_I2C_ADDRESS);
        ConfigureTouchIntInput();

        if (TOUCH_RST_ENABLED && TOUCH_RST_PIN != GPIO_NUM_NC) {
            gpio_config_t rst_cfg = {
                .pin_bit_mask = BIT64(TOUCH_RST_PIN),
                .mode = GPIO_MODE_OUTPUT,
                .pull_up_en = GPIO_PULLUP_DISABLE,
                .pull_down_en = GPIO_PULLDOWN_DISABLE,
                .intr_type = GPIO_INTR_DISABLE,
            };
            gpio_config(&rst_cfg);
            gpio_set_level(TOUCH_RST_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(1));
            gpio_set_level(TOUCH_RST_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(20));
            gpio_set_level(TOUCH_RST_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        // Waveshare's Arduino DriveBus talks to the FT3168 at 100 kHz.
        i2c_device_config_t dev_cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = FT3168_I2C_ADDRESS,
            .scl_speed_hz = 100000,
            .scl_wait_us = 20000,
            .flags = {},
        };

        esp_err_t ret = i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &ft3168_dev_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Touch I2C device init failed: %s", esp_err_to_name(ret));
            return;
        }

        esp_err_t probe_ret = ESP_FAIL;
        for (int attempt = 0; attempt < 50; ++attempt) {
            probe_ret = i2c_master_probe(i2c_bus_, FT3168_I2C_ADDRESS, pdMS_TO_TICKS(100));
            if (probe_ret == ESP_OK) {
                break;
            }
            if (attempt == 0 || (attempt + 1) % 10 == 0) {
                ESP_LOGW(TAG, "FT3168 probe attempt %d failed: %s",
                         attempt + 1, esp_err_to_name(probe_ret));
                LogI2cScan("while waiting for FT3168");
            }
            if ((attempt + 1) % 10 == 0) {
                esp_err_t reset_ret = i2c_master_bus_reset(i2c_bus_);
                if (reset_ret != ESP_OK) {
                    ESP_LOGW(TAG, "I2C bus reset while waiting for FT3168 failed: %s",
                             esp_err_to_name(reset_ret));
                }
            }
            vTaskDelay(pdMS_TO_TICKS(100));
        }
        ESP_LOGI(TAG, "FT3168 probe: %s", esp_err_to_name(probe_ret));

        // Verify comms with the repeated-start transaction used by the touch task.
        uint8_t reg2 = FT3168_REG_FINGER_NUM;
        uint8_t touch_data[5] = {};
        esp_err_t touch_ret;
        {
            std::lock_guard<std::mutex> lock(i2c_mutex_);
            touch_ret = i2c_master_transmit_receive(ft3168_dev_, &reg2, 1, touch_data, sizeof(touch_data), pdMS_TO_TICKS(100));
        }
        ESP_LOGI(TAG, "FT3168 regs[2-6]: %s %02X %02X %02X %02X %02X",
                 esp_err_to_name(touch_ret), touch_data[0], touch_data[1],
                 touch_data[2], touch_data[3], touch_data[4]);

        // FT3x68 default touch interrupt is an active-low pulse.
        esp_err_t isr_ret = gpio_install_isr_service(0);
        if (isr_ret != ESP_OK && isr_ret != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "Touch ISR service install failed: %s", esp_err_to_name(isr_ret));
        }

        touch_ready_ = true;
        ESP_LOGI(TAG, "FT5x06-compatible touch enabled on GPIO%d falling edge", TOUCH_INT_PIN);

        // Stack in PSRAM so we don't eat internal SRAM (which fragments DMA pools).
        constexpr uint32_t kStackWords = 4096 / sizeof(StackType_t);
        touch_stack_ = static_cast<StackType_t*>(
            heap_caps_malloc(kStackWords * sizeof(StackType_t),
                             MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        touch_tcb_ = static_cast<StaticTask_t*>(
            heap_caps_malloc(sizeof(StaticTask_t),
                             MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
        if (!touch_stack_ || !touch_tcb_) {
            ESP_LOGE(TAG, "Failed to allocate touch task memory");
            return;
        }
        touch_task_ = xTaskCreateStaticPinnedToCore(
            TouchTask, "touch_reader", kStackWords,
            this, 5, touch_stack_, touch_tcb_, 0);
        if (touch_task_) {
            esp_err_t add_ret = gpio_isr_handler_add(TOUCH_INT_PIN, TouchIsrHandler, this);
            if (add_ret != ESP_OK) {
                ESP_LOGW(TAG, "Touch ISR handler add failed: %s", esp_err_to_name(add_ret));
            }
        }
    }

    static void IRAM_ATTR TouchIsrHandler(void* arg) {
        auto* board = static_cast<WaveshareEsp32s3TouchAMOLED2inch06*>(arg);
        BaseType_t higher_priority_woken = pdFALSE;
        if (board->touch_task_) {
            vTaskNotifyGiveFromISR(board->touch_task_, &higher_priority_woken);
        }
        if (higher_priority_woken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }

    static void TouchTask(void* arg) {
        auto* board = static_cast<WaveshareEsp32s3TouchAMOLED2inch06*>(arg);
        bool was_touched = false;
        int64_t touch_start_us = 0;
        int64_t last_touch_us = 0;
        int64_t last_probe_us = 0;
        int64_t last_read_us = 0;
        int64_t last_int_log_us = 0;
        int64_t last_error_log_us = 0;
        int64_t ignore_irq_until_us = 0;
        int last_int_level = -1;
        uint16_t last_x = 0, last_y = 0;

        auto finish_touch = [&]() {
            was_touched = false;
            const int64_t duration_ms = (last_touch_us - touch_start_us) / 1000;
            {
                std::lock_guard<std::mutex> lock(board->touch_state_mutex_);
                board->last_touch_status_ = ESP_OK;
                board->last_touch_pressed_ = false;
                board->last_touch_points_ = 0;
            }
            if (board->touch_woke_from_sleep_) {
                board->touch_woke_from_sleep_ = false;
                return;
            }
            if (duration_ms >= 0 && duration_ms < 1200) {
                Application::GetInstance().Schedule([]() {
                    Application::GetInstance().ToggleChatState();
                });
            } else if (duration_ms >= 1200 && duration_ms < 5000) {
                char event_context[96];
                snprintf(event_context, sizeof(event_context),
                         "{\"x\":%u,\"y\":%u,\"duration_ms\":%lld}",
                         static_cast<unsigned>(last_x),
                         static_cast<unsigned>(last_y),
                         static_cast<long long>(duration_ms));
                Application::GetInstance().SendCharacterEvent("pet", event_context);
            }
        };

        while (true) {
            const uint32_t notified = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50));
            const int64_t now = esp_timer_get_time();
            const int int_level = TOUCH_INT_PIN == GPIO_NUM_NC ? 0 : gpio_get_level(TOUCH_INT_PIN);
            bool read_due = notified != 0 || int_level == 0 || was_touched;
            if (int_level != last_int_level || (now - last_int_log_us) > 5000000LL) {
                ESP_LOGI(TAG, "touch-int irq=%lu int=%d ready=%d",
                         static_cast<unsigned long>(notified), int_level, board->touch_ready_ ? 1 : 0);
                last_int_level = int_level;
                last_int_log_us = now;
            }

            if (notified && now < ignore_irq_until_us && int_level != 0) {
                continue;
            }

            if (!read_due) {
                continue;
            }
            if (was_touched && !notified && int_level != 0) {
                if ((now - last_probe_us) < 50000LL) {
                    continue;
                }
                last_probe_us = now;
            }
            if (was_touched && (now - last_read_us) < 15000LL) {
                ulTaskNotifyTake(pdTRUE, 0);
                continue;
            }

            uint16_t x = 0, y = 0;
            uint8_t points = 0;
            if (notified && !was_touched) {
                vTaskDelay(pdMS_TO_TICKS(5));
            }
            esp_err_t ret = board->ReadFt3168Touch(&x, &y, &points);
            last_read_us = esp_timer_get_time();

            // Diagnostic: log touch movement at a bounded rate and any status change.
            {
                static uint32_t n = 0;
                static esp_err_t prev = ESP_OK;
                static uint16_t log_x = 0;
                static uint16_t log_y = 0;
                static int64_t last_point_log_us = 0;
                ++n;
                const uint16_t dx = x > log_x ? x - log_x : log_x - x;
                const uint16_t dy = y > log_y ? y - log_y : log_y - y;
                const bool moved = (dx + dy) >= 8;
                const bool should_log =
                    (ret == ESP_OK && points > 0 && (moved || (now - last_point_log_us) > 150000LL)) ||
                    ret != prev ||
                    (now - last_error_log_us) > 1000000LL;
                if (should_log) {
                    ESP_LOGI(TAG, "touch#%lu irq=%lu probe=%d int=%d %s pts=%u x=%u y=%u",
                             static_cast<unsigned long>(n),
                             static_cast<unsigned long>(notified),
                             (was_touched && !notified) ? 1 : 0,
                             int_level,
                             esp_err_to_name(ret), points, x, y);
                    prev = ret;
                    if (ret != ESP_OK) {
                        last_error_log_us = now;
                    } else if (points > 0) {
                        log_x = x;
                        log_y = y;
                        last_point_log_us = now;
                    }
                }
            }

            if (ret == ESP_OK && points > 0) {
                ignore_irq_until_us = 0;
                last_x = x;
                last_y = y;
                last_touch_us = now;
                {
                    std::lock_guard<std::mutex> lock(board->touch_state_mutex_);
                    board->last_touch_status_ = ESP_OK;
                    board->last_touch_pressed_ = true;
                    board->last_touch_points_ = points;
                    board->last_touch_x_ = x;
                    board->last_touch_y_ = y;
                }
                if (!was_touched) {
                    was_touched = true;
                    touch_start_us = now;
                    board->touch_woke_from_sleep_ =
                        board->power_save_timer_ &&
                        (board->power_save_timer_->IsSleeping() ||
                         board->power_save_timer_->IsDimmed());
                    if (board->power_save_timer_) {
                        board->power_save_timer_->WakeUp();
                    }
                    if (board->touch_woke_from_sleep_) {
                        ESP_LOGI(TAG, "Touch wake-up");
                    }
                    if (board->cat_display_) {
                        board->cat_display_->SpawnTouchBubbles(x, y);
                    }
                }
                if (board->cat_display_) {
                    board->cat_display_->AddTrailPoint(x, y);
                }
            } else {
                if (ret != ESP_OK && board->IsFt3168SleepError(ret)) {
                    ignore_irq_until_us = now + 250000LL;
                }
                if (was_touched && (now - last_touch_us) > 100000LL) {
                // No touch for 100ms — finger lifted.
                    finish_touch();
                }
            }
        }
    }

    // ── uSD ──

    void InitializeSdCard() {
        esp_vfs_fat_sdmmc_mount_config_t mount_config = {
            .format_if_mount_failed = false,
            .max_files = 5,
            .allocation_unit_size = 16 * 1024,
            .disk_status_check_enable = false,
        };
        sdmmc_host_t host = SDMMC_HOST_DEFAULT();
        sdmmc_slot_config_t slot_config = {
            .clk = SDCARD_CLK_PIN,
            .cmd = SDCARD_CMD_PIN,
            .d0 = SDCARD_D0_PIN,
            .d1 = GPIO_NUM_NC,
            .d2 = GPIO_NUM_NC,
            .d3 = GPIO_NUM_NC,
            .d4 = GPIO_NUM_NC,
            .d5 = GPIO_NUM_NC,
            .d6 = GPIO_NUM_NC,
            .d7 = GPIO_NUM_NC,
            .cd = SDMMC_SLOT_NO_CD,
            .wp = SDMMC_SLOT_NO_WP,
            .width = 1,
            .flags = 0,
        };

        esp_err_t ret = esp_vfs_fat_sdmmc_mount(SDCARD_MOUNT_POINT, &host, &slot_config, &mount_config, &sdcard_);
        if (ret == ESP_OK) {
            sdcard_mounted_ = true;
            ESP_LOGI(TAG, "SD card mounted at %s: %s", SDCARD_MOUNT_POINT, sdcard_->cid.name);
        } else {
            sdcard_ = nullptr;
            ESP_LOGW(TAG, "SD card mount skipped/failed: %s", esp_err_to_name(ret));
        }
    }

    // ── IMU ──

    void InitializeImu() {
        ESP_LOGI(TAG, "Init QMI8658 IMU");
        if (!ProbeI2cDevice(QMI8658_ADDRESS_HIGH) && !ProbeI2cDevice(QMI8658_ADDRESS_LOW)) {
            ESP_LOGW(TAG, "QMI8658 IMU not found");
            return;
        }

        uint8_t addr = ProbeI2cDevice(QMI8658_ADDRESS_HIGH) ? QMI8658_ADDRESS_HIGH : QMI8658_ADDRESS_LOW;
        esp_err_t ret = qmi8658_init(&imu_, i2c_bus_, addr);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "QMI8658 init failed: %s", esp_err_to_name(ret));
            return;
        }
        qmi8658_set_accel_range(&imu_, QMI8658_ACCEL_RANGE_8G);
        qmi8658_set_accel_odr(&imu_, QMI8658_ACCEL_ODR_500HZ);
        qmi8658_set_gyro_range(&imu_, QMI8658_GYRO_RANGE_512DPS);
        qmi8658_set_gyro_odr(&imu_, QMI8658_GYRO_ODR_500HZ);
        qmi8658_set_accel_unit_mps2(&imu_, true);
        qmi8658_set_gyro_unit_dps(&imu_, true);
        imu_ready_ = true;
    }

    // ── Motion interaction / optional haptic motor ──

    void InitializeVibrationMotor() {
#if VIBRATION_MOTOR_ENABLED
        gpio_config_t config = {
            .pin_bit_mask = BIT64(VIBRATION_MOTOR_GPIO),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        if (gpio_config(&config) != ESP_OK) {
            ESP_LOGE(TAG, "Vibration motor GPIO setup failed");
            return;
        }
        vibration_ready_ = true;
        gpio_set_level(VIBRATION_MOTOR_GPIO, VIBRATION_MOTOR_ACTIVE_HIGH ? 0 : 1);
        ESP_LOGI(TAG, "Vibration motor enabled on GPIO%d", VIBRATION_MOTOR_GPIO);
#else
        ESP_LOGI(TAG, "Vibration motor disabled: set VIBRATION_MOTOR_ENABLED and GPIO after wiring");
#endif
    }

    void SetVibration(bool enabled) {
#if VIBRATION_MOTOR_ENABLED
        if (vibration_ready_) {
            gpio_set_level(VIBRATION_MOTOR_GPIO,
                           enabled == VIBRATION_MOTOR_ACTIVE_HIGH ? 1 : 0);
        }
#else
        (void)enabled;
#endif
    }

    void RunShakeReaction(float impulse, float rotation) {
        ESP_LOGI(TAG, "Strong shake detected: impulse=%.1f m/s2 rotation=%.1f dps",
                 impulse, rotation);
        if (power_save_timer_) {
            power_save_timer_->WakeUp();
        }
        if (cat_display_) {
            cat_display_->PlayDizzy();
        }
        char event_context[96];
        snprintf(event_context, sizeof(event_context),
                 "{\"impulse_mps2\":%.1f,\"rotation_dps\":%.1f,\"reaction\":\"dizzy\"}",
                 impulse, rotation);
        Application::GetInstance().SendCharacterEvent("shake", event_context);

        SetVibration(true);
        vTaskDelay(pdMS_TO_TICKS(120));
        SetVibration(false);
        if (servo_controller_) {
            servo_controller_->SetLed(255, 205, 45);
            servo_controller_->SetPose("dizzy");
            servo_controller_->SetDefaultLed();
        }
        SetVibration(true);
        vTaskDelay(pdMS_TO_TICKS(100));
        SetVibration(false);
    }

    static void MotionInteractionTask(void* arg) {
        auto* board = static_cast<WaveshareEsp32s3TouchAMOLED2inch06*>(arg);
        int shake_samples = 0;
        while (true) {
            qmi8658_data_t data = {};
            esp_err_t read_status = ESP_ERR_INVALID_STATE;
            if (board->imu_ready_) {
                std::lock_guard<std::mutex> lock(board->imu_mutex_);
                std::lock_guard<std::mutex> i2c_lock(board->i2c_mutex_);
                read_status = qmi8658_read_sensor_data(&board->imu_, &data);
            }
            if (read_status == ESP_OK) {
                const float accel = sqrtf(data.accelX * data.accelX +
                                          data.accelY * data.accelY +
                                          data.accelZ * data.accelZ);
                const float impulse = fabsf(accel - 9.81f);
                const float rotation = sqrtf(data.gyroX * data.gyroX +
                                             data.gyroY * data.gyroY +
                                             data.gyroZ * data.gyroZ);
                const bool strong = (impulse > 10.0f && rotation > 130.0f) ||
                                    impulse > 19.0f || rotation > 550.0f;
                shake_samples = strong ? shake_samples + 1 : std::max(0, shake_samples - 1);

                const int64_t now = esp_timer_get_time();
                if ((shake_samples >= 2 || impulse > 28.0f) &&
                    now - board->last_shake_us_ > 4000000LL) {
                    board->last_shake_us_ = now;
                    shake_samples = 0;
                    board->RunShakeReaction(impulse, rotation);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(35));
        }
    }

    void InitializeMotionInteraction() {
        InitializeVibrationMotor();
        if (!imu_ready_) {
            ESP_LOGW(TAG, "Shake interaction disabled: IMU not ready");
            return;
        }
        BaseType_t ret = xTaskCreate(
            MotionInteractionTask,
            "shake_interaction",
            4096,
            this,
            3,
            &motion_task_);
        if (ret != pdPASS) {
            motion_task_ = nullptr;
            ESP_LOGE(TAG, "Failed to start shake interaction task");
        } else {
            ESP_LOGI(TAG, "Shake interaction ready");
        }
    }

    // ── Button ──

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting &&
                !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
                return;
            }
            app.PlaySound(Lang::Sounds::OGG_POPUP);
            app.ToggleChatState();
            power_save_timer_->WakeUp();
        });
#if CONFIG_USE_DEVICE_AEC
        boot_button_.OnDoubleClick([]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle)
                app.SetAecMode(app.GetAecMode() == kAecOff ? kAecOnDeviceSide : kAecOff);
        });
#endif
    }

    // ── Servos ──

    void InitializeServoController() {
        servo_controller_ = new ServoController();
        if (servo_controller_ && servo_controller_->Init()) {
            ESP_LOGI(TAG, "ServoController OK");
        } else {
            ESP_LOGE(TAG, "ServoController init failed");
            delete servo_controller_;
            servo_controller_ = nullptr;
        }
        // Console always starts, regardless of servo availability
        InitializeSerialCommandConsole();
    }

    static std::string Trim(const std::string& value) {
        size_t first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return "";
        }
        size_t last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    static std::string Uppercase(std::string value) {
        for (auto& ch : value) {
            ch = (char)std::toupper((unsigned char)ch);
        }
        return value;
    }

    static bool LooksLikeSerialCommand(const std::string& line) {
        std::string trimmed = Trim(line);
        if (trimmed.empty()) {
            return false;
        }
        unsigned char first = (unsigned char)trimmed[0];
        if (!std::isalpha(first) && first != '?') {
            return false;
        }
        for (unsigned char ch : trimmed) {
            if (ch < 32 || ch > 126) {
                return false;
            }
        }
        return true;
    }

    void PrintSerialCommandHelp() {
        printf("\nAMOLED console commands:\n");
        printf("  HELP\n");
        printf("  CHAT                           — toggle voice assistant\n");
        printf("  TRANSLATE <th|ru|en|off>       — translator mode\n");
        printf("  EMOTION <happy|sad|angry|shy|love|sleep>\n");
        printf("  STATUS?\n");
        printf("  DIST?\n");
        printf("  RADAR?                         — C1001 presence/motion/distance\n");
        printf("  SERVO <yaw> <pitch>\n");
        printf("  YAW <angle>\n");
        printf("  PITCH <angle>\n");
        printf("  POSE <home|left|right|up|down|nod|shake|dance>\n");
        printf("  LED <r> <g> <b>\n");
        printf("  LEDDEFAULT\n");
        printf("  LEDTEST\n");
        printf("  LEDOFF\n");
        printf("  FRAME\n");
        printf("  STREAM OFF\n\n");
    }

    void HandleSerialCommand(const std::string& raw_line) {
        std::string line = Trim(raw_line);
        if (line.empty()) {
            return;
        }

        std::istringstream iss(line);
        std::string command;
        iss >> command;
        command = Uppercase(command);
        if (command.size() > 1 && command.back() == '?') {
            command.pop_back();
        }

        if (command == "HELP" || command == "?") {
            PrintSerialCommandHelp();
            if (!servo_controller_) printf("  (XIAO bridge not connected — servo commands unavailable)\n");
            fflush(stdout);
            return;
        } else if (command == "CHAT" || command == "VOICE") {
            Application::GetInstance().ToggleChatState();
            printf("OK CHAT\n");
            fflush(stdout);
            return;
        } else if (command == "TRANSLATE" || command == "TRANSLATOR") {
            std::string target;
            if (!(iss >> target)) {
                printf("ERR usage: TRANSLATE <th|ru|en|off>\n");
                fflush(stdout);
                return;
            }
            std::string target_upper = Uppercase(target);
            if (target_upper == "OFF" || target_upper == "0" || target_upper == "FALSE") {
                Application::GetInstance().Schedule([]() {
                    Application::GetInstance().SetTranslatorMode(false);
                });
                printf("OK TRANSLATE OFF\n");
                fflush(stdout);
                return;
            }

            std::string language = target;
            if (target_upper == "TH" || target_upper == "THAI") {
                language = "Thai";
            } else if (target_upper == "RU" || target_upper == "RUSSIAN") {
                language = "Russian";
            } else if (target_upper == "EN" || target_upper == "ENGLISH") {
                language = "English";
            }
            Application::GetInstance().Schedule([language]() {
                auto& app = Application::GetInstance();
                app.SetTranslatorMode(true, language, "auto");
                if (app.GetDeviceState() == kDeviceStateIdle) {
                    app.ToggleChatState();
                }
            });
            printf("OK TRANSLATE %s\n", language.c_str());
            fflush(stdout);
            return;
        } else if (command == "EMOTION") {
            std::string emo;
            if (iss >> emo) {
                if (cat_display_) cat_display_->SetStateFromEmotion(emo.c_str());
                printf("OK EMOTION %s\n", emo.c_str());
            } else {
                printf("ERR usage: EMOTION <happy|sad|angry|shy|love|sleep>\n");
            }
            fflush(stdout);
            return;
        } else if (!servo_controller_) {
            printf("ERR XIAO bridge not ready\n");
            fflush(stdout);
        } else if (command == "STATUS") {
            std::string response;
            printf("%s\n", servo_controller_->RequestStatus(response) ? response.c_str() : "ERR STATUS_TIMEOUT");
        } else if (command == "DIST" || command == "DISTANCE") {
            std::string response;
            printf("%s\n", servo_controller_->RequestDistance(response) ? response.c_str() : "ERR DIST_TIMEOUT");
        } else if (command == "RADAR") {
            std::string response;
            printf("%s\n", servo_controller_->RequestRadar(response) ? response.c_str() : "ERR RADAR_TIMEOUT");
        } else if (command == "YAW") {
            int angle = 0;
            if (iss >> angle) {
                printf("%s\n", servo_controller_->SetServoAngle(SERVO_HEAD_YAW_NUM, angle) ? "OK YAW" : "ERR YAW");
            } else {
                printf("ERR usage: YAW <angle>\n");
            }
        } else if (command == "PITCH") {
            int angle = 0;
            if (iss >> angle) {
                printf("%s\n", servo_controller_->SetServoAngle(SERVO_HEAD_PITCH_NUM, angle) ? "OK PITCH" : "ERR PITCH");
            } else {
                printf("ERR usage: PITCH <angle>\n");
            }
        } else if (command == "SERVO") {
            int yaw = 0;
            int pitch = 0;
            if (iss >> yaw >> pitch) {
                bool ok = servo_controller_->SetMultipleServos({
                    {SERVO_HEAD_YAW_NUM, yaw},
                    {SERVO_HEAD_PITCH_NUM, pitch},
                });
                printf("%s\n", ok ? "OK SERVO" : "ERR SERVO");
            } else {
                printf("ERR usage: SERVO <yaw> <pitch>\n");
            }
        } else if (command == "POSE") {
            std::string pose;
            if (iss >> pose) {
                printf("%s\n", servo_controller_->SetPose(pose) ? "OK POSE" : "ERR POSE");
            } else {
                printf("ERR usage: POSE <name>\n");
            }
        } else if (command == "LED") {
            int r = 0;
            int g = 0;
            int b = 0;
            if (iss >> r >> g >> b) {
                printf("%s\n", servo_controller_->SetLed(r, g, b) ? "OK LED" : "ERR LED");
            } else {
                printf("ERR usage: LED <r> <g> <b>\n");
            }
        } else if (command == "LEDDEFAULT") {
            printf("%s\n", servo_controller_->SetDefaultLed() ? "OK LEDDEFAULT" : "ERR LEDDEFAULT");
        } else if (command == "LEDTEST") {
            printf("%s\n", servo_controller_->LedTest() ? "OK LEDTEST" : "ERR LEDTEST");
        } else if (command == "LEDOFF") {
            printf("%s\n", servo_controller_->LedOff() ? "OK LEDOFF" : "ERR LEDOFF");
        } else if (command == "FRAME") {
            std::string jpeg;
            std::string error;
            uint32_t frame_id = 0;
            if (servo_controller_->RequestFrame(jpeg, frame_id, error)) {
                printf("OK FRAME id=%lu bytes=%u\n", (unsigned long)frame_id, (unsigned)jpeg.size());
            } else {
                printf("ERR FRAME %s\n", error.c_str());
            }
        } else if (command == "STREAM") {
            std::string arg;
            iss >> arg;
            if (Uppercase(arg) == "OFF") {
                printf("%s\n", servo_controller_->CameraStreamOff() ? "OK STREAM_OFF" : "ERR STREAM_OFF");
            } else {
                printf("ERR only STREAM OFF is supported from AMOLED console\n");
            }
        } else {
            printf("ERR UNKNOWN_CMD. Type HELP\n");
        }
        fflush(stdout);
    }

    static void SerialConsoleTask(void* arg) {
        auto* board = static_cast<WaveshareEsp32s3TouchAMOLED2inch06*>(arg);
        std::string line;
        bool last_was_cr = false;
        int64_t last_char_us = 0;
        printf("\nAMOLED command console ready. Type HELP.\n");
        fflush(stdout);
        while (true) {
            int ch = fgetc(stdin);
            if (ch == EOF) {
                if (!line.empty() && esp_timer_get_time() - last_char_us > 500000) {
                    if (LooksLikeSerialCommand(line)) {
                        printf("AMOLED> %s\n", line.c_str());
                        fflush(stdout);
                        board->HandleSerialCommand(line);
                    }
                    line.clear();
                    last_was_cr = false;
                }
                vTaskDelay(pdMS_TO_TICKS(20));
                continue;
            }
            if (ch == '\r' || ch == '\n') {
                if (ch == '\n' && last_was_cr) {
                    last_was_cr = false;
                    continue;
                }
                last_was_cr = ch == '\r';
                if (LooksLikeSerialCommand(line)) {
                    printf("AMOLED> %s\n", line.c_str());
                    fflush(stdout);
                    board->HandleSerialCommand(line);
                }
                line.clear();
                continue;
            }
            last_was_cr = false;
            if (ch >= 32 && ch <= 126) {
                if (line.size() >= 160) {
                    line.clear();
                    continue;
                }
                line.push_back((char)ch);
                last_char_us = esp_timer_get_time();
            }
        }
    }

    void InitializeSerialCommandConsole() {
        if (serial_console_task_) {
            return;
        }
        // USB Serial/JTAG as primary console: IDF uses blocking VFS for stdin. Never call
        // usb_serial_jtag_vfs_use_nonblocking() here — it triggers assert in usb_serial_jtag_read
        // when typing in idf.py monitor (IDF v6.x usb_serial_jtag_vfs.c).

        BaseType_t ret = xTaskCreate(
            SerialConsoleTask,
            "amoled_console",
            4096,
            this,
            2,
            &serial_console_task_);
        if (ret != pdPASS) {
            serial_console_task_ = nullptr;
            ESP_LOGE(TAG, "Failed to start AMOLED command console");
        }
    }

    void RegisterMcpTools() {
        auto& mcp = McpServer::GetInstance();

        mcp.AddTool("self.system.reconfigure_wifi",
            "Reboot the device and enter WiFi configuration mode.\n"
            "**CAUTION** — ask the user to confirm first.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                ResetWifiConfiguration(); return true;
            });

        if (!servo_controller_) return;

        mcp.AddTool("self.robot.set_servo",
            "Управление сервоприводами головы через XIAO: 1=yaw, 2=pitch, угол 40-80°",
            PropertyList({
                Property("servo_num", kPropertyTypeInteger, 1,  1, 2),
                Property("angle",     kPropertyTypeInteger, SERVO_HOME_ANGLE, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE),
            }),
            [this](const PropertyList& p) -> ReturnValue {
                return servo_controller_ &&
                       servo_controller_->SetServoAngle(
                           p["servo_num"].value<int>(), p["angle"].value<int>());
            });

        mcp.AddTool("self.robot.set_pose",
            "Поза головы: home/reset, left/right/up/down, nod, shake, dance, "
            "dizzy, greet/greeting, sad/sad_pose, happy/happy_pose",
            PropertyList({ Property("pose", kPropertyTypeString) }),
            [this](const PropertyList& p) -> ReturnValue {
                return servo_controller_ &&
                       servo_controller_->SetPose(p["pose"].value<std::string>());
            });

        mcp.AddTool("self.robot.move_servos",
            "Несколько сервоприводов головы через XIAO: 'S1:45,S2:120'",
            PropertyList({ Property("servos", kPropertyTypeString) }),
            [this](const PropertyList& p) -> ReturnValue {
                std::string s = p["servos"].value<std::string>();
                std::vector<std::pair<int,int>> cmds;
                std::istringstream iss(s);
                std::string tok;
                bool ok = true;
                while (std::getline(iss, tok, ',')) {
                    tok.erase(0, tok.find_first_not_of(" \t"));
                    tok.erase(tok.find_last_not_of(" \t") + 1);
                    if (!tok.empty() && (tok[0]=='S'||tok[0]=='s')) {
                        size_t cp = tok.find(':');
                        if (cp != std::string::npos)
                            cmds.push_back({ std::stoi(tok.substr(1,cp-1)),
                                             std::stoi(tok.substr(cp+1)) });
                        else ok = false;
                    }
                }
                return ok && servo_controller_ &&
                       servo_controller_->SetMultipleServos(cmds);
            });

        mcp.AddTool("self.robot.set_led",
            "Set XIAO LED ring color. RGB values 0-255.",
            PropertyList({
                Property("r", kPropertyTypeInteger, XIAO_LED_DEFAULT_R, 0, 255),
                Property("g", kPropertyTypeInteger, XIAO_LED_DEFAULT_G, 0, 255),
                Property("b", kPropertyTypeInteger, XIAO_LED_DEFAULT_B, 0, 255),
            }),
            [this](const PropertyList& p) -> ReturnValue {
                return servo_controller_ &&
                       servo_controller_->SetLed(
                           p["r"].value<int>(), p["g"].value<int>(), p["b"].value<int>());
            });

        mcp.AddTool("self.robot.led_default",
            "Set XIAO LED ring to default neon blue.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                return servo_controller_ && servo_controller_->SetDefaultLed();
            });

        mcp.AddTool("self.robot.led_off",
            "Turn off XIAO LED ring.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                return servo_controller_ && servo_controller_->LedOff();
            });

        mcp.AddTool("self.robot.led_test",
            "Run XIAO LED ring RGB/white test animation.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                return servo_controller_ && servo_controller_->LedTest();
            });

        ESP_LOGI(TAG, "Robot MCP tools registered");
    }

    void RegisterHardwareMcpTools() {
        auto& mcp = McpServer::GetInstance();

        mcp.AddTool("self.hardware.get_motion",
            "Read QMI8658 accelerometer and gyroscope data.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                cJSON* root = cJSON_CreateObject();
                cJSON_AddBoolToObject(root, "ready", imu_ready_);
                if (!imu_ready_) {
                    return root;
                }
                qmi8658_data_t data = {};
                esp_err_t ret;
                {
                    std::lock_guard<std::mutex> lock(imu_mutex_);
                    std::lock_guard<std::mutex> i2c_lock(i2c_mutex_);
                    ret = qmi8658_read_sensor_data(&imu_, &data);
                }
                if (ret != ESP_OK) {
                    cJSON_AddStringToObject(root, "error", esp_err_to_name(ret));
                    return root;
                }
                cJSON* accel = cJSON_CreateObject();
                cJSON_AddNumberToObject(accel, "x_mps2", data.accelX);
                cJSON_AddNumberToObject(accel, "y_mps2", data.accelY);
                cJSON_AddNumberToObject(accel, "z_mps2", data.accelZ);
                cJSON_AddItemToObject(root, "accel", accel);

                cJSON* gyro = cJSON_CreateObject();
                cJSON_AddNumberToObject(gyro, "x_dps", data.gyroX);
                cJSON_AddNumberToObject(gyro, "y_dps", data.gyroY);
                cJSON_AddNumberToObject(gyro, "z_dps", data.gyroZ);
                cJSON_AddItemToObject(root, "gyro", gyro);

                cJSON_AddNumberToObject(root, "temperature_c", data.temperature);
                cJSON_AddNumberToObject(root, "timestamp", data.timestamp);
                return root;
            });

        mcp.AddTool("self.hardware.get_touch",
            "Read the touchscreen state.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                cJSON* root = cJSON_CreateObject();
                cJSON_AddBoolToObject(root, "ready", touch_ready_);
                if (!touch_ready_) {
                    return root;
                }
                uint16_t x = 0;
                uint16_t y = 0;
                uint8_t points = 0;
                esp_err_t ret = ESP_OK;
                bool read_now = true;
                if (read_now) {
                    ret = ReadFt3168Touch(&x, &y, &points);
                    {
                        std::lock_guard<std::mutex> lock(touch_state_mutex_);
                        last_touch_status_ = ret;
                        last_touch_pressed_ = ret == ESP_OK && points > 0;
                        last_touch_points_ = ret == ESP_OK ? points : 0;
                        if (last_touch_pressed_) {
                            last_touch_x_ = x;
                            last_touch_y_ = y;
                        }
                    }
                } else {
                    std::lock_guard<std::mutex> lock(touch_state_mutex_);
                    ret = last_touch_status_ == ESP_ERR_INVALID_STATE ? ESP_OK : last_touch_status_;
                    points = last_touch_pressed_ ? last_touch_points_ : 0;
                    x = last_touch_x_;
                    y = last_touch_y_;
                }
                cJSON_AddStringToObject(root, "read_status", esp_err_to_name(ret));
                cJSON_AddBoolToObject(root, "read_now", read_now);
                cJSON_AddNumberToObject(root, "int_level", TOUCH_INT_PIN == GPIO_NUM_NC ? -1 : gpio_get_level(TOUCH_INT_PIN));
                cJSON_AddBoolToObject(root, "touched", points > 0);
                cJSON_AddNumberToObject(root, "points", points);
                if (points > 0) {
                    cJSON_AddNumberToObject(root, "x", x);
                    cJSON_AddNumberToObject(root, "y", y);
                } else {
                    std::lock_guard<std::mutex> lock(touch_state_mutex_);
                    cJSON_AddBoolToObject(root, "last_touched", last_touch_pressed_);
                    cJSON_AddNumberToObject(root, "last_points", last_touch_points_);
                    cJSON_AddNumberToObject(root, "last_x", last_touch_x_);
                    cJSON_AddNumberToObject(root, "last_y", last_touch_y_);
                }
                cJSON_AddStringToObject(root, "controller", "FT5x06-compatible");
                return root;
            });

        mcp.AddTool("self.hardware.get_sdcard",
            "Read SD card mount and card information.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                cJSON* root = cJSON_CreateObject();
                cJSON_AddBoolToObject(root, "mounted", sdcard_mounted_);
                cJSON_AddStringToObject(root, "mount_point", SDCARD_MOUNT_POINT);
                if (sdcard_) {
                    cJSON_AddStringToObject(root, "name", sdcard_->cid.name);
                    cJSON_AddNumberToObject(root, "capacity_mb",
                        (double)((uint64_t)sdcard_->csd.capacity * sdcard_->csd.sector_size) / (1024.0 * 1024.0));
                    cJSON_AddNumberToObject(root, "sector_size", sdcard_->csd.sector_size);
                }
                return root;
            });

        mcp.AddTool("self.hardware.get_xiao_status",
            "Read status line from external XIAO camera/servo/LED/proximity bridge.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                cJSON* root = cJSON_CreateObject();
                cJSON_AddBoolToObject(root, "ready", servo_controller_ != nullptr);
                if (!servo_controller_) {
                    return root;
                }
                std::string response;
                bool ok = servo_controller_->RequestStatus(response);
                cJSON_AddBoolToObject(root, "ok", ok);
                if (ok) {
                    cJSON_AddStringToObject(root, "response", response.c_str());
                } else {
                    cJSON_AddStringToObject(root, "error", "timeout");
                }
                return root;
            });

        mcp.AddTool("self.hardware.get_proximity",
            "Read VL53L0X proximity distance from the external XIAO bridge.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                cJSON* root = cJSON_CreateObject();
                cJSON_AddBoolToObject(root, "ready", servo_controller_ != nullptr);
                if (!servo_controller_) {
                    return root;
                }
                std::string response;
                bool ok = servo_controller_->RequestDistance(response);
                cJSON_AddBoolToObject(root, "ok", ok);
                if (ok) {
                    cJSON_AddStringToObject(root, "response", response.c_str());
                    int distance_mm = -1;
                    if (sscanf(response.c_str(), "DIST %d", &distance_mm) == 1) {
                        cJSON_AddNumberToObject(root, "distance_mm", distance_mm);
                    }
                } else {
                    cJSON_AddStringToObject(root, "error", "timeout");
                }
                return root;
            });

        mcp.AddTool("self.sensor.get_radar",
            "Read DFRobot C1001 24GHz radar data from the XIAO bridge: human presence, motion direction, distance.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                cJSON* root = cJSON_CreateObject();
                cJSON_AddBoolToObject(root, "ready", servo_controller_ != nullptr);
                if (!servo_controller_) {
                    return root;
                }
                std::string response;
                bool ok = servo_controller_->RequestRadar(response);
                cJSON_AddBoolToObject(root, "ok", ok);
                if (ok) {
                    cJSON_AddStringToObject(root, "response", response.c_str());
                    int presence = -1, motion = -1, range = -1;
                    if (sscanf(response.c_str(), "RADAR presence=%d motion=%d range=%d",
                               &presence, &motion, &range) == 3) {
                        cJSON_AddBoolToObject(root, "human_present", presence == 1);
                        // motion: 0=none 1=still 2=active
                        const char* motion_str = motion == 0 ? "none" :
                                                 motion == 1 ? "still" : "active";
                        cJSON_AddStringToObject(root, "motion", motion_str);
                        cJSON_AddNumberToObject(root, "moving_range_cm", range);
                    }
                } else {
                    cJSON_AddStringToObject(root, "error", "timeout");
                }
                return root;
            });

        mcp.AddTool("self.sensor.get_distance",
            "Read VL53L0X distance from the external XIAO bridge. Alias for proximity.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                cJSON* root = cJSON_CreateObject();
                cJSON_AddBoolToObject(root, "ready", servo_controller_ != nullptr);
                if (!servo_controller_) {
                    return root;
                }
                std::string response;
                bool ok = servo_controller_->RequestDistance(response);
                cJSON_AddBoolToObject(root, "ok", ok);
                if (ok) {
                    cJSON_AddStringToObject(root, "response", response.c_str());
                    int distance_mm = -1;
                    if (sscanf(response.c_str(), "DIST %d", &distance_mm) == 1) {
                        cJSON_AddNumberToObject(root, "distance_mm", distance_mm);
                    }
                } else {
                    cJSON_AddStringToObject(root, "error", "timeout");
                }
                return root;
            });

        mcp.AddTool("self.camera.get_status",
            "Read XIAO camera bridge status: camera, VL53L0X, stream and servo state.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                cJSON* root = cJSON_CreateObject();
                cJSON_AddBoolToObject(root, "ready", servo_controller_ != nullptr);
                if (!servo_controller_) {
                    return root;
                }
                std::string response;
                bool ok = servo_controller_->RequestStatus(response);
                cJSON_AddBoolToObject(root, "ok", ok);
                if (!ok) {
                    cJSON_AddStringToObject(root, "error", "timeout");
                    return root;
                }
                cJSON_AddStringToObject(root, "response", response.c_str());
                int camera = 0, vl53 = 0, c1001 = 0, sd = 0;
                int servos = 0, led = 0, stream = 0, yaw = 0, pitch = 0, wifi = 0;
                // New format: STATUS camera=%d vl53=%d c1001=%d sd=%d servos=%d led=%d stream=%d yaw=%d pitch=%d wifi=%d
                int parsed = sscanf(response.c_str(),
                    "STATUS camera=%d vl53=%d c1001=%d sd=%d servos=%d led=%d stream=%d yaw=%d pitch=%d wifi=%d",
                    &camera, &vl53, &c1001, &sd, &servos, &led, &stream, &yaw, &pitch, &wifi);
                if (parsed >= 2) {
                    cJSON_AddBoolToObject(root, "camera_ready",  camera  != 0);
                    cJSON_AddBoolToObject(root, "vl53_ready",    vl53    != 0);
                    cJSON_AddBoolToObject(root, "c1001_ready",   c1001   != 0);
                    cJSON_AddBoolToObject(root, "sd_ok",         sd      != 0);
                    cJSON_AddBoolToObject(root, "servos_ok",     servos  != 0);
                    cJSON_AddBoolToObject(root, "led_ok",        led     != 0);
                    cJSON_AddBoolToObject(root, "stream_enabled",stream  != 0);
                    cJSON_AddNumberToObject(root, "yaw",   yaw);
                    cJSON_AddNumberToObject(root, "pitch", pitch);
                    cJSON_AddBoolToObject(root, "wifi",    wifi != 0);
                }
                return root;
            });

        mcp.AddTool("self.camera.capture_frame",
            "Capture one JPEG frame from the external XIAO camera bridge.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                cJSON* error_root = nullptr;
                if (!servo_controller_) {
                    error_root = cJSON_CreateObject();
                    cJSON_AddBoolToObject(error_root, "ok", false);
                    cJSON_AddStringToObject(error_root, "error", "XIAO bridge not ready");
                    return error_root;
                }

                std::string jpeg;
                std::string error;
                uint32_t frame_id = 0;
                bool ok = servo_controller_->RequestFrame(jpeg, frame_id, error);
                if (!ok) {
                    error_root = cJSON_CreateObject();
                    cJSON_AddBoolToObject(error_root, "ok", false);
                    cJSON_AddStringToObject(error_root, "error", error.c_str());
                    return error_root;
                }

                return new ImageContent("image/jpeg", jpeg);
            });

        mcp.AddTool("self.camera.stream_off",
            "Stop XIAO camera UART stream mode.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                return servo_controller_ && servo_controller_->CameraStreamOff();
            });
    }

    // Play "/sdcard/<name>_emotion.mp4" over the live UI. LVGL is paused for
    // the whole clip and repaints the screen afterwards. Runs on the main
    // loop (scheduled by the BLE service), so voice handling is briefly
    // blocked — emotion clips should stay short (a few seconds).
    void PlayEmotionVideo(const std::string& name) {
        if (!sdcard_mounted_) {
            ESP_LOGW(TAG, "Emotion video: SD card is not mounted");
            return;
        }
        const std::string path = std::string(SDCARD_MOUNT_POINT "/") + name + "_emotion.mp4";
        if (!lvgl_port_lock(2000)) {
            ESP_LOGW(TAG, "Emotion video: LVGL lock timeout");
            return;
        }
        // The internal DMA heap is exhausted at runtime, so the player
        // borrows LVGL's draw buffer for its stripe transfers — LVGL is
        // locked for the whole clip and repaints the screen afterwards.
        lv_display_t* disp = lv_display_get_default();
        lv_draw_buf_t* draw_buf = disp ? lv_display_get_buf_active(disp) : nullptr;
        if (draw_buf && draw_buf->data && draw_buf->data_size > 0) {
            startup_play_mp4_with_blit_buffer(panel_handle_, DISPLAY_WIDTH, DISPLAY_HEIGHT,
                                              path.c_str(), draw_buf->data, draw_buf->data_size);
        } else {
            startup_play_mp4(panel_handle_, DISPLAY_WIDTH, DISPLAY_HEIGHT, path.c_str());
        }
        lv_obj_invalidate(lv_scr_act());
        lvgl_port_unlock();
    }

public:
    WaveshareEsp32s3TouchAMOLED2inch06() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();
        InitializeAxp2101();
        InitializeSdCard();          // must be before splash/audio
        InitializeSpi();
        InitializeLcdPanel();        // hardware panel only, no LVGL yet

        // Intro video from SD card root, then optional startup chime.
        // Skip only self-inflicted restarts (panic, watchdog, esp_restart,
        // deep-sleep wake) so a crash can never replay the intro; any kind of
        // cold start (power button, EN reset, USB, brownout) plays it.
        const esp_reset_reason_t reset_reason = esp_reset_reason();
        const bool soft_restart = reset_reason == ESP_RST_SW ||
                                  reset_reason == ESP_RST_PANIC ||
                                  reset_reason == ESP_RST_INT_WDT ||
                                  reset_reason == ESP_RST_TASK_WDT ||
                                  reset_reason == ESP_RST_WDT ||
                                  reset_reason == ESP_RST_DEEPSLEEP;
        ESP_LOGI(TAG, "Reset reason %d, intro %s", (int)reset_reason,
                 soft_restart ? "skipped" : "enabled");
        if (!soft_restart) {
            startup_play_mp4(panel_handle_, DISPLAY_WIDTH, DISPLAY_HEIGHT, STARTUP_INTRO_MP4);
            startup_play_wav(i2c_bus_, SDCARD_MOUNT_POINT "/assets/load.wav");
        }

        InitializeDisplay();         // LVGL + CatDisplay + backlight
        InitializeButtons();
        InitializePowerSaveTimer();
        InitializeTouch();
        InitializeImu();
        InitializeServoController();
        InitializeMotionInteraction();
        homebot_ble_.SetXiaoController(servo_controller_);
        homebot_ble_.SetVideoPlayer([this](const std::string& name) {
            PlayEmotionVideo(name);
        });
        RegisterMcpTools();
        RegisterHardwareMcpTools();
        homebot_ble_.Start();
    }

    ~WaveshareEsp32s3TouchAMOLED2inch06() {
        if (motion_task_) {
            vTaskDelete(motion_task_);
            motion_task_ = nullptr;
        }
        SetVibration(false);
        if (TOUCH_INT_PIN != GPIO_NUM_NC) {
            gpio_isr_handler_remove(TOUCH_INT_PIN);
        }
        if (touch_task_) {
            vTaskDelete(touch_task_);
            touch_task_ = nullptr;
        }
        heap_caps_free(touch_stack_);  touch_stack_ = nullptr;
        heap_caps_free(touch_tcb_);    touch_tcb_   = nullptr;
        if (touch_timer_) {
            esp_timer_stop(touch_timer_);
            esp_timer_delete(touch_timer_);
            touch_timer_ = nullptr;
        }
        if (touch_ && touch_->del) {
            touch_->del(touch_);
            touch_ = nullptr;
        }
        if (ft3168_dev_) {
            i2c_master_bus_rm_device(ft3168_dev_);
            ft3168_dev_ = nullptr;
        }
        if (sdcard_mounted_ && sdcard_) {
            esp_vfs_fat_sdcard_unmount(SDCARD_MOUNT_POINT, sdcard_);
            sdcard_ = nullptr;
            sdcard_mounted_ = false;
        }
        if (serial_console_task_) {
            vTaskDelete(serial_console_task_);
            serial_console_task_ = nullptr;
        }
        delete servo_controller_;
        servo_controller_ = nullptr;
    }

    // ── Board API ──

    virtual void StartNetwork() override {
        WifiBoard::StartNetwork();
        if (servo_controller_) {
            servo_controller_->InitEspNow();
        }
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            i2c_bus_,
            AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,   AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN,  AUDIO_CODEC_ES8311_ADDR, AUDIO_CODEC_ES7210_ADDR,
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        if (display_adapter_) return display_adapter_;
        if (lvgl_display_)    return lvgl_display_;
        static NoDisplay nd;
        return &nd;
    }

    virtual Backlight* GetBacklight() override {
        return backlight_;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        static bool last_dis = false;
        static uint32_t read_failures = 0;
        esp_err_t ret;
        {
            std::lock_guard<std::mutex> lock(i2c_mutex_);
            ret = pmic_->GetBatteryStatus(level, charging, discharging);
        }
        if (ret != ESP_OK) {
            if (read_failures++ == 0 || read_failures % 20 == 0) {
                ESP_LOGW(TAG, "Battery status read skipped: %s", esp_err_to_name(ret));
            }
            return false;
        }
        read_failures = 0;
        if (discharging != last_dis) {
            power_save_timer_->SetEnabled(discharging);
            last_dis = discharging;
        }
        return true;
    }

    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) power_save_timer_->WakeUp();
        WifiBoard::SetPowerSaveMode(enabled);
    }
};

DECLARE_BOARD(WaveshareEsp32s3TouchAMOLED2inch06);
