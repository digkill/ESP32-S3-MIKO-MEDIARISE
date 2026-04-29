#include "wifi_board.h"
#include "display/lcd_display.h"
#include "esp_lcd_sh8601.h"

#include "codecs/box_audio_codec.h"
#include "application.h"
#include "button.h"
#include "led/single_led.h"
#include "mcp_server.h"
#include "config.h"
#include "power_save_timer.h"
#include "axp2101.h"
#include "i2c_device.h"
#include "servo_controller.h"
#include "cat_display.h"
#include <wifi_station.h>
#include <qmi8658.h>

#include <esp_log.h>
#include <esp_err.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_vendor.h>
#include <esp_lcd_touch.h>
#include <esp_lcd_touch_ft5x06.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include <driver/sdmmc_host.h>
#include <esp_vfs_fat.h>
#include <esp_lvgl_port.h>
#include <sdmmc_cmd.h>
#include "settings.h"

#include <sstream>
#include <vector>
#include <mutex>
#include <cstring>

#define TAG "WaveshareAMOLED2_06"

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
        }
        cat_->Update();
    }

    void SetPowerSaveMode(bool on) override {
        if (!cat_) return;
        if (on) {
            cat_->FillScreen(0x0000);
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
    CatDisplay*       cat_          = nullptr;
    CustomLcdDisplay* lvgl_display_ = nullptr;
};

// ── Board class ───────────────────────────────────────────────────────────────

class WaveshareEsp32s3TouchAMOLED2inch06 : public WifiBoard {
private:
    i2c_master_bus_handle_t   i2c_bus_         = nullptr;
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
    esp_lcd_touch_handle_t    touch_           = nullptr;
    esp_timer_handle_t        touch_timer_     = nullptr;
    esp_lcd_panel_io_handle_t touch_io_        = nullptr;
    sdmmc_card_t*             sdcard_          = nullptr;
    bool                      sdcard_mounted_  = false;
    qmi8658_dev_t             imu_             = {};
    bool                      imu_ready_       = false;

    // ── Power save ──

    void InitializePowerSaveTimer() {
        power_save_timer_ = new PowerSaveTimer(-1, 60, 300);
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(20);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() { pmic_->PowerOff(); });
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

    // ── Display ──

    void InitializeDisplay() {
        // 1. Panel IO (QSPI)
        esp_lcd_panel_io_spi_config_t io_config =
            SH8601_PANEL_IO_QSPI_CONFIG(EXAMPLE_PIN_NUM_LCD_CS, nullptr, nullptr);
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io_));

        // 2. Panel driver
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

        // 3. LVGL display (initialises LVGL, handles flushing to SH8601)
        lvgl_display_ = new CustomLcdDisplay(
            panel_io_, panel,
            DISPLAY_WIDTH, DISPLAY_HEIGHT,
            DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
            DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);

        // 4. Cat animation — LVGL canvas covering the full screen.
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

        // 5. Adapter routes voice-assistant calls to cat
        display_adapter_ = new CatDisplayAdapter(cat_display_, lvgl_display_);

        // 6. Backlight
        backlight_ = new CustomBacklight(panel_io_);
        backlight_->SetBrightness(100, true);
    }

    // ── Touch ──

    void InitializeTouch() {
        ESP_LOGI(TAG, "Init FT5x06 touch");
        if (!ProbeI2cDevice(ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS)) {
            ESP_LOGW(TAG, "FT5x06 touch not found at 0x%02X", ESP_LCD_TOUCH_IO_I2C_FT5x06_ADDRESS);
            return;
        }

        esp_lcd_touch_config_t tp_cfg = {
            .x_max = DISPLAY_WIDTH,
            .y_max = DISPLAY_HEIGHT,
            .rst_gpio_num = TOUCH_RST_PIN,
            .int_gpio_num = TOUCH_INT_PIN,
            .levels = {
                .reset = 0,
                .interrupt = 0,
            },
            .flags = {
                .swap_xy = DISPLAY_SWAP_XY,
                .mirror_x = DISPLAY_MIRROR_X,
                .mirror_y = DISPLAY_MIRROR_Y,
            },
        };
        esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_FT5x06_CONFIG();
        tp_io_config.scl_speed_hz = 400000;

        esp_err_t ret = esp_lcd_new_panel_io_i2c(i2c_bus_, &tp_io_config, &touch_io_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Touch I2C IO init failed: %s", esp_err_to_name(ret));
            return;
        }
        ret = esp_lcd_touch_new_i2c_ft5x06(touch_io_, &tp_cfg, &touch_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "FT5x06 init failed: %s", esp_err_to_name(ret));
            touch_ = nullptr;
            return;
        }

        const esp_timer_create_args_t timer_args = {
            .callback = &WaveshareEsp32s3TouchAMOLED2inch06::TouchTimerCallback,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "touch_poll",
            .skip_unhandled_events = true,
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &touch_timer_));
        ESP_ERROR_CHECK(esp_timer_start_periodic(touch_timer_, 50 * 1000));
    }

    static void TouchTimerCallback(void* arg) {
        auto* board = static_cast<WaveshareEsp32s3TouchAMOLED2inch06*>(arg);
        if (!board || !board->touch_) {
            return;
        }

        static bool was_touched = false;
        static int64_t touch_start_us = 0;
        esp_lcd_touch_point_data_t point = {};
        uint8_t points = 0;

        if (esp_lcd_touch_read_data(board->touch_) != ESP_OK) {
            return;
        }
        bool touched = esp_lcd_touch_get_data(board->touch_, &point, &points, 1) == ESP_OK && points > 0;
        int64_t now = esp_timer_get_time();

        if (touched && !was_touched) {
            was_touched = true;
            touch_start_us = now;
            if (board->power_save_timer_) {
                board->power_save_timer_->WakeUp();
            }
        } else if (!touched && was_touched) {
            was_touched = false;
            int64_t duration_ms = (now - touch_start_us) / 1000;
            if (duration_ms > 40 && duration_ms < 1200) {
                Application::GetInstance().ToggleChatState();
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

    // ── Button ──

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting &&
                !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
                return;
            }
            app.ToggleChatState();
            power_save_timer_->WakeUp();
        });
#if CONFIG_USE_DEVICE_AEC
        boot_button_.OnDoubleClick([this]() {
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
            "Управление сервоприводом (1-10), угол 0-180°",
            PropertyList({
                Property("servo_num", kPropertyTypeInteger, 1,  1, 10),
                Property("angle",     kPropertyTypeInteger, 90, 0, 180),
            }),
            [this](const PropertyList& p) -> ReturnValue {
                return servo_controller_ &&
                       servo_controller_->SetServoAngle(
                           p["servo_num"].value<int>(), p["angle"].value<int>());
            });

        mcp.AddTool("self.robot.set_pose",
            "Поза робота: home/reset, wave/wave_hand, dance/dancing, "
            "greet/greeting, sad/sad_pose, happy/happy_pose",
            PropertyList({ Property("pose", kPropertyTypeString) }),
            [this](const PropertyList& p) -> ReturnValue {
                return servo_controller_ &&
                       servo_controller_->SetPose(p["pose"].value<std::string>());
            });

        mcp.AddTool("self.robot.move_servos",
            "Несколько сервоприводов: 'S1:45,S2:120'",
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
                esp_err_t ret = qmi8658_read_sensor_data(&imu_, &data);
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
            "Read the FT5x06 touchscreen state.",
            PropertyList(),
            [this](const PropertyList&) -> ReturnValue {
                cJSON* root = cJSON_CreateObject();
                cJSON_AddBoolToObject(root, "ready", touch_ != nullptr);
                if (!touch_) {
                    return root;
                }
                esp_lcd_touch_point_data_t point = {};
                uint8_t points = 0;
                esp_err_t ret = esp_lcd_touch_read_data(touch_);
                cJSON_AddStringToObject(root, "read_status", esp_err_to_name(ret));
                bool touched = ret == ESP_OK && esp_lcd_touch_get_data(touch_, &point, &points, 1) == ESP_OK && points > 0;
                cJSON_AddBoolToObject(root, "touched", touched);
                cJSON_AddNumberToObject(root, "points", points);
                if (touched) {
                    cJSON_AddNumberToObject(root, "x", point.x);
                    cJSON_AddNumberToObject(root, "y", point.y);
                    cJSON_AddNumberToObject(root, "strength", point.strength);
                }
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
    }

public:
    WaveshareEsp32s3TouchAMOLED2inch06() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializeCodecI2c();
        InitializeAxp2101();
        InitializeSpi();
        InitializeDisplay();
        InitializeTouch();
        InitializeButtons();
        InitializePowerSaveTimer();
        InitializeSdCard();
        InitializeImu();
        InitializeServoController();
        RegisterMcpTools();
        RegisterHardwareMcpTools();
    }

    ~WaveshareEsp32s3TouchAMOLED2inch06() {
        if (touch_timer_) {
            esp_timer_stop(touch_timer_);
            esp_timer_delete(touch_timer_);
            touch_timer_ = nullptr;
        }
        if (touch_ && touch_->del) {
            touch_->del(touch_);
            touch_ = nullptr;
        }
        if (sdcard_mounted_ && sdcard_) {
            esp_vfs_fat_sdcard_unmount(SDCARD_MOUNT_POINT, sdcard_);
            sdcard_ = nullptr;
            sdcard_mounted_ = false;
        }
        delete servo_controller_;
        servo_controller_ = nullptr;
    }

    // ── Board API ──

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
        charging    = pmic_->IsCharging();
        discharging = pmic_->IsDischarging();
        if (discharging != last_dis) {
            power_save_timer_->SetEnabled(discharging);
            last_dis = discharging;
        }
        level = pmic_->GetBatteryLevel();
        return true;
    }

    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) power_save_timer_->WakeUp();
        WifiBoard::SetPowerSaveMode(enabled);
    }
};

DECLARE_BOARD(WaveshareEsp32s3TouchAMOLED2inch06);
