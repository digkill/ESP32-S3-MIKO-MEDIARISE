#include "wifi_board.h"
#include "codecs/es8311_audio_codec.h"
#include "display/lcd_display.h"
#include "application.h"
#include "button.h"
#include "config.h"
#include "led/single_led.h"
#include <wifi_station.h>
#include <esp_log.h>
#include <driver/i2c_master.h>

#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_st7789.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include <driver/spi_common.h>

#define TAG "HZWDONE_ESP32_S3"

LV_FONT_DECLARE(font_puhui_16_4);
LV_FONT_DECLARE(font_awesome_16_4);

class Hzwdone_ESP32_S3 : public WifiBoard {
private:
    i2c_master_bus_handle_t codec_i2c_bus_ = nullptr;
    Button boot_button_;
    Display* display_ = nullptr;

    void InitializeCodecI2c() {
        // Initialize I2C peripheral for audio codec
        i2c_master_bus_config_t i2c_bus_cfg = {
            .i2c_port = I2C_NUM_0,
            .sda_io_num = AUDIO_CODEC_I2C_SDA_PIN,
            .scl_io_num = AUDIO_CODEC_I2C_SCL_PIN,
            .clk_source = I2C_CLK_SRC_DEFAULT,
        };
        ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_cfg, &codec_i2c_bus_));
    }

    void InitializeSpi() {
        ESP_LOGI(TAG, "Инициализация SPI шины для дисплея");
        
        spi_bus_config_t buscfg = {
            .mosi_io_num = DISPLAY_SPI_MOSI_PIN,
            .miso_io_num = GPIO_NUM_NC,
            .sclk_io_num = DISPLAY_SPI_SCLK_PIN,
            .quadwp_io_num = GPIO_NUM_NC,
            .quadhd_io_num = GPIO_NUM_NC,
            .max_transfer_sz = DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t),
        };
        
        ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
        ESP_LOGI(TAG, "SPI шина инициализирована");
    }

    void InitializeSt7789Display() {
        ESP_LOGI(TAG, "Инициализация дисплея ST7789 (2 дюйма, 320x240)");
        
        // Panel IO configuration
        esp_lcd_panel_io_spi_config_t io_config = {
            .cs_gpio_num = DISPLAY_SPI_CS_PIN,
            .dc_gpio_num = DISPLAY_SPI_DC_PIN,
            .spi_mode = 0,
            .pclk_hz = DISPLAY_SPI_SCLK_HZ,
            .trans_queue_depth = 10,
            .lcd_cmd_bits = 8,
            .lcd_param_bits = 8,
        };
        
        esp_lcd_panel_io_handle_t io_handle = nullptr;
        ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(SPI3_HOST, &io_config, &io_handle));
        ESP_LOGI(TAG, "Panel IO создан");

        // Panel configuration
        esp_lcd_panel_handle_t panel_handle = nullptr;
        esp_lcd_panel_dev_config_t panel_config = {};
        panel_config.reset_gpio_num = DISPLAY_SPI_RESET_PIN;
        panel_config.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
        panel_config.bits_per_pixel = 16;

        ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
        ESP_LOGI(TAG, "Панель ST7789 создана");

        // Initialize panel
        esp_lcd_panel_reset(panel_handle);
        esp_lcd_panel_init(panel_handle);
        esp_lcd_panel_invert_color(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y);
        esp_lcd_panel_swap_xy(panel_handle, DISPLAY_SWAP_XY);
        esp_lcd_panel_disp_on_off(panel_handle, true);
        ESP_LOGI(TAG, "Дисплей ST7789 инициализирован");

        // Create display
        display_ = new SpiLcdDisplay(io_handle, panel_handle,
                                    DISPLAY_WIDTH, DISPLAY_HEIGHT, 
                                    DISPLAY_OFFSET_X, DISPLAY_OFFSET_Y, 
                                    DISPLAY_MIRROR_X, DISPLAY_MIRROR_Y, 
                                    DISPLAY_SWAP_XY);
        
        if (display_ == nullptr) {
            ESP_LOGE(TAG, "ОШИБКА: display_ == nullptr после создания!");
        } else {
            ESP_LOGI(TAG, "SpiLcdDisplay успешно создан");
        }
    }

public:
    Hzwdone_ESP32_S3() : boot_button_(BOOT_BUTTON_GPIO) {
        ESP_LOGI(TAG, "Инициализация платы HZWDONE ESP32-S3");

        // Initialize I2C for audio codec
        InitializeCodecI2c();

        // Initialize audio codec
        auto codec = new Es8311AudioCodec(
            codec_i2c_bus_,
            I2C_NUM_0,
            AUDIO_INPUT_SAMPLE_RATE,
            AUDIO_OUTPUT_SAMPLE_RATE,
            AUDIO_I2S_GPIO_MCLK,
            AUDIO_I2S_GPIO_BCLK,
            AUDIO_I2S_GPIO_WS,
            AUDIO_I2S_GPIO_DOUT,
            AUDIO_I2S_GPIO_DIN,
            AUDIO_CODEC_PA_PIN,
            AUDIO_CODEC_ES8311_ADDR,
            true,  // use_mclk
            false  // pa_inverted
        );
        codec->SetInputGain(0);
        codec->SetOutputVolume(80);
        codec->EnableInput(true);
        codec->EnableOutput(true);
        SetAudioCodec(codec);
        ESP_LOGI(TAG, "Аудио кодек ES8311 инициализирован");

        // Initialize SPI and display
        InitializeSpi();
        InitializeSt7789Display();

        // Initialize buttons
        InitializeButtons();

        ESP_LOGI(TAG, "Плата HZWDONE ESP32-S3 инициализирована");
    }

    ~Hzwdone_ESP32_S3() {
        if (codec_i2c_bus_) {
            i2c_del_master_bus(codec_i2c_bus_);
            codec_i2c_bus_ = nullptr;
        }
        if (display_) {
            delete display_;
            display_ = nullptr;
        }
    }

    virtual Display* GetDisplay() override {
        return display_;
    }

    virtual Led* GetLed() override {
        static SingleLed led(BUILTIN_LED_GPIO);
        return &led;
    }

    void InitializeButtons() {
        boot_button_.OnPress([this]() {
            auto& app = Application::GetInstance();
            if (app.GetDeviceState() == kDeviceStateStarting &&
                !WifiStation::GetInstance().IsConnected()) {
                ResetWifiConfiguration();
            }
            app.ToggleChatState();
        });
        ESP_LOGI(TAG, "Кнопки инициализированы");
    }
};

DECLARE_BOARD(Hzwdone_ESP32_S3);

