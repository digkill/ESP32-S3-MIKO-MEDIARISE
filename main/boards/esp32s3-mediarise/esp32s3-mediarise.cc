#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "codecs/no_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#include "assets/lang_config.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <esp_efuse_table.h>
#include <driver/i2c_master.h>

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_gc9a01.h>
#include "system_reset.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include <esp_timer.h>
#include "i2c_device.h"
#include <esp_lcd_panel_vendor.h>
#include <driver/spi_common.h>
#include "power_save_timer.h"
#include <esp_sleep.h>
#include <driver/rtc_io.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "power_manager.h"
#include "simple_display.h"
#include "servo_controller.h"
#include "mcp_server.h"
#include <cstring>
#include <mutex>

#define TAG "ESP32S3_MediaRise"

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);


class Cst816d : public I2cDevice {
public:
    struct TouchPoint_t {
        int num = 0;
        int x = -1;
        int y = -1;
    };
    Cst816d(i2c_master_bus_handle_t i2c_bus, uint8_t addr) : I2cDevice(i2c_bus, addr) {
        uint8_t chip_id = ReadReg(0xA3);
        ESP_LOGI(TAG, "Get chip ID: 0x%02X", chip_id);
        last_chip_id_ = chip_id;
        read_buffer_ = new uint8_t[6];
    }

    ~Cst816d() {
        if (read_buffer_) {
            delete[] read_buffer_;
            read_buffer_ = nullptr;
        }
    }

    void UpdateTouchPoint() {
        if (!read_buffer_) return;
        ReadRegs(0x02, read_buffer_, 6);
        if (read_buffer_[0] == 0xFF) {
            read_buffer_[0] = 0x00;
        }
        tp_.num = read_buffer_[0] & 0x01;
        tp_.x = ((read_buffer_[1] & 0x0F) << 8) | read_buffer_[2];
        tp_.y = ((read_buffer_[3] & 0x0F) << 8) | read_buffer_[4];
    }

    const TouchPoint_t& GetTouchPoint() const {
        return tp_;
    }

    static bool Probe(i2c_master_bus_handle_t i2c_bus, uint8_t addr, uint8_t& chip_id) {
        if (!i2c_bus) return false;
        i2c_master_dev_handle_t dev = nullptr;
        i2c_device_config_t cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 400 * 1000,
            .scl_wait_us = 0,
            .flags = {
                .disable_ack_check = 0,
            },
        };
        esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &cfg, &dev);
        if (ret != ESP_OK || dev == nullptr) {
            return false;
        }
        uint8_t reg = 0xA3;
        uint8_t id = 0;
        ret = i2c_master_transmit_receive(dev, &reg, 1, &id, 1, 100);
        i2c_master_bus_rm_device(dev);
        if (ret == ESP_OK) {
            chip_id = id;
            return true;
        }
        return false;
    }

private:
    uint8_t* read_buffer_ = nullptr;
    TouchPoint_t tp_;
    uint8_t last_chip_id_ = 0;
};


class CustomLcdDisplay : public SpiLcdDisplay {
public:
    CustomLcdDisplay(esp_lcd_panel_io_handle_t io_handle,
                    esp_lcd_panel_handle_t panel_handle,
                    int width,
                    int height,
                    int offset_x,
                    int offset_y,
                    bool mirror_x,
                    bool mirror_y,
                    bool swap_xy)
        : SpiLcdDisplay(io_handle, panel_handle, width, height, offset_x, offset_y, mirror_x, mirror_y, swap_xy) {

        ESP_LOGI(TAG, "=== CustomLcdDisplay конструктор ===");
        ESP_LOGI(TAG, "Настройка круглого экрана...");
        
        DisplayLockGuard lock(this);
        if (status_bar_ == nullptr) {
            ESP_LOGE(TAG, "ОШИБКА: status_bar_ == nullptr!");
        } else {
            ESP_LOGI(TAG, "status_bar_ найден: %p", status_bar_);
            // Поскольку экран круглый, нужно увеличить левый и правый отступы для строки состояния
            int pad_value = LV_HOR_RES * 0.33;
            ESP_LOGI(TAG, "Установка отступов для круглого экрана: %d пикселей (33%% ширины)", pad_value);
            lv_obj_set_style_pad_left(status_bar_, pad_value, 0);
            lv_obj_set_style_pad_right(status_bar_, pad_value, 0);
            ESP_LOGI(TAG, "Отступы установлены");
        }
        ESP_LOGI(TAG, "=== CustomLcdDisplay инициализация завершена ===");
    }
};

class SimpleDisplayAdapter : public Display {
public:
    explicit SimpleDisplayAdapter(SimpleDisplay* simple_display)
        : simple_display_(simple_display),
          last_eye_color_(SimpleDisplay::Color565(255, 240, 90)) {
        width_ = DISPLAY_WIDTH;
        height_ = DISPLAY_HEIGHT;
        if (simple_display_) {
            simple_display_->DrawRobotBase();
            simple_display_->DrawEyes(last_eye_color_);
        }
    }

    void SetStatus(const char* status) override {
        if (!status || !simple_display_) {
            return;
        }
        const uint16_t color = EyeColorForStatus(status);
        DrawEyesIfChanged(color);
    }

    void SetChatMessage(const char* role, const char* content) override {
        if (!simple_display_) {
            return;
        }
        const uint16_t color = EyeColorForRole(role);
        DrawEyesIfChanged(color);
        if (content && *content) {
            ESP_LOGI(TAG, "Chat: %s", content);
        }
    }

    void SetEmotion(const char* emotion) override {
        if (!emotion || !simple_display_) {
            return;
        }
        const uint16_t color = EyeColorForEmotion(emotion);
        DrawEyesIfChanged(color);
    }

    void UpdateStatusBar(bool update_all = false) override {
        if (simple_display_ && update_all) {
            simple_display_->UpdateIdleEyes();
        }
    }

    void SetPowerSaveMode(bool on) override {
        if (!simple_display_) {
            return;
        }
        if (on) {
            simple_display_->FillScreen(SimpleDisplay::Color565(0, 0, 0));
        } else {
            simple_display_->DrawRobotBase();
            simple_display_->DrawEyes(last_eye_color_);
        }
    }

private:
    bool Lock(int timeout_ms = 0) override {
        (void)timeout_ms;
        mutex_.lock();
        return true;
    }

    void Unlock() override {
        mutex_.unlock();
    }

    uint16_t EyeColorForStatus(const char* status) {
        if (strcmp(status, Lang::Strings::LISTENING) == 0) {
            return SimpleDisplay::Color565(80, 180, 255);
        }
        if (strcmp(status, Lang::Strings::SPEAKING) == 0) {
            return SimpleDisplay::Color565(120, 255, 140);
        }
        if (strcmp(status, Lang::Strings::CONNECTING) == 0 ||
            strcmp(status, Lang::Strings::LOADING_PROTOCOL) == 0) {
            return SimpleDisplay::Color565(255, 200, 80);
        }
        if (strcmp(status, Lang::Strings::STANDBY) == 0) {
            return SimpleDisplay::Color565(255, 240, 90);
        }
        return last_eye_color_;
    }

    uint16_t EyeColorForRole(const char* role) {
        if (!role) {
            return last_eye_color_;
        }
        if (strcmp(role, "user") == 0) {
            return SimpleDisplay::Color565(120, 200, 255);
        }
        if (strcmp(role, "assistant") == 0) {
            return SimpleDisplay::Color565(140, 255, 140);
        }
        if (strcmp(role, "system") == 0) {
            return SimpleDisplay::Color565(255, 200, 80);
        }
        return last_eye_color_;
    }

    uint16_t EyeColorForEmotion(const char* emotion) {
        if (strcmp(emotion, "happy") == 0) {
            return SimpleDisplay::Color565(255, 240, 90);
        }
        if (strcmp(emotion, "sad") == 0) {
            return SimpleDisplay::Color565(80, 140, 255);
        }
        if (strcmp(emotion, "angry") == 0) {
            return SimpleDisplay::Color565(255, 80, 80);
        }
        if (strcmp(emotion, "neutral") == 0) {
            return SimpleDisplay::Color565(200, 200, 200);
        }
        return last_eye_color_;
    }

    void DrawEyesIfChanged(uint16_t color) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        if (color == last_eye_color_) {
            return;
        }
        last_eye_color_ = color;
        simple_display_->DrawEyes(color);
    }

    SimpleDisplay* simple_display_ = nullptr;
    uint16_t last_eye_color_ = 0;
    std::recursive_mutex mutex_;
};


class ESP32S3_MediaRise : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_ = nullptr;
    i2c_master_bus_handle_t i2c_bus_ = nullptr;
    Button boot_button_;
    Display* display_ = nullptr;
    esp_timer_handle_t touchpad_timer_ = nullptr;
    Cst816d* cst816d_ = nullptr;
    PowerSaveTimer* power_save_timer_ = nullptr;
    esp_lcd_panel_handle_t panel_ = nullptr;
    PowerManager* power_manager_ = nullptr;
    SimpleDisplay* simple_display_ = nullptr;  // Простой дисплей без LVGL
    ServoController* servo_controller_ = nullptr;
    uint8_t codec_i2c_addr_ = AUDIO_CODEC_ES8311_ADDR;
    bool codec_i2c_present_ = false;

    void InitializePowerSaveTimer() {
        rtc_gpio_init(GPIO_NUM_3);
        rtc_gpio_set_direction(GPIO_NUM_3, RTC_GPIO_MODE_OUTPUT_ONLY);
        rtc_gpio_set_level(GPIO_NUM_3, 1);

        power_save_timer_ = new PowerSaveTimer(-1, 60, 290);
        power_save_timer_->OnEnterSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(true);
            GetBacklight()->SetBrightness(1);
        });
        power_save_timer_->OnExitSleepMode([this]() {
            GetDisplay()->SetPowerSaveMode(false);
            GetBacklight()->RestoreBrightness();
        });
        power_save_timer_->OnShutdownRequest([this]() {
            ESP_LOGI(TAG, "Shutting down");
            // Отключить ES8311 аудио кодек
            auto codec = GetAudioCodec();
            if (codec) {
                codec->EnableInput(false);
                codec->EnableOutput(false);
            }
            rtc_gpio_set_level(GPIO_NUM_3, 0);
            // Включить функцию удержания, чтобы уровень не менялся во время сна
            rtc_gpio_hold_en(GPIO_NUM_3);
            esp_lcd_panel_disp_on_off(panel_, false); // Выключить дисплей
            esp_deep_sleep_start();
        });
        power_save_timer_->SetEnabled(true);
    }

    void InitializePowerManager() {
        power_manager_ = new PowerManager(BATTERY_CHARGING_PIN, ADC_CHANNEL_0);
        power_manager_->OnChargingStatusChanged([this](bool is_charging) {
            if (is_charging) {
                power_save_timer_->SetEnabled(false);
            } else {
                power_save_timer_->SetEnabled(true);
            }
        });
    }

    void InitializeCodecI2c() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    bool ProbeI2cDevice(i2c_master_bus_handle_t i2c_bus, uint8_t addr) {
        if (!i2c_bus) {
            return false;
        }
        i2c_master_dev_handle_t dev = nullptr;
        i2c_device_config_t cfg = {
            .dev_addr_length = I2C_ADDR_BIT_LEN_7,
            .device_address = addr,
            .scl_speed_hz = 400 * 1000,
            .scl_wait_us = 0,
            .flags = {
                .disable_ack_check = 0,
            },
        };
        esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &cfg, &dev);
        if (ret != ESP_OK || dev == nullptr) {
            return false;
        }
        uint8_t reg = 0x00;
        uint8_t val = 0;
        ret = i2c_master_transmit_receive(dev, &reg, 1, &val, 1, 100);
        i2c_master_bus_rm_device(dev);
        return ret == ESP_OK;
    }

    void DetectCodecAddress() {
        codec_i2c_present_ = false;
        if (!codec_i2c_bus_) {
            ESP_LOGE(TAG, "Codec I2C bus not initialized");
            return;
        }
        const uint8_t candidates[] = {AUDIO_CODEC_ES8311_ADDR, AUDIO_CODEC_ES8311_ADDR_ALT};
        for (uint8_t addr : candidates) {
            if (ProbeI2cDevice(codec_i2c_bus_, addr)) {
                codec_i2c_addr_ = addr;
                codec_i2c_present_ = true;
                ESP_LOGI(TAG, "ES8311 найден по адресу 0x%02X", addr);
                return;
            }
        }
        ESP_LOGE(TAG,
            "ES8311 не найден (SDA=%d SCL=%d). Проверьте адрес/подключение.",
            AUDIO_CODEC_I2C_SDA_PIN, AUDIO_CODEC_I2C_SCL_PIN);
    }

    void InitializeCodecI2c_Touch() {
        // Initialize I2C peripheral
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_1,
            .sda_io_num = TP_PIN_NUM_TP_SDA,
            .scl_io_num = TP_PIN_NUM_TP_SCL,
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .glitch_ignore_cnt = 7,
            .intr_priority = 0,
            .trans_queue_depth = 0,
            .flags = {
                .enable_internal_pullup = 1,
            },
        };
        esp_err_t ret = i2c_new_master_bus(&i2c_bus_cfg, &i2c_bus_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(ret));
            i2c_bus_ = nullptr;
        }
    }


    static void touchpad_timer_callback(void* arg) {
        auto* board = static_cast<ESP32S3_MediaRise*>(arg);
        if (!board || !board->cst816d_) return;
        static bool was_touched = false;
        static int64_t touch_start_time = 0;
        const int64_t TOUCH_THRESHOLD_MS = 500;  // Порог длительности касания, более 500мс считается долгим нажатием

        board->cst816d_->UpdateTouchPoint();
        auto touch_point = board->cst816d_->GetTouchPoint();

        // Обнаружение начала касания
        if (touch_point.num > 0 && !was_touched) {
            was_touched = true;
            touch_start_time = esp_timer_get_time() / 1000; // Преобразовать в миллисекунды
        }
        // Обнаружение отпускания касания
        else if (touch_point.num == 0 && was_touched) {
            was_touched = false;
            int64_t touch_duration = (esp_timer_get_time() / 1000) - touch_start_time;

            // Только короткое касание вызывает действие
            if (touch_duration < TOUCH_THRESHOLD_MS) {
                auto& app = Application::GetInstance();
                if (app.GetDeviceState() == kDeviceStateStarting &&
                    !WifiStation::GetInstance().IsConnected()) {
                    board->ResetWifiConfiguration();
                }
                app.ToggleChatState();
            }
        }
    }

    void InitializeCst816DTouchPad() {
        ESP_LOGI(TAG, "Init Cst816D");

        // Инициализация выводов RST/INT (RST может отсутствовать)
        if (TP_PIN_NUM_TP_RST != GPIO_NUM_NC) {
            gpio_config_t io_conf = {};
            io_conf.intr_type = GPIO_INTR_DISABLE;
            io_conf.mode = GPIO_MODE_OUTPUT;
            io_conf.pin_bit_mask = (1ULL << TP_PIN_NUM_TP_RST);
            io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
            io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
            gpio_config(&io_conf);
        }

        gpio_config_t int_conf = {};
        int_conf.intr_type = GPIO_INTR_DISABLE;
        int_conf.mode = GPIO_MODE_INPUT;
        int_conf.pin_bit_mask = (1ULL << TP_PIN_NUM_TP_INT);
        int_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        int_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&int_conf);

        // Последовательность сброса сенсорного чипа (если RST есть)
        if (TP_PIN_NUM_TP_RST != GPIO_NUM_NC) {
            gpio_set_level(TP_PIN_NUM_TP_RST, 0);
            vTaskDelay(pdMS_TO_TICKS(5));
            gpio_set_level(TP_PIN_NUM_TP_RST, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
        }

        // Проверка наличия сенсорного чипа
        uint8_t chip_id = 0;
        if (!i2c_bus_) {
            ESP_LOGW(TAG, "Touch I2C bus not initialized, skip touch");
            return;
        }
        bool touch_available = Cst816d::Probe(i2c_bus_, 0x15, chip_id);
        if (!touch_available) {
            ESP_LOGW(TAG, "CST816D not found, running in non-touch mode");
            // Освободить I2C шину сенсора, чтобы избежать повторных ошибок при отсутствии устройства
            i2c_del_master_bus(i2c_bus_);
            i2c_bus_ = nullptr;
            return;
        }

        cst816d_ = new Cst816d(i2c_bus_, 0x15);

        // Создать таймер с интервалом 10мс
        esp_timer_create_args_t timer_args = {
            .callback = touchpad_timer_callback,
            .arg = this,
            .dispatch_method = ESP_TIMER_TASK,
            .name = "touchpad_timer",
            .skip_unhandled_events = true,
        };

        if (esp_timer_create(&timer_args, &touchpad_timer_) == ESP_OK) {
            esp_timer_start_periodic(touchpad_timer_, 10 * 1000); // 10ms = 10000us
        }
    }

    // Диагностика физического подключения дисплея
    void DiagnoseDisplayPins() {
        ESP_LOGI(TAG, "=== ДИАГНОСТИКА ФИЗИЧЕСКОГО ПОДКЛЮЧЕНИЯ ДИСПЛЕЯ ===");
        
        // Проверка пинов SPI
        ESP_LOGI(TAG, "Пины SPI дисплея:");
        ESP_LOGI(TAG, "  SCLK: GPIO%d", DISPLAY_SPI_SCLK_PIN);
        ESP_LOGI(TAG, "  MOSI: GPIO%d", DISPLAY_SPI_MOSI_PIN);
        ESP_LOGI(TAG, "  CS:   GPIO%d", DISPLAY_SPI_CS_PIN);
        ESP_LOGI(TAG, "  DC:   GPIO%d", DISPLAY_SPI_DC_PIN);
        ESP_LOGI(TAG, "  RESET: GPIO%d", DISPLAY_SPI_RESET_PIN);
        ESP_LOGI(TAG, "  Backlight: GPIO%d (invert=%s)", DISPLAY_BACKLIGHT_PIN, 
                 DISPLAY_BACKLIGHT_OUTPUT_INVERT ? "true" : "false");
        
        // Тест пина RESET - мигание для проверки подключения
        ESP_LOGI(TAG, "Тест пина RESET...");
        gpio_config_t reset_io_conf = {};
        reset_io_conf.intr_type = GPIO_INTR_DISABLE;
        reset_io_conf.mode = GPIO_MODE_OUTPUT;
        reset_io_conf.pin_bit_mask = (1ULL << DISPLAY_SPI_RESET_PIN);
        reset_io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        reset_io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&reset_io_conf);
        
        // Мигание RESET 3 раза
        for (int i = 0; i < 3; i++) {
            gpio_set_level(DISPLAY_SPI_RESET_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(50));
            gpio_set_level(DISPLAY_SPI_RESET_PIN, 1);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        ESP_LOGI(TAG, "RESET протестирован (3 мигания)");
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // Тест пина CS - установка в высокий уровень
        ESP_LOGI(TAG, "Тест пина CS...");
        gpio_config_t cs_io_conf = {};
        cs_io_conf.intr_type = GPIO_INTR_DISABLE;
        cs_io_conf.mode = GPIO_MODE_OUTPUT;
        cs_io_conf.pin_bit_mask = (1ULL << DISPLAY_SPI_CS_PIN);
        cs_io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        cs_io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&cs_io_conf);
        gpio_set_level(DISPLAY_SPI_CS_PIN, 1);
        ESP_LOGI(TAG, "CS установлен в HIGH");
        
        // Тест пина DC
        ESP_LOGI(TAG, "Тест пина DC...");
        gpio_config_t dc_io_conf = {};
        dc_io_conf.intr_type = GPIO_INTR_DISABLE;
        dc_io_conf.mode = GPIO_MODE_OUTPUT;
        dc_io_conf.pin_bit_mask = (1ULL << DISPLAY_SPI_DC_PIN);
        dc_io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        dc_io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&dc_io_conf);
        gpio_set_level(DISPLAY_SPI_DC_PIN, 1);
        ESP_LOGI(TAG, "DC установлен в HIGH");
        
        ESP_LOGI(TAG, "=== ДИАГНОСТИКА ПИНОВ ЗАВЕРШЕНА ===");
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    // Инициализация SPI
    void InitializeSpi() {
        ESP_LOGI(TAG, "=== Инициализация SPI шины ===");
        ESP_LOGI(TAG, "SPI Host: SPI3_HOST");
        ESP_LOGI(TAG, "SCLK Pin: GPIO%d", DISPLAY_SPI_SCLK_PIN);
        ESP_LOGI(TAG, "MOSI Pin: GPIO%d", DISPLAY_SPI_MOSI_PIN);
        ESP_LOGI(TAG, "Размер буфера DMA: %d байт", DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t));
        
        spi_bus_config_t buscfg = GC9A01_PANEL_BUS_SPI_CONFIG(DISPLAY_SPI_SCLK_PIN, DISPLAY_SPI_MOSI_PIN,
                                    DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t));
        
        esp_err_t ret = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ОШИБКА инициализации SPI: %s (0x%x)", esp_err_to_name(ret), ret);
            ESP_LOGE(TAG, "ПРОВЕРЬТЕ: подключение пинов SCLK и MOSI к дисплею");
        } else {
            ESP_LOGI(TAG, "SPI шина успешно инициализирована");
        }
        ESP_ERROR_CHECK(ret);
    }

    // Инициализация GC9A01
    void InitializeGc9a01Display() {
        ESP_LOGI(TAG, "=== Инициализация дисплея GC9A01 ===");
        ESP_LOGI(TAG, "Размер дисплея: %dx%d", DISPLAY_WIDTH, DISPLAY_HEIGHT);
        ESP_LOGI(TAG, "Частота SPI: %d Hz", DISPLAY_SPI_SCLK_HZ);
        
        ESP_LOGI(TAG, "Установка Panel IO...");
        ESP_LOGI(TAG, "  CS Pin: GPIO%d", DISPLAY_SPI_CS_PIN);
        ESP_LOGI(TAG, "  DC Pin: GPIO%d", DISPLAY_SPI_DC_PIN);
        ESP_LOGI(TAG, "  RESET Pin: GPIO%d", DISPLAY_SPI_RESET_PIN);
        
        esp_lcd_panel_io_handle_t io_handle = NULL;
        esp_lcd_panel_io_spi_config_t io_config = GC9A01_PANEL_IO_SPI_CONFIG(DISPLAY_SPI_CS_PIN, DISPLAY_SPI_DC_PIN, 0, NULL);
        io_config.pclk_hz = DISPLAY_SPI_SCLK_HZ;
        
        esp_err_t ret = esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &io_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ОШИБКА создания Panel IO: %s (0x%x)", esp_err_to_name(ret), ret);
        } else {
            ESP_LOGI(TAG, "Panel IO успешно создан");
        }
        ESP_ERROR_CHECK(ret);

        ESP_LOGI(TAG, "Установка драйвера панели GC9A01...");
        esp_lcd_panel_handle_t panel_handle = NULL;
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_SPI_RESET_PIN;
        panel_config.rgb_endian = LCD_RGB_ENDIAN_BGR;
        panel_config.bits_per_pixel = 16;
        ESP_LOGI(TAG, "  RGB Endian: BGR");
        ESP_LOGI(TAG, "  Bits per pixel: %d", panel_config.bits_per_pixel);

        ret = esp_lcd_new_panel_gc9a01(io_handle, &panel_config, &panel_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ОШИБКА создания панели GC9A01: %s (0x%x)", esp_err_to_name(ret), ret);
        } else {
            ESP_LOGI(TAG, "Панель GC9A01 успешно создана");
        }
        ESP_ERROR_CHECK(ret);
        
        panel_ = panel_handle;
        
        ESP_LOGI(TAG, "Сброс панели...");
        // Дополнительная задержка перед сбросом для стабилизации питания
        vTaskDelay(pdMS_TO_TICKS(50));
        ret = esp_lcd_panel_reset(panel_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ОШИБКА сброса панели: %s (0x%x)", esp_err_to_name(ret), ret);
            ESP_LOGE(TAG, "ПРОВЕРЬТЕ: подключение пина RESET (GPIO%d) к дисплею", DISPLAY_SPI_RESET_PIN);
        } else {
            ESP_LOGI(TAG, "Панель сброшена");
        }
        ESP_ERROR_CHECK(ret);
        // Задержка после сброса для стабилизации
        vTaskDelay(pdMS_TO_TICKS(100));
        
        ESP_LOGI(TAG, "Инициализация панели...");
        ret = esp_lcd_panel_init(panel_handle);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ОШИБКА инициализации панели: %s (0x%x)", esp_err_to_name(ret), ret);
            ESP_LOGE(TAG, "ВОЗМОЖНЫЕ ПРИЧИНЫ:");
            ESP_LOGE(TAG, "  1. Дисплей не подключен или неправильно подключен");
            ESP_LOGE(TAG, "  2. Неправильные пины в config.h");
            ESP_LOGE(TAG, "  3. Проблема с питанием дисплея (проверьте VCC и GND)");
            ESP_LOGE(TAG, "  4. Неисправный дисплей");
            ESP_LOGE(TAG, "  5. Проблема с SPI шиной (SCLK/MOSI)");
        } else {
            ESP_LOGI(TAG, "Панель инициализирована успешно");
        }
        ESP_ERROR_CHECK(ret);
        // Задержка после инициализации
        vTaskDelay(pdMS_TO_TICKS(50));
        
        ESP_LOGI(TAG, "Инверсия цвета: true");
        ret = esp_lcd_panel_invert_color(panel_handle, true);
        ESP_ERROR_CHECK(ret);
        
        ESP_LOGI(TAG, "Зеркалирование: X=true, Y=false");
        ret = esp_lcd_panel_mirror(panel_handle, true, false);
        ESP_ERROR_CHECK(ret);
        
        ESP_LOGI(TAG, "Включение дисплея...");
        ret = esp_lcd_panel_disp_on_off(panel_handle, true);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "ОШИБКА включения дисплея: %s (0x%x)", esp_err_to_name(ret), ret);
        } else {
            ESP_LOGI(TAG, "Дисплей включен");
        }
        ESP_ERROR_CHECK(ret);

        ESP_LOGI(TAG, "Отправка дополнительных команд инициализации...");
        uint8_t data_0x62[] = { 0x18, 0x0D, 0x71, 0xED, 0x70, 0x70, 0x18, 0x0F, 0x71, 0xEF, 0x70, 0x70 };
        ret = esp_lcd_panel_io_tx_param(io_handle, 0x62, data_0x62, sizeof(data_0x62));
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  Команда 0x62 отправлена");
        } else {
            ESP_LOGW(TAG, "  Предупреждение при отправке 0x62: %s", esp_err_to_name(ret));
        }

        uint8_t data_0x63[] = { 0x18, 0x11, 0x71, 0xF1, 0x70, 0x70, 0x18, 0x13, 0x71, 0xF3, 0x70, 0x70 };
        ret = esp_lcd_panel_io_tx_param(io_handle, 0x63, data_0x63, sizeof(data_0x63));
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  Команда 0x63 отправлена");
        } else {
            ESP_LOGW(TAG, "  Предупреждение при отправке 0x63: %s", esp_err_to_name(ret));
        }

        uint8_t data_0x36[] = { 0x48};
        ret = esp_lcd_panel_io_tx_param(io_handle, 0x36, data_0x36, sizeof(data_0x36));
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  Команда 0x36 отправлена");
        } else {
            ESP_LOGW(TAG, "  Предупреждение при отправке 0x36: %s", esp_err_to_name(ret));
        }

        uint8_t data_0xC3[] = { 0x1F};
        ret = esp_lcd_panel_io_tx_param(io_handle, 0xC3, data_0xC3, sizeof(data_0xC3));
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  Команда 0xC3 отправлена");
        } else {
            ESP_LOGW(TAG, "  Предупреждение при отправке 0xC3: %s", esp_err_to_name(ret));
        }

        uint8_t data_0xC4[] = { 0x1F};
        ret = esp_lcd_panel_io_tx_param(io_handle, 0xC4, data_0xC4, sizeof(data_0xC4));
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  Команда 0xC4 отправлена");
        } else {
            ESP_LOGW(TAG, "  Предупреждение при отправке 0xC4: %s", esp_err_to_name(ret));
        }

        ESP_LOGI(TAG, "Создание объекта CustomLcdDisplay...");
        ESP_LOGI(TAG, "  Параметры: width=%d, height=%d, offset_x=%d, offset_y=%d", 
                 DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y);
        ESP_LOGI(TAG, "  mirror_x=%s, mirror_y=%s, swap_xy=%s", 
                 DISPLAY_MIRROR_X ? "true" : "false",
                 DISPLAY_MIRROR_Y ? "true" : "false",
                 DISPLAY_SWAP_XY ? "true" : "false");
        
#if CONFIG_DISPLAY_TYPE_SIMPLE_MEDIARISE
        // Создаем простой дисплей для прямой отрисовки
        ESP_LOGI(TAG, "Создание SimpleDisplay (без LVGL)...");
        simple_display_ = new SimpleDisplay(panel_handle, DISPLAY_WIDTH, DISPLAY_HEIGHT);
        if (simple_display_ && simple_display_->Init()) {
            ESP_LOGI(TAG, "SimpleDisplay успешно создан и инициализирован");
            
            // Рисуем тестовую картинку робота
            ESP_LOGI(TAG, "Рисование тестового робота...");
            simple_display_->DrawRobotBase();
            uint16_t yellow_eye = SimpleDisplay::Color565(255, 240, 90);
            simple_display_->DrawEyes(yellow_eye);
            ESP_LOGI(TAG, "Тестовый робот нарисован");
        } else {
            ESP_LOGE(TAG, "ОШИБКА создания SimpleDisplay!");
        }
        // Создаем адаптер Display для SimpleDisplay
        display_ = new SimpleDisplayAdapter(simple_display_);
        ESP_LOGI(TAG, "LVGL дисплей отключен (используется SimpleDisplay)");
#else
        // Создаем LVGL дисплей (полный UI)
        ESP_LOGI(TAG, "Создание CustomLcdDisplay для LVGL...");
        display_ = new CustomLcdDisplay(io_handle, panel_handle,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        
        if (display_ == nullptr) {
            ESP_LOGE(TAG, "КРИТИЧЕСКАЯ ОШИБКА: display_ == nullptr после создания!");
        } else {
            ESP_LOGI(TAG, "CustomLcdDisplay успешно создан, указатель: %p", display_);
        }
        // SimpleDisplay не создаем в этом режиме
        simple_display_ = nullptr;
        ESP_LOGI(TAG, "SimpleDisplay отключен (используется LVGL)");
#endif
        ESP_LOGI(TAG, "=== Инициализация GC9A01 завершена ===");
    }

    void InitializeButtons() {
        boot_button_.OnPressDown([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
                return;
            }
            app.StartListening();
        });
        boot_button_.OnPressUp([this]() {
            auto& app = Application::GetInstance();
            app.StopListening();
        });
    }

    void InitializeServoController() {
        ESP_LOGI(TAG, "Инициализация контроллера сервоприводов...");
        servo_controller_ = new ServoController();
        if (servo_controller_ && servo_controller_->Init()) {
            ESP_LOGI(TAG, "Контроллер сервоприводов инициализирован успешно");
        } else {
            ESP_LOGE(TAG, "Ошибка инициализации контроллера сервоприводов");
            if (servo_controller_) {
                delete servo_controller_;
                servo_controller_ = nullptr;
            }
        }
    }

    void RegisterMcpTools() {
        if (!servo_controller_) {
            ESP_LOGW(TAG, "ServoController не инициализирован, пропуск регистрации MCP инструментов");
            return;
        }

        auto& mcp_server = McpServer::GetInstance();
        ESP_LOGI(TAG, "Регистрация MCP инструментов для управления роботом...");

        // Управление отдельным сервоприводом
        mcp_server.AddTool("self.robot.set_servo",
            "Управление сервоприводом робота. Устанавливает угол поворота для указанного сервопривода.\n"
            "servo_num: номер сервопривода (1-10)\n"
            "angle: угол поворота (0-180 градусов)",
            PropertyList({
                Property("servo_num", kPropertyTypeInteger, 1, 1, 10),
                Property("angle", kPropertyTypeInteger, 90, 0, 180)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                int servo_num = properties["servo_num"].value<int>();
                int angle = properties["angle"].value<int>();
                if (servo_controller_ && servo_controller_->SetServoAngle(servo_num, angle)) {
                    ESP_LOGI(TAG, "Установлен сервопривод %d на угол %d", servo_num, angle);
                    return true;
                }
                return false;
            });

        // Установка позы робота
        mcp_server.AddTool("self.robot.set_pose",
            "Установка позы робота. Выполняет предустановленную позу.\n"
            "Доступные позы:\n"
            "- home/reset: домашняя поза (все сервоприводы в среднее положение)\n"
            "- wave/wave_hand: махать рукой\n"
            "- dance/dancing: танец\n"
            "- greet/greeting: приветствие (поднять руки)\n"
            "- sad/sad_pose: грустная поза (опустить руки)\n"
            "- happy/happy_pose: радостная поза (поднять руки вверх)",
            PropertyList({
                Property("pose", kPropertyTypeString)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                std::string pose = properties["pose"].value<std::string>();
                if (servo_controller_ && servo_controller_->SetPose(pose)) {
                    ESP_LOGI(TAG, "Выполнена поза: %s", pose.c_str());
                    return true;
                }
                return false;
            });

        // Управление несколькими сервоприводами одновременно
        mcp_server.AddTool("self.robot.move_servos",
            "Управление несколькими сервоприводами одновременно. "
            "servos: строка с командами в формате 'S1:45,S2:120,S3:90' (сервопривод:угол,разделитель запятая)",
            PropertyList({
                Property("servos", kPropertyTypeString)
            }),
            [this](const PropertyList& properties) -> ReturnValue {
                std::string servos_str = properties["servos"].value<std::string>();
                std::vector<std::pair<int, int>> commands;
                
                // Парсинг строки вида "S1:45,S2:120,S3:90"
                std::istringstream iss(servos_str);
                std::string token;
                bool all_ok = true;
                
                while (std::getline(iss, token, ',')) {
                    // Удаляем пробелы
                    token.erase(0, token.find_first_not_of(" \t"));
                    token.erase(token.find_last_not_of(" \t") + 1);
                    
                    // Парсим "S1:45"
                    if (token[0] == 'S' || token[0] == 's') {
                        size_t colon_pos = token.find(':');
                        if (colon_pos != std::string::npos) {
                            int servo_num = std::stoi(token.substr(1, colon_pos - 1));
                            int angle = std::stoi(token.substr(colon_pos + 1));
                            commands.push_back({servo_num, angle});
                        } else {
                            all_ok = false;
                        }
                    }
                }
                
                if (all_ok && servo_controller_ && servo_controller_->SetMultipleServos(commands)) {
                    ESP_LOGI(TAG, "Установлено %zu сервоприводов", commands.size());
                    return true;
                }
                return false;
            });

        ESP_LOGI(TAG, "MCP инструменты для управления роботом зарегистрированы");
    }

public:
    ESP32S3_MediaRise() : boot_button_(BOOT_BUTTON_GPIO) {
        // Сначала инициализировать I2C для сенсора и проверить/инициализировать сенсор (если сенсора нет, пропустить)
#if ENABLE_TOUCHPAD
        InitializeCodecI2c_Touch();
        InitializeCst816DTouchPad();
#else
        ESP_LOGI(TAG, "Touch отключен в конфигурации");
#endif

        // Инициализировать I2C для аудио (ES8311)
#if AUDIO_CODEC_TYPE_ES8311
        InitializeCodecI2c();
        DetectCodecAddress();
#endif

        // Сначала настроить всё, что связано с дисплеем
        // Диагностика физического подключения ПЕРЕД инициализацией
        DiagnoseDisplayPins();
        
        InitializeSpi();
        InitializeGc9a01Display();
        InitializeButtons();
        ESP_LOGI(TAG, "=== Инициализация подсветки ===");
        if (GetBacklight()) {
            ESP_LOGI(TAG, "Backlight найден, указатель: %p", GetBacklight());
            ESP_LOGI(TAG, "Backlight Pin: GPIO%d", DISPLAY_BACKLIGHT_PIN);
            ESP_LOGI(TAG, "Backlight Invert: %s", DISPLAY_BACKLIGHT_OUTPUT_INVERT ? "true" : "false");
            
            // ТЕСТ ПОДСВЕТКИ: мигание для проверки физического подключения
            ESP_LOGI(TAG, "ТЕСТ ПОДСВЕТКИ: мигание 3 раза...");
            for (int i = 0; i < 3; i++) {
                GetBacklight()->SetBrightness(100, false);
                vTaskDelay(pdMS_TO_TICKS(200));
                GetBacklight()->SetBrightness(0, false);
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            ESP_LOGI(TAG, "Тест подсветки завершён. Если подсветка не мигала - проверьте подключение GPIO%d", DISPLAY_BACKLIGHT_PIN);
            
            // Сразу установить яркость, чтобы убедиться, что подсветка работает (без ожидания плавного перехода)
            // Поскольку output_invert=true, нужно убедиться, что brightness > 0
            ESP_LOGI(TAG, "Установка начальной яркости: 75%%");
            GetBacklight()->SetBrightness(75, false);
            // Подождать завершения инициализации подсветки
            vTaskDelay(pdMS_TO_TICKS(300));
            ESP_LOGI(TAG, "Текущая яркость после установки: %d%%", GetBacklight()->brightness());
            
            // Затем восстановить сохранённую яркость
            ESP_LOGI(TAG, "Восстановление сохранённой яркости...");
            GetBacklight()->RestoreBrightness();
            ESP_LOGI(TAG, "Яркость после восстановления: %d%%", GetBacklight()->brightness());
            ESP_LOGI(TAG, "Подсветка инициализирована");
        } else {
            ESP_LOGE(TAG, "КРИТИЧЕСКАЯ ОШИБКА: Backlight == nullptr!");
            ESP_LOGE(TAG, "  Проверьте инициализацию PwmBacklight");
        }

        // Инициализировать логику энергосбережения после того, как дисплей и подсветка готовы, чтобы избежать нулевых указателей
        InitializePowerSaveTimer();
        InitializePowerManager();

        // Инициализировать управление сервоприводами
        InitializeServoController();

        // Регистрация MCP инструментов для управления роботом
        RegisterMcpTools();
    }

    ~ESP32S3_MediaRise() {
        if (servo_controller_) {
            delete servo_controller_;
            servo_controller_ = nullptr;
        }
        if (touchpad_timer_) {
            esp_timer_stop(touchpad_timer_);
            esp_timer_delete(touchpad_timer_);
            touchpad_timer_ = nullptr;
        }
        if (cst816d_) {
            delete cst816d_;
            cst816d_ = nullptr;
        }
        if (power_save_timer_) {
            delete power_save_timer_;
            power_save_timer_ = nullptr;
        }
        if (power_manager_) {
            delete power_manager_;
            power_manager_ = nullptr;
        }
        if (display_) {
            delete display_;
            display_ = nullptr;
        }
        if (simple_display_) {
            delete simple_display_;
            simple_display_ = nullptr;
        }
        if (i2c_bus_) {
            i2c_del_master_bus(i2c_bus_);
            i2c_bus_ = nullptr;
        }
        if (codec_i2c_bus_) {
            i2c_del_master_bus(codec_i2c_bus_);
            codec_i2c_bus_ = nullptr;
        }
    }


    virtual Led* GetLed() override {
        if (BUILTIN_LED_GPIO == GPIO_NUM_NC) {
            static NoLed led;
            return &led;
        }
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual Display* GetDisplay() override {
        if (display_ != nullptr) {
            return display_;
        }
        static NoDisplay no_display;
        return &no_display;
    }
    
    SimpleDisplay* GetSimpleDisplay() {
        return simple_display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual AudioCodec* GetAudioCodec() override {
#if AUDIO_CODEC_TYPE_PCM5101
        static NoAudioCodecSimplex no_audio(
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_SPK_GPIO_BCLK,
            AUDIO_I2S_SPK_GPIO_LRCK,
            AUDIO_I2S_SPK_GPIO_DOUT,
            I2S_STD_SLOT_LEFT,
            AUDIO_I2S_MIC_GPIO_SCK,
            AUDIO_I2S_MIC_GPIO_WS,
            AUDIO_I2S_MIC_GPIO_DIN,
            I2S_STD_SLOT_RIGHT);
        return &no_audio;
#else
        static NoAudioCodecDuplex no_audio(
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN);
        if (!codec_i2c_present_) {
            return &no_audio;
        }
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, codec_i2c_addr_);
        return &audio_codec;
#endif
    }

    Cst816d* GetTouchpad() {
        return cst816d_;
    }

    ServoController* GetServoController() {
        return servo_controller_;
    }

    virtual bool GetBatteryLevel(int& level, bool& charging, bool& discharging) override {
        if (!power_manager_) {
            level = 0;
            charging = false;
            discharging = true;
            return false;
        }
        
        static bool last_discharging = false;
        charging = power_manager_->IsCharging();
        discharging = power_manager_->IsDischarging();
        if (discharging != last_discharging) {
            power_save_timer_->SetEnabled(discharging);
            last_discharging = discharging;
        }
        level = power_manager_->GetBatteryLevel();
        return true;
    }

    virtual void SetPowerSaveMode(bool enabled) override {
        if (!enabled) {
            power_save_timer_->WakeUp();
        }
        WifiBoard::SetPowerSaveMode(enabled);
    }
};

DECLARE_BOARD(ESP32S3_MediaRise);
