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
#include "cat_display.h"
#include "servo_controller.h"
#include <wifi_station.h>

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_vendor.h>
#include <driver/i2c_master.h>
#include <driver/spi_master.h>
#include "settings.h"
#include "homebot_ble_service.h"

#include <esp_lvgl_port.h>
#include <lvgl.h>
#include <cstdio>
#include <sstream>
#include <vector>

#define TAG "WaveshareEsp32c6TouchAMOLED2inch06"

class Pmic : public Axp2101 {
public:
    Pmic(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : Axp2101(i2c_bus, addr) {
        WriteReg(0x22, 0b110); // PWRON > OFFLEVEL as POWEROFF Source enable
        WriteReg(0x27, 0x10);  // hold 4s to power off

        // Disable All DCs but DC1
        WriteReg(0x80, 0x01);
        // Disable All LDOs
        WriteReg(0x90, 0x00);
        WriteReg(0x91, 0x00);

        // Set DC1 to 3.3V
        WriteReg(0x82, (3300 - 1500) / 100);

        // Set ALDO1 to 3.3V
        WriteReg(0x92, (3300 - 500) / 100);
        WriteReg(0x93, (3300 - 500) / 100);

        // Enable ALDO1(MIC)
        WriteReg(0x90, 0x03);

        WriteReg(0x64, 0x02); // CV charger voltage setting to 4.1V

        WriteReg(0x61, 0x02); // set Main battery precharge current to 50mA
        WriteReg(0x62, 0x0A); // set Main battery charger current to 400mA ( 0x08-200mA, 0x09-300mA, 0x0A-400mA )
        WriteReg(0x63, 0x01); // set Main battery term charge current to 25mA
    }
};

#define LCD_OPCODE_WRITE_CMD (0x02ULL)
#define LCD_OPCODE_READ_CMD (0x03ULL)
#define LCD_OPCODE_WRITE_COLOR (0x32ULL)

static const sh8601_lcd_init_cmd_t vendor_specific_init[] = {
    // set display to qspi mode
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

// 在waveshare_amoled_2_06类之前添加新的显示类
class CustomLcdDisplay : public SpiLcdDisplay {
public:
    static void rounder_event_cb(lv_event_t* e) {
        lv_area_t* area = (lv_area_t* )lv_event_get_param(e);
        uint16_t x1 = area->x1;
        uint16_t x2 = area->x2;

        uint16_t y1 = area->y1;
        uint16_t y2 = area->y2;

        // round the start of coordinate down to the nearest 2M number
        area->x1 = (x1 >> 1) << 1;
        area->y1 = (y1 >> 1) << 1;
        // round the end of coordinate up to the nearest 2N+1 number
        area->x2 = ((x2 >> 1) << 1) + 1;
        area->y2 = ((y2 >> 1) << 1) + 1;
    }

    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle,
                     esp_lcd_panel_handle_t panel_handle,
                     int width,
                     int height,
                     int offset_x,
                     int offset_y,
                     bool mirror_x,
                     bool mirror_y,
                     bool swap_xy)
        : SpiLcdDisplay(io_handle, panel_handle,
                        width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy) {
        DisplayLockGuard lock(this);
        lv_obj_set_style_pad_left(status_bar_, LV_HOR_RES*  0.1, 0);
        lv_obj_set_style_pad_right(status_bar_, LV_HOR_RES*  0.1, 0);
        lv_display_add_event_cb(display_, rounder_event_cb, LV_EVENT_INVALIDATE_AREA, NULL);
    }
};

class CustomBacklight : public Backlight {
public:
    CustomBacklight(esp_lcd_panel_io_handle_t panel_io) : Backlight(), panel_io_(panel_io) {}

protected:
    esp_lcd_panel_io_handle_t panel_io_;

    virtual void SetBrightnessImpl(uint8_t brightness) override {
        auto display = Board::GetInstance().GetDisplay();
        DisplayLockGuard lock(display);
        uint8_t data[1] = {((uint8_t)((255*  brightness) / 100))};
        int lcd_cmd = 0x51;
        lcd_cmd &= 0xff;
        lcd_cmd <<= 8;
        lcd_cmd |= LCD_OPCODE_WRITE_CMD << 24;
        esp_lcd_panel_io_tx_param(panel_io_, lcd_cmd, &data, sizeof(data));
    }
};

class CatDisplayAdapter : public Display {
public:
    CatDisplayAdapter(CatDisplay* cat, CustomLcdDisplay* lvgl_display)
        : cat_(cat), lvgl_display_(lvgl_display) {
        width_ = DISPLAY_WIDTH;
        height_ = DISPLAY_HEIGHT;
    }

    void SetStatus(const char* status) override {
        if (cat_) {
            cat_->SetStateFromStatus(status);
        }
    }

    void SetEmotion(const char* emotion) override {
        if (cat_) {
            cat_->SetStateFromEmotion(emotion);
        }
    }

    void SetChatMessage(const char* role, const char* content) override {
        if (content && *content) {
            ESP_LOGI(TAG, "Chat [%s]: %.80s", role ? role : "?", content);
        }
    }

    void UpdateStatusBar(bool update_all = false) override {
        if (!cat_) {
            return;
        }
        int level = 0;
        bool charging = false;
        bool discharging = false;
        if (Board::GetInstance().GetBatteryLevel(level, charging, discharging)) {
            cat_->SetBatteryStatus(level, charging);
        }
        cat_->Update();
    }

    void SetPowerSaveMode(bool on) override {
        if (!cat_) {
            return;
        }
        if (on) {
            cat_->SetState(CatDisplay::State::SLEEPING);
            cat_->Redraw();
        } else {
            cat_->SetState(CatDisplay::State::IDLE);
            cat_->Redraw();
        }
    }

protected:
    bool Lock(int timeout_ms = 0) override {
        return lvgl_port_lock(timeout_ms <= 0 ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms));
    }

    void Unlock() override {
        lvgl_port_unlock();
    }

private:
    CatDisplay* cat_ = nullptr;
    CustomLcdDisplay* lvgl_display_ = nullptr;
};

class WaveshareEsp32c6TouchAMOLED2inch06 : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_;
    Pmic* pmic_ = nullptr;
    Button boot_button_;
    CustomLcdDisplay* lvgl_display_ = nullptr;
    CatDisplay* cat_display_ = nullptr;
    CatDisplayAdapter* display_adapter_ = nullptr;
    CustomBacklight* backlight_;
    PowerSaveTimer* power_save_timer_;
    ServoController* servo_controller_ = nullptr;
    HomeBotBleService homebot_ble_;

    void InitializePowerSaveTimer() {
        constexpr int kSecondsToSleep = 30 * 60;
        power_save_timer_ = new PowerSaveTimer(-1, kSecondsToSleep, -1);
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(10);
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
        });
        power_save_timer_->SetEnabled(true);
    }

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_));
    }

    void InitializeAxp2101() {
        ESP_LOGI(TAG, "Init AXP2101");
        pmic_ = new Pmic(i2c_bus_, 0x34);
    }

    void InitializeSpi() {
        spi_bus_config_t buscfg = {};
        buscfg.sclk_io_num = EXAMPLE_PIN_NUM_LCD_PCLK;
        buscfg.data0_io_num = EXAMPLE_PIN_NUM_LCD_DATA0;
        buscfg.data1_io_num = EXAMPLE_PIN_NUM_LCD_DATA1;
        buscfg.data2_io_num = EXAMPLE_PIN_NUM_LCD_DATA2;
        buscfg.data3_io_num = EXAMPLE_PIN_NUM_LCD_DATA3;
        buscfg.max_transfer_sz = DISPLAY_WIDTH*  DISPLAY_HEIGHT*  sizeof(uint16_t);
        buscfg.flags = SPICOMMON_BUSFLAG_QUAD;
        ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO));
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });

#if CONFIG_USE_DEVICE_AEC
        boot_button_.OnDoubleClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateIdle) {
                app.SetAecMode(app.GetAecMode() == kAecOff ? kAecOnDeviceSide : kAecOff);
            }
        });
#endif
    }

    void InitializeSH8601Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        // 液晶屏控制IO初始化
        ESP_LOGD(TAG, "Install panel IO");
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = EXAMPLE_PIN_NUM_LCD_CS;
        io_config.dc_gpio_num = static_cast<gpio_num_t>(-1);
        io_config.spi_mode = 0;
        io_config.pclk_hz = 40 * 1000 * 1000;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 32;
        io_config.lcd_param_bits = 8;
        io_config.flags.quad_mode = true;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI2_HOST, &io_config, &panel_io));

        // 初始化液晶屏驱动芯片
        ESP_LOGD(TAG, "Install LCD driver");
        const sh8601_vendor_config_t vendor_config = {
            .init_cmds = &vendor_specific_init[0],
            .init_cmds_size = sizeof(vendor_specific_init) / sizeof(sh8601_lcd_init_cmd_t),
            .flags = {
                .use_qspi_interface = 1,
            }};

        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
        panel_config.bits_per_pixel = 16;
        panel_config.vendor_config = (void* )&vendor_config;
        ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(panel_io, &panel_config, &panel));
        esp_lcd_panel_set_gap(panel, 0x16, 0);
        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, false);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);
        lvgl_display_ = new CustomLcdDisplay(panel_io, panel,
                                             DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y,
                                             DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);

        // ESP32-C6 has no configured external PSRAM. Render the cat into a
        // compact source canvas and let LVGL scale it across the full panel.
        static constexpr int kCatCanvasWidth = 144;
        static constexpr int kCatCanvasHeight = 176;
        static constexpr int kCatScaleX =
            (DISPLAY_WIDTH * LV_SCALE_NONE + kCatCanvasWidth - 1) / kCatCanvasWidth;
        static constexpr int kCatScaleY =
            (DISPLAY_HEIGHT * LV_SCALE_NONE + kCatCanvasHeight - 1) / kCatCanvasHeight;
        static constexpr float kCatPresentationScale =
            ((kCatScaleX + kCatScaleY) * 0.5f) / LV_SCALE_NONE;
        lv_obj_t* canvas = nullptr;
        if (lvgl_port_lock(1000)) {
            lv_obj_clean(lv_scr_act());
            lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x050508), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);

            const size_t bytes = kCatCanvasWidth * kCatCanvasHeight * sizeof(uint16_t);
            auto* buffer = static_cast<uint16_t*>(
                heap_caps_malloc(bytes, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
            if (buffer != nullptr) {
                canvas = lv_canvas_create(lv_scr_act());
                lv_canvas_set_buffer(canvas, buffer, kCatCanvasWidth, kCatCanvasHeight,
                                     LV_COLOR_FORMAT_RGB565);
                lv_image_set_pivot(canvas, kCatCanvasWidth / 2, kCatCanvasHeight / 2);
                lv_image_set_scale_x(canvas, kCatScaleX);
                lv_image_set_scale_y(canvas, kCatScaleY);
                lv_obj_center(canvas);
            } else {
                ESP_LOGE(TAG, "Unable to allocate %u bytes for cat animation", bytes);
            }
            lvgl_port_unlock();
        }

        if (canvas != nullptr) {
            cat_display_ = new CatDisplay(kCatCanvasWidth, kCatCanvasHeight, canvas, kCatPresentationScale);
            if (!cat_display_->Init()) {
                delete cat_display_;
                cat_display_ = nullptr;
            }
        }
        display_adapter_ = new CatDisplayAdapter(cat_display_, lvgl_display_);
        backlight_ = new CustomBacklight(panel_io);
        backlight_->RestoreBrightness();
    }

    void InitializeServoController() {
        servo_controller_ = new ServoController();
        if (!servo_controller_->Init()) {
            ESP_LOGE(TAG, "XIAO servo/LED bridge init failed");
            delete servo_controller_;
            servo_controller_ = nullptr;
        }
    }

    void InitializeTools() {
        auto &mcp_server = McpServer::GetInstance();
        mcp_server.AddTool("self.system.reconfigure_wifi",
            "Reboot the device and enter WiFi configuration mode.\n"
            "**CAUTION** You must ask the user to confirm this action.",
            PropertyList(), [this](const PropertyList& properties) {
                ResetWifiConfiguration();
                return true;
            });

        if (servo_controller_ == nullptr) {
            return;
        }

        mcp_server.AddTool("self.robot.set_servo",
            "Control XIAO head servo: 1=yaw, 2=pitch, angle 40-80.",
            PropertyList({
                Property("servo_num", kPropertyTypeInteger, 1, 1, 2),
                Property("angle", kPropertyTypeInteger, SERVO_HOME_ANGLE, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE),
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                return servo_controller_->SetServoAngle(
                    properties["servo_num"].value<int>(), properties["angle"].value<int>());
            });

        mcp_server.AddTool("self.robot.set_pose",
            "Set head pose: home, left, right, up, down, nod, shake, dance, happy or sad.",
            PropertyList({Property("pose", kPropertyTypeString)}),
            [this](const PropertyList& properties) -> ReturnValue {
                return servo_controller_->SetPose(properties["pose"].value<std::string>());
            });

        mcp_server.AddTool("self.robot.move_servos",
            "Move multiple head servos, for example 'S1:45,S2:120'.",
            PropertyList({Property("servos", kPropertyTypeString)}),
            [this](const PropertyList& properties) -> ReturnValue {
                std::vector<std::pair<int, int>> commands;
                std::istringstream input(properties["servos"].value<std::string>());
                std::string token;
                while (std::getline(input, token, ',')) {
                    int servo = 0;
                    int angle = 0;
                    if (std::sscanf(token.c_str(), " S%d:%d", &servo, &angle) != 2 &&
                        std::sscanf(token.c_str(), " s%d:%d", &servo, &angle) != 2) {
                        return false;
                    }
                    commands.emplace_back(servo, angle);
                }
                return !commands.empty() && servo_controller_->SetMultipleServos(commands);
            });

        mcp_server.AddTool("self.robot.set_led",
            "Set XIAO LED ring RGB color from 0 to 255.",
            PropertyList({
                Property("r", kPropertyTypeInteger, XIAO_LED_DEFAULT_R, 0, 255),
                Property("g", kPropertyTypeInteger, XIAO_LED_DEFAULT_G, 0, 255),
                Property("b", kPropertyTypeInteger, XIAO_LED_DEFAULT_B, 0, 255),
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                return servo_controller_->SetLed(
                    properties["r"].value<int>(), properties["g"].value<int>(), properties["b"].value<int>());
            });

        mcp_server.AddTool("self.robot.led_default", "Set the default XIAO LED color.",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                return servo_controller_->SetDefaultLed();
            });
        mcp_server.AddTool("self.robot.led_off", "Turn the XIAO LED ring off.",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                return servo_controller_->LedOff();
            });
        mcp_server.AddTool("self.robot.led_test", "Run the XIAO LED test sequence.",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                return servo_controller_->LedTest();
            });

        mcp_server.AddTool("self.hardware.get_xiao_status",
            "Read status from the external XIAO servo/LED/proximity bridge.",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                cJSON* root = cJSON_CreateObject();
                std::string response;
                const bool ok = servo_controller_->RequestStatus(response);
                cJSON_AddBoolToObject(root, "ok", ok);
                if (ok) {
                    cJSON_AddStringToObject(root, "response", response.c_str());
                }
                return root;
            });

        mcp_server.AddTool("self.hardware.get_proximity",
            "Read distance from the XIAO proximity bridge.",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                cJSON* root = cJSON_CreateObject();
                std::string response;
                const bool ok = servo_controller_->RequestDistance(response);
                cJSON_AddBoolToObject(root, "ok", ok);
                if (ok) {
                    cJSON_AddStringToObject(root, "response", response.c_str());
                    int distance = -1;
                    if (std::sscanf(response.c_str(), "DIST %d", &distance) == 1) {
                        cJSON_AddNumberToObject(root, "distance_mm", distance);
                    }
                }
                return root;
            });

        mcp_server.AddTool("self.sensor.get_distance",
            "Read distance from the external XIAO bridge.",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                cJSON* root = cJSON_CreateObject();
                std::string response;
                const bool ok = servo_controller_->RequestDistance(response);
                cJSON_AddBoolToObject(root, "ok", ok);
                if (ok) {
                    cJSON_AddStringToObject(root, "response", response.c_str());
                    int distance = -1;
                    if (std::sscanf(response.c_str(), "DIST %d", &distance) == 1) {
                        cJSON_AddNumberToObject(root, "distance_mm", distance);
                    }
                }
                return root;
            });

        mcp_server.AddTool("self.camera.get_status",
            "Read camera, proximity and servo state from the XIAO bridge.",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                cJSON* root = cJSON_CreateObject();
                std::string response;
                const bool ok = servo_controller_->RequestStatus(response);
                cJSON_AddBoolToObject(root, "ok", ok);
                if (ok) {
                    cJSON_AddStringToObject(root, "response", response.c_str());
                }
                return root;
            });

        mcp_server.AddTool("self.camera.capture_frame",
            "Capture one JPEG frame from the external XIAO camera bridge.",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                std::string jpeg;
                std::string error;
                uint32_t frame_id = 0;
                if (!servo_controller_->RequestFrame(jpeg, frame_id, error)) {
                    cJSON* root = cJSON_CreateObject();
                    cJSON_AddBoolToObject(root, "ok", false);
                    cJSON_AddStringToObject(root, "error", error.c_str());
                    return root;
                }
                return new ImageContent("image/jpeg", jpeg);
            });

        mcp_server.AddTool("self.camera.stream_off",
            "Stop XIAO camera UART stream mode.",
            PropertyList(), [this](const PropertyList&) -> ReturnValue {
                return servo_controller_->CameraStreamOff();
            });
    }

public:
    WaveshareEsp32c6TouchAMOLED2inch06() : boot_button_(BOOT_BUTTON_GPIO) {
        InitializePowerSaveTimer();
        InitializeCodecI2c();
        InitializeAxp2101();
        InitializeSpi();
        InitializeSH8601Display();
        InitializeButtons();
        InitializeServoController();
        homebot_ble_.SetXiaoController(servo_controller_);
        InitializeTools();
        homebot_ble_.Start();
    }

    virtual AudioCodec* GetAudioCodec() override {
        static BoxAudioCodec audio_codec(
            i2c_bus_, 
            AUDIO_INPUT_SAMPLE_RATE, 
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, 
            AUDIO_I2S_GPIO_BCLK, 
            AUDIO_I2S_GPIO_WS, 
            AUDIO_I2S_GPIO_DOUT, 
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, 
            AUDIO_CODEC_ES8311_ADDR, 
            AUDIO_CODEC_ES7210_ADDR, 
            AUDIO_INPUT_REFERENCE);
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_adapter_ != nullptr ? static_cast<Display*>(display_adapter_)
                                           : static_cast<Display*>(lvgl_display_);
    }

    virtual Backlight* GetBacklight() override {
        return backlight_;
    }

    virtual bool GetBatteryLevel(int &level, bool &charging, bool &discharging) override {
        static bool last_discharging = false;
        charging = pmic_->IsCharging();
        discharging = pmic_->IsDischarging();
        if (discharging != last_discharging)
        {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }

        level = pmic_->GetBatteryLevel();
        return true;
    }

    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled)
        {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveMode(enabled);
    }
};

DECLARE_BOARD(WaveshareEsp32c6TouchAMOLED2inch06);
