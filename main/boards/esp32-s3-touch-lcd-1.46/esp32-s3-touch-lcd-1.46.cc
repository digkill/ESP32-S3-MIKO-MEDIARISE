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
#include "esp_io_expander_tca9554.h"
#include <iot_button.h>
#include "simple_display.h"
#include "freertos/task.h"
#include <string>
#include <cstring>
#include <driver/gpio.h>
#include <driver/spi_common.h>

#define TAG "waveshare_lcd_1_46"

// Обертка для SimpleDisplay, реализующая интерфейс Display
class SimpleDisplayWrapper : public Display {
private:
    SimpleDisplay* simple_display_;
    TaskHandle_t animation_task_handle_;
    bool locked_;

    static void AnimationTask(void* param) {
        SimpleDisplayWrapper* self = static_cast<SimpleDisplayWrapper*>(param);
        // Небольшая задержка перед началом анимации
        vTaskDelay(pdMS_TO_TICKS(500));
        while (true) {
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
    i2c_master_bus_handle_t i2c_bus_;
    esp_io_expander_handle_t io_expander = NULL;
    esp_lcd_panel_handle_t panel_handle_ = nullptr;
    Display* display_;
    button_handle_t boot_btn, pwr_btn;
    button_driver_t* boot_btn_driver_ = nullptr;
    button_driver_t* pwr_btn_driver_ = nullptr;
    static CustomBoard* instance_;

    void InitializeI2c() {
        // I2C отключен (тачскрин не используется)
        ESP_LOGI(TAG, "I2C disabled (touchscreen not used)");
        i2c_bus_ = nullptr;
    }
    
    void InitializeTca9554(void) {
        if (i2c_bus_ == nullptr) {
            ESP_LOGW(TAG, "I2C bus not initialized, skipping TCA9554");
            io_expander = NULL;
            return;
        }
        esp_err_t ret = esp_io_expander_new_i2c_tca9554(i2c_bus_, I2C_ADDRESS, &io_expander);
        if(ret != ESP_OK) {
            ESP_LOGW(TAG, "TCA9554 not found, continuing without IO expander");
            io_expander = NULL;
            return;
        }

        ESP_LOGI(TAG, "TCA9554 initialized successfully");

        // uint32_t input_level_mask = 0;
        // ret = esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1, IO_EXPANDER_INPUT);               // 设置引脚 EXIO0 和 EXIO1 模式为输入 
        // ret = esp_io_expander_get_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1, &input_level_mask);             // 获取引脚 EXIO0 和 EXIO1 的电平状态,存放在 input_level_mask 中

        // ret = esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_2 | IO_EXPANDER_PIN_NUM_3, IO_EXPANDER_OUTPUT);              // 设置引脚 EXIO2 和 EXIO3 模式为输出
        // ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_2 | IO_EXPANDER_PIN_NUM_3, 1);                             // 将引脚电平设置为 1
        // ret = esp_io_expander_print_state(io_expander);                                                                             // 打印引脚状态

        ret = esp_io_expander_set_dir(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1, IO_EXPANDER_OUTPUT);                 // 设置引脚 EXIO0 和 EXIO1 模式为输出
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set TCA9554 direction");
            return;
        }
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1, 1);                                // 复位 LCD 与 TouchPad
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set TCA9554 level");
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(300));
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1, 0);                                // 复位 LCD 与 TouchPad
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set TCA9554 level");
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(300));
        ret = esp_io_expander_set_level(io_expander, IO_EXPANDER_PIN_NUM_0 | IO_EXPANDER_PIN_NUM_1, 1);                                // 复位 LCD 与 TouchPad
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set TCA9554 level");
            return;
        }
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
        display_ = new SimpleDisplayWrapper(panel, DISPLAY_WIDTH, DISPLAY_HEIGHT);
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

public:
    CustomBoard() { 
        InitializeI2c();
        InitializeTca9554();
        InitializeSpi();
        InitializeSt7789Display();
        InitializeButtons();
        GetBacklight()->RestoreBrightness();
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
