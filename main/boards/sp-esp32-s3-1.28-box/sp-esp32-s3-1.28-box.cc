#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
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

#define TAG "Spotpear_ESP32_S3_1_28_BOX"

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


class Spotpear_ESP32_S3_1_28_BOX : public WifiBoard {
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
            // .glitch_ignore_cnt = 7,
            // .intr_priority = 0,
            // .trans_queue_depth = 0,
            // .flags = {
            //     .enable_internal_pullup = 1,
            // },
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
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
        auto* board = static_cast<Spotpear_ESP32_S3_1_28_BOX*>(arg);
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

        // Инициализация выводов RST/INT
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_DISABLE;
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = (1ULL << TP_PIN_NUM_TP_RST);
        io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io_conf);

        gpio_config_t int_conf = {};
        int_conf.intr_type = GPIO_INTR_DISABLE;
        int_conf.mode = GPIO_MODE_INPUT;
        int_conf.pin_bit_mask = (1ULL << TP_PIN_NUM_TP_INT);
        int_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
        int_conf.pull_up_en = GPIO_PULLUP_ENABLE;
        gpio_config(&int_conf);

        // Последовательность сброса сенсорного чипа
        gpio_set_level(TP_PIN_NUM_TP_RST, 0);
        vTaskDelay(pdMS_TO_TICKS(5));
        gpio_set_level(TP_PIN_NUM_TP_RST, 1);
        vTaskDelay(pdMS_TO_TICKS(50));

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
        
        display_ = new CustomLcdDisplay(io_handle, panel_handle,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, DISPLAY_SWAP_XY);
        
        if (display_ == nullptr) {
            ESP_LOGE(TAG, "КРИТИЧЕСКАЯ ОШИБКА: display_ == nullptr после создания!");
        } else {
            ESP_LOGI(TAG, "CustomLcdDisplay успешно создан, указатель: %p", display_);
        }
        ESP_LOGI(TAG, "=== Инициализация GC9A01 завершена ===");
    }

    void InitializeButtons() {
        boot_button_.OnClick([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting && !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
    }

public:
    Spotpear_ESP32_S3_1_28_BOX() : boot_button_(BOOT_BUTTON_GPIO) {
        // Сначала инициализировать I2C для сенсора и проверить/инициализировать сенсор (если сенсора нет, пропустить)
        InitializeCodecI2c_Touch();
        InitializeCst816DTouchPad();

        // Инициализировать I2C для аудио
        InitializeCodecI2c();

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
    }

    ~Spotpear_ESP32_S3_1_28_BOX() {
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
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Backlight* GetBacklight() override {
        static PwmBacklight backlight(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_OUTPUT_INVERT);
        return &backlight;
    }

    virtual AudioCodec* GetAudioCodec() override {
        static Es8311AudioCodec audio_codec(codec_i2c_bus_, I2C_NUM_0, AUDIO_INPUT_SAMPLE_RATE, AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK, AUDIO_I2S_GPIO_BCLK, AUDIO_I2S_GPIO_WS, AUDIO_I2S_GPIO_DOUT, AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN, AUDIO_CODEC_ES8311_ADDR);
        return &audio_codec;
    }

    Cst816d* GetTouchpad() {
        return cst816d_;
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

DECLARE_BOARD(Spotpear_ESP32_S3_1_28_BOX);
