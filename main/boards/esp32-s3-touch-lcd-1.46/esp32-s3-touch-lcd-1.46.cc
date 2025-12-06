#include "wifi_board.h"
#include "codecs/no_audio_codec.h"
#include "display/display.h"
#include "application.h"
#include "config.h"
#include "assets/lang_config.h"

#include <esp_log.h>
#include <driver/i2c_master.h>
#include <driver/ledc.h>
#include <wifi_station.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_st7789.h>
#include <esp_timer.h>
#include <esp_task_wdt.h>
#include "esp_io_expander_tca9554.h"
#include <iot_button.h>
#include "simple_display.h"
#include "freertos/task.h"
#include <string>
#include <cstring>
#include <driver/gpio.h>
#include <driver/spi_common.h>
#include <math.h>
#include <algorithm>

#define TAG "waveshare_lcd_1_46"

namespace {

constexpr uint8_t kQmi8658Addr = 0x6B;
constexpr uint8_t kQmi8658WhoAmI = 0x00;
constexpr uint8_t kQmi8658Ctrl1 = 0x02;
constexpr uint8_t kQmi8658Ctrl2 = 0x03;
constexpr uint8_t kQmi8658Ctrl3 = 0x04;
constexpr uint8_t kQmi8658Ctrl5 = 0x06;
constexpr uint8_t kQmi8658Ctrl7 = 0x08;
constexpr uint8_t kQmi8658Status = 0x2D;
constexpr uint8_t kQmi8658AxL = 0x35;
constexpr float kShakeAxisThresholdG = 1.8f;
constexpr int64_t kShakeCooldownUs = 4 * 1000 * 1000;

}  // namespace

class Qmi8658Sensor {
public:
    bool Init(i2c_master_bus_handle_t bus) {
        if (!bus) {
            ESP_LOGE(TAG, "QMI8658 Init: bus is null");
            return false;
        }
        ESP_LOGI(TAG, "QMI8658 Init: Adding device to I2C bus...");
        i2c_device_config_t dev_cfg = {
            .device_address = kQmi8658Addr,
            .scl_speed_hz = 400000,
        };
        esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &device_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to add QMI8658 device: %s", esp_err_to_name(ret));
            return false;
        }
        ESP_LOGI(TAG, "QMI8658 device added, reading WHO_AM_I...");
        uint8_t who_am_i = 0;
        if (!ReadRegs(kQmi8658WhoAmI, &who_am_i, 1) || who_am_i == 0x00) {
            ESP_LOGE(TAG, "Failed to read QMI8658 WHO_AM_I (got 0x%02X), device may not be present", who_am_i);
            i2c_master_bus_rm_device(device_);
            device_ = nullptr;
            return false;
        }
        ESP_LOGI(TAG, "QMI8658 WHO_AM_I: 0x%02X", who_am_i);

        // Enable auto increment and oscillator.
        uint8_t ctrl1 = 0x40;  // auto inc
        if (!WriteReg(kQmi8658Ctrl1, ctrl1)) {
            ESP_LOGE(TAG, "Failed to write QMI8658 Ctrl1");
            i2c_master_bus_rm_device(device_);
            device_ = nullptr;
            return false;
        }

        // Accelerometer: 4G, 500Hz.
        uint8_t ctrl2 = (0x04 << 4) | 0x04;
        if (!WriteReg(kQmi8658Ctrl2, ctrl2)) {
            ESP_LOGE(TAG, "Failed to write QMI8658 Ctrl2");
            i2c_master_bus_rm_device(device_);
            device_ = nullptr;
            return false;
        }

        // Gyro: 256DPS, 500Hz.
        uint8_t ctrl3 = (0x05 << 4) | 0x04;
        if (!WriteReg(kQmi8658Ctrl3, ctrl3)) {
            ESP_LOGE(TAG, "Failed to write QMI8658 Ctrl3");
            i2c_master_bus_rm_device(device_);
            device_ = nullptr;
            return false;
        }

        // Enable LPF for acc/gyro.
        if (!WriteReg(kQmi8658Ctrl5, 0x11)) {
            ESP_LOGE(TAG, "Failed to write QMI8658 Ctrl5");
            i2c_master_bus_rm_device(device_);
            device_ = nullptr;
            return false;
        }

        // Enable high speed clock and both sensors.
        if (!WriteReg(kQmi8658Ctrl7, 0x43)) {
            ESP_LOGE(TAG, "Failed to write QMI8658 Ctrl7");
            i2c_master_bus_rm_device(device_);
            device_ = nullptr;
            return false;
        }

        accel_scale_ = 4.0f / 32768.0f;
        return true;
    }

    bool ReadAccel(float& x, float& y, float& z) {
        if (!device_) {
            return false;
        }
        uint8_t buf[6];
        if (!ReadRegs(kQmi8658AxL, buf, sizeof(buf))) {
            return false;
        }
        int16_t raw_x = static_cast<int16_t>((buf[1] << 8) | buf[0]);
        int16_t raw_y = static_cast<int16_t>((buf[3] << 8) | buf[2]);
        int16_t raw_z = static_cast<int16_t>((buf[5] << 8) | buf[4]);
        x = raw_x * accel_scale_;
        y = raw_y * accel_scale_;
        z = raw_z * accel_scale_;
        return true;
    }

private:
    bool WriteReg(uint8_t reg, uint8_t value) {
        if (!device_) return false;
        uint8_t data[2] = {reg, value};
        // Таймаут 100ms для I2C операций
        esp_err_t ret = i2c_master_transmit(device_, data, sizeof(data), pdMS_TO_TICKS(100));
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "QMI8658 WriteReg failed: %s (reg=0x%02X, val=0x%02X)", esp_err_to_name(ret), reg, value);
        }
        return ret == ESP_OK;
    }

    bool ReadRegs(uint8_t reg, uint8_t* data, size_t len) {
        if (!device_) return false;
        // Таймаут 100ms для I2C операций
        esp_err_t ret = i2c_master_transmit_receive(device_, &reg, 1, data, len, pdMS_TO_TICKS(100));
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "QMI8658 ReadRegs failed: %s (reg=0x%02X)", esp_err_to_name(ret), reg);
        }
        return ret == ESP_OK;
    }

    i2c_master_dev_handle_t device_{nullptr};
    float accel_scale_{1.0f};
};

// Обертка для SimpleDisplay, реализующая интерфейс Display
class SimpleDisplayWrapper : public Display {
private:
    SimpleDisplay* simple_display_;
    TaskHandle_t animation_task_handle_;
    bool locked_;

    static void AnimationTask(void* param) {
        // Добавляем задачу в watchdog
        esp_task_wdt_add(NULL);
        
        SimpleDisplayWrapper* self = static_cast<SimpleDisplayWrapper*>(param);
        // Небольшая задержка перед началом анимации
        vTaskDelay(pdMS_TO_TICKS(500));
        while (true) {
            // Сбрасываем watchdog в цикле
            esp_task_wdt_reset();
            if (self->simple_display_) {
                self->simple_display_->Update();
            }
            vTaskDelay(pdMS_TO_TICKS(100)); // Обновляем каждые 100ms
        }
    }

public:
    SimpleDisplayWrapper(esp_lcd_panel_handle_t panel, int width, int height) 
        : animation_task_handle_(nullptr), locked_(false) {
        width_ = width;
        height_ = height;
        simple_display_ = new SimpleDisplay(panel, width, height);
        if (simple_display_ && simple_display_->Init()) {
            ESP_LOGI(TAG, "SimpleDisplay инициализирован");
            // Создаем задачу для анимации глаз
            xTaskCreate(AnimationTask, "eye_anim", 2048, this, 5, &animation_task_handle_);
        } else {
            ESP_LOGE(TAG, "Ошибка инициализации SimpleDisplay");
        }
    }

    ~SimpleDisplayWrapper() {
        if (animation_task_handle_) {
            vTaskDelete(animation_task_handle_);
        }
        if (simple_display_) {
            delete simple_display_;
        }
    }

    void TriggerShakeEffect() {
        if (simple_display_) {
            simple_display_->TriggerDizzyEffect();
        }
    }

    virtual void SetStatus(const char* status) override {
        ESP_LOGI(TAG, "SetStatus: %s", status ? status : "null");
        // Обновляем эмоцию в зависимости от статуса
        if (simple_display_ && status) {
            if (strcmp(status, Lang::Strings::LISTENING) == 0) {
                simple_display_->SetEmotion("happy");
                simple_display_->SetSpeaking(false);
            } else if (strcmp(status, Lang::Strings::STANDBY) == 0) {
                simple_display_->SetEmotion("neutral");
                simple_display_->SetSpeaking(false);
            } else if (strcmp(status, Lang::Strings::SPEAKING) == 0) {
                simple_display_->SetEmotion("neutral");
                simple_display_->SetSpeaking(true);  // Включаем анимацию рта
            } else if (strcmp(status, Lang::Strings::CONNECTING) == 0) {
                simple_display_->SetEmotion("thinking");
                simple_display_->SetSpeaking(false);
            } else {
                simple_display_->SetSpeaking(false);
            }
        }
    }

    virtual void ShowNotification(const char* notification, int duration_ms = 3000) override {
        // Простая реализация - можно расширить
    }

    virtual void ShowNotification(const std::string &notification, int duration_ms = 3000) override {
        ShowNotification(notification.c_str(), duration_ms);
    }

    virtual void SetEmotion(const char* emotion) override {
        if (simple_display_ && emotion) {
            simple_display_->SetEmotion(emotion);
        }
    }

    virtual void SetChatMessage(const char* role, const char* content) override {
        // Простая реализация - можно расширить
    }

    virtual void SetTheme(Theme* theme) override {
        current_theme_ = theme;
    }

    virtual void UpdateStatusBar(bool update_all = false) override {
        if (simple_display_) {
            simple_display_->UpdateStatusBar();
        }
    }

    virtual void SetPowerSaveMode(bool on) override {
        // Простая реализация - можно расширить
    }

    virtual bool Lock(int timeout_ms = 0) override {
        locked_ = true;
        return true;
    }

    virtual void Unlock() override {
        locked_ = false;
    }
};

class CustomBoard : public WifiBoard {
private:
    i2c_master_bus_handle_t i2c_bus_ = nullptr;  // I2C отключен - тачскрин и IMU не используются
    esp_io_expander_handle_t io_expander = nullptr;  // TCA9554 отключен
    esp_lcd_panel_handle_t panel_handle_ = nullptr;
    SimpleDisplayWrapper* simple_display_wrapper_ = nullptr;
    Display* display_ = nullptr;
    button_handle_t boot_btn, pwr_btn;
    button_driver_t* boot_btn_driver_ = nullptr;
    button_driver_t* pwr_btn_driver_ = nullptr;
    static CustomBoard* instance_;
    TaskHandle_t imu_task_handle_ = nullptr;
    Qmi8658Sensor imu_sensor_;
    int64_t last_shake_time_us_ = 0;

    void InitializeI2c() {
        // I2C полностью отключен - тачскрин и IMU не используются
        ESP_LOGI(TAG, "I2C disabled (touchscreen and IMU not used)");
        i2c_bus_ = nullptr;
    }
    
    void InitializeTca9554(void) {
        // TCA9554 отключен - тачскрин не используется
        ESP_LOGI(TAG, "TCA9554 disabled (touchscreen not used)");
        io_expander = nullptr;
    }

    void InitializeSpi() {
        ESP_LOGI(TAG, "Initialize SPI bus for ST7789");

        const spi_bus_config_t bus_config = {
            .mosi_io_num = DISPLAY_SPI_MOSI_PIN,
            .miso_io_num = GPIO_NUM_NC,  // ST7789 не использует MISO
            .sclk_io_num = DISPLAY_SPI_SCLK_PIN,
            .quadwp_io_num = GPIO_NUM_NC,
            .quadhd_io_num = GPIO_NUM_NC,
            .max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t),
        };
        ESP_ERROR_CHECK(spi_bus_initialize(DISPLAY_SPI_HOST, &bus_config, SPI_DMA_CH_AUTO));
        ESP_LOGI(TAG, "SPI bus initialized");
    }


    void InitializeSt7789Display() {
        esp_lcd_panel_io_handle_t panel_io = nullptr;
        esp_lcd_panel_handle_t panel = nullptr;

        ESP_LOGI(TAG, "Install ST7789 panel IO");
        
        esp_lcd_panel_io_spi_config_t io_config = {};
        io_config.cs_gpio_num = DISPLAY_SPI_CS_PIN;
        io_config.dc_gpio_num = DISPLAY_SPI_DC_PIN;
        io_config.spi_mode = 0;
        io_config.pclk_hz = DISPLAY_SPI_CLOCK_HZ;
        io_config.trans_queue_depth = 10;
        io_config.lcd_cmd_bits = 8;
        io_config.lcd_param_bits = 8;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(DISPLAY_SPI_HOST, &io_config, &panel_io));
        ESP_LOGI(TAG, "Panel IO created");

        ESP_LOGI(TAG, "Install ST7789 panel driver");
        esp_lcd_panel_dev_config_t panel_config = {
            .reset_gpio_num = DISPLAY_SPI_RST_PIN,
            .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
            .bits_per_pixel = 16,
        };
        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(panel_io, &panel_config, &panel));
        ESP_LOGI(TAG, "ST7789 panel created");

        esp_lcd_panel_reset(panel);
        esp_lcd_panel_init(panel);
        esp_lcd_panel_invert_color(panel, true);  // ST7789 обычно требует инверсию цвета
        esp_lcd_panel_swap_xy(panel, DISPLAY_SWAP_XY);
        esp_lcd_panel_mirror(panel, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_disp_on_off(panel, true);
        ESP_LOGI(TAG, "ST7789 panel initialized");

        // Включаем подсветку
        gpio_reset_pin(DISPLAY_SPI_BL_PIN);
        gpio_set_direction(DISPLAY_SPI_BL_PIN, GPIO_MODE_OUTPUT);
        gpio_set_level(DISPLAY_SPI_BL_PIN, 1);
        ESP_LOGI(TAG, "Backlight enabled");

        ESP_LOGI(TAG, "Initializing SimpleDisplay with robot eyes animation...");
        panel_handle_ = panel;
        
        // Используем SimpleDisplay с анимацией глаз
        simple_display_wrapper_ = new SimpleDisplayWrapper(panel, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        display_ = simple_display_wrapper_;
        ESP_LOGI(TAG, "SimpleDisplayWrapper initialized");
    }
 
    void InitializeButtonsCustom() {
        gpio_reset_pin(BOOT_BUTTON_GPIO);                                     
        gpio_set_direction(BOOT_BUTTON_GPIO, GPIO_MODE_INPUT);   
        gpio_reset_pin(PWR_BUTTON_GPIO);                                     
        gpio_set_direction(PWR_BUTTON_GPIO, GPIO_MODE_INPUT);   
        gpio_reset_pin(PWR_Control_PIN);                                     
        gpio_set_direction(PWR_Control_PIN, GPIO_MODE_OUTPUT);     
        // gpio_set_level(PWR_Control_PIN, false);
        gpio_set_level(PWR_Control_PIN, true);
    }

    void InitializeButtons() {
        instance_ = this;
        InitializeButtonsCustom();

        // Boot Button
        button_config_t boot_btn_config = {
            .long_press_time = 2000,
            .short_press_time = 0
        };
        boot_btn_driver_ = (button_driver_t*)calloc(1, sizeof(button_driver_t));
        boot_btn_driver_->enable_power_save = false;
        boot_btn_driver_->get_key_level = [](button_driver_t *button_driver) -> uint8_t {
            return !gpio_get_level(BOOT_BUTTON_GPIO);
        };
        ESP_ERROR_CHECK(iot_button_create(&boot_btn_config, boot_btn_driver_, &boot_btn));
        iot_button_register_cb(boot_btn, BUTTON_SINGLE_CLICK, nullptr, [](void* button_handle, void* usr_data) {
            auto self = static_cast<CustomBoard*>(usr_data);
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                self->ResetWifiConfiguration();
            }
            app.ToggleChatState();
        }, this);
        iot_button_register_cb(boot_btn, BUTTON_LONG_PRESS_START, nullptr, [](void* button_handle, void* usr_data) {
            // 长按无处理
        }, this);

        // Power Button
        button_config_t pwr_btn_config = {
            .long_press_time = 5000,
            .short_press_time = 0
        };
        pwr_btn_driver_ = (button_driver_t*)calloc(1, sizeof(button_driver_t));
        pwr_btn_driver_->enable_power_save = false;
        pwr_btn_driver_->get_key_level = [](button_driver_t *button_driver) -> uint8_t {
            return !gpio_get_level(PWR_BUTTON_GPIO);
        };
        ESP_ERROR_CHECK(iot_button_create(&pwr_btn_config, pwr_btn_driver_, &pwr_btn));
        iot_button_register_cb(pwr_btn, BUTTON_SINGLE_CLICK, nullptr, [](void* button_handle, void* usr_data) {
            // 短按无处理
        }, this);
        iot_button_register_cb(pwr_btn, BUTTON_LONG_PRESS_START, nullptr, [](void* button_handle, void* usr_data) {
            auto self = static_cast<CustomBoard*>(usr_data);
            if(self->GetBacklight()->brightness() > 0) {
                self->GetBacklight()->SetBrightness(0);
                gpio_set_level(PWR_Control_PIN, false);
            }
            else {
                self->GetBacklight()->RestoreBrightness();
                gpio_set_level(PWR_Control_PIN, true);
            }
        }, this);
    }

    // IMU отключен - задачи не используются
    // static void ImuTask(void* param) {
    //     CustomBoard* self = static_cast<CustomBoard*>(param);
    //     while (true) {
    //         if (self->imu_task_handle_ == nullptr) {
    //             vTaskDelete(nullptr);
    //         }
    //         float x = 0, y = 0, z = 0;
    //         if (self->imu_sensor_.ReadAccel(x, y, z)) {
    //             float max_axis = fabsf(x);
    //             max_axis = std::max(max_axis, fabsf(y));
    //             max_axis = std::max(max_axis, fabsf(z));
    //             if (max_axis > kShakeAxisThresholdG) {
    //                 int64_t now = esp_timer_get_time();
    //                 if (now - self->last_shake_time_us_ > kShakeCooldownUs) {
    //                     self->last_shake_time_us_ = now;
    //                     self->HandleShakeDetected();
    //                 }
    //             }
    //         }
    //         vTaskDelay(pdMS_TO_TICKS(80));
    //     }
    // }

    void HandleShakeDetected() {
        // IMU отключен, функция не используется
        // if (simple_display_wrapper_) {
        //     simple_display_wrapper_->TriggerShakeEffect();
        // }
        // Application::GetInstance().PlaySound(Lang::Sounds::OGG_POPUP);
    }

    void StartImuMonitor() {
        // IMU полностью отключен
        ESP_LOGI(TAG, "IMU monitor disabled (not used)");
        return;
    }

public:
    CustomBoard() { 
        ESP_LOGI(TAG, "CustomBoard constructor started");
        // I2C отключен для тачскрина, но может быть нужен для других устройств
        // InitializeI2c();  // Отключено - тачскрин не используется
        // ESP_LOGI(TAG, "I2C initialized");
        // InitializeTca9554();  // Отключено - тачскрин не используется
        // ESP_LOGI(TAG, "TCA9554 initialized");
        InitializeSpi();
        ESP_LOGI(TAG, "SPI initialized");
        InitializeSt7789Display();
        ESP_LOGI(TAG, "ST7789 display initialized");
        InitializeButtons();
        ESP_LOGI(TAG, "Buttons initialized");
        GetBacklight()->RestoreBrightness();
        ESP_LOGI(TAG, "Backlight restored");
        // StartImuMonitor();  // Отключено - акселерометр не используется
        // ESP_LOGI(TAG, "IMU monitor started");
        ESP_LOGI(TAG, "CustomBoard constructor completed");
    }

    virtual AudioCodec* GetAudioCodec() override {
        static bool first_call = true;
        static NoAudioCodecSimplex audio_codec(AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT, I2S_STD_SLOT_LEFT, 
            AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN, I2S_STD_SLOT_RIGHT);
        
        if (first_call) {
            ESP_LOGI(TAG, "Audio codec initialized: Input=%dHz, Output=%dHz", AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE);
            ESP_LOGI(TAG, "Speaker: BCLK=%d, LRCK=%d, DOUT=%d", AUDIO_I2S_SPK_GPIO_BCLK, AUDIO_I2S_SPK_GPIO_LRCK, AUDIO_I2S_SPK_GPIO_DOUT);
            ESP_LOGI(TAG, "Microphone: SCK=%d, WS=%d, DIN=%d", AUDIO_I2S_MIC_GPIO_SCK, AUDIO_I2S_MIC_GPIO_WS, AUDIO_I2S_MIC_GPIO_DIN);
            first_call = false;
        }
        return &audio_codec;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }
    
    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }
};

DECLARE_BOARD(CustomBoard);

CustomBoard* CustomBoard::instance_ = nullptr;
