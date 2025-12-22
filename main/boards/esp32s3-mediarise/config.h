#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <driver/spi_master.h>

/* --------- AUDIO (ES8311) --------- */
#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000
#define AUDIO_CODEC_TYPE_PCM5101 1
#define AUDIO_CODEC_TYPE_ES8311  0

// WebSocket protocol v3: send raw Opus frames by default.
#define DEFAULT_WEBSOCKET_BP3_HEADER 0

#define BOOT_BUTTON_GPIO GPIO_NUM_0
#define PWR_BUTTON_GPIO  GPIO_NUM_NC
#define PWR_Control_PIN  GPIO_NUM_NC

// I2S pins (ESP32-S3-Touch-LCD-1.46B: PCM5101 + I2S mic)
#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_NC
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_48
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_38
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_47
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_39

// Compatibility aliases used elsewhere
#define AUDIO_I2S_MIC_GPIO_WS  GPIO_NUM_2
#define AUDIO_I2S_MIC_GPIO_SCK GPIO_NUM_15
#define AUDIO_I2S_MIC_GPIO_DIN GPIO_NUM_39
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_47
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_48
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_38

#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_11
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_10
#define AUDIO_CODEC_PA_PIN      GPIO_NUM_NC
#define AUDIO_CODEC_ES8311_ADDR 0x18
#define AUDIO_CODEC_ES8311_ADDR_ALT 0x19

#define ENABLE_TOUCHPAD 0

/* Optional IMU I2C (unused on this board) */
#define I2C_SCL_IO GPIO_NUM_NC
#define I2C_SDA_IO GPIO_NUM_NC

#define I2C_ADDRESS ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000

/* --------- DISPLAY GC9A01 (SPI) --------- */
#define DISPLAY_SPI_HOST     SPI3_HOST
#define DISPLAY_SPI_SCLK_PIN GPIO_NUM_4
#define DISPLAY_SPI_MOSI_PIN GPIO_NUM_2
#define DISPLAY_SPI_CS_PIN   GPIO_NUM_5
#define DISPLAY_SPI_DC_PIN   GPIO_NUM_47
#define DISPLAY_SPI_RST_PIN  GPIO_NUM_38
#define DISPLAY_SPI_BL_PIN   GPIO_NUM_42
#define DISPLAY_SPI_CLOCK_HZ (40 * 1000 * 1000)

#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY  false

#define DISPLAY_SPI_RESET_PIN DISPLAY_SPI_RST_PIN
#define DISPLAY_SPI_SCLK_HZ   DISPLAY_SPI_CLOCK_HZ

#define QSPI_LCD_H_RES         (240)
#define QSPI_LCD_V_RES         (240)
#define QSPI_LCD_BIT_PER_PIXEL (16)

#define QSPI_LCD_HOST          SPI3_HOST
#define QSPI_PIN_NUM_LCD_PCLK  DISPLAY_SPI_SCLK_PIN
#define QSPI_PIN_NUM_LCD_CS    DISPLAY_SPI_CS_PIN
#define QSPI_PIN_NUM_LCD_DATA0 DISPLAY_SPI_MOSI_PIN

#define QSPI_PIN_NUM_LCD_DATA1 GPIO_NUM_NC
#define QSPI_PIN_NUM_LCD_DATA2 GPIO_NUM_NC
#define QSPI_PIN_NUM_LCD_DATA3 GPIO_NUM_NC

#define QSPI_PIN_NUM_LCD_RST DISPLAY_SPI_RST_PIN
#define QSPI_PIN_NUM_LCD_BL  DISPLAY_SPI_BL_PIN

#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 0

/* --------- TOUCH CST816D (I2C) --------- */
#define TP_PORT        (I2C_NUM_1)
#define TP_PIN_NUM_SDA GPIO_NUM_11
#define TP_PIN_NUM_SCL GPIO_NUM_7
#define TP_PIN_NUM_RST GPIO_NUM_6
#define TP_PIN_NUM_INT GPIO_NUM_12

#define TP_PIN_NUM_TP_SDA TP_PIN_NUM_SDA
#define TP_PIN_NUM_TP_SCL TP_PIN_NUM_SCL
#define TP_PIN_NUM_TP_RST TP_PIN_NUM_RST
#define TP_PIN_NUM_TP_INT TP_PIN_NUM_INT

/* Backlight */
#define DISPLAY_BACKLIGHT_PIN           DISPLAY_SPI_BL_PIN
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

/* Servo UART */
#define SERVO_UART_PORT_NUM UART_NUM_1
#define SERVO_UART_TX_PIN   GPIO_NUM_17
#define SERVO_UART_RX_PIN   GPIO_NUM_18
#define SERVO_UART_BAUD_RATE 115200

/* Other */
#define BUILTIN_LED_GPIO     GPIO_NUM_NC
#define BATTERY_CHARGING_PIN GPIO_NUM_41

/* QSPI config macro (DATA1/DATA2/DATA3 unused in SPI mode) */
#define TAIJIPI_SPD2010_PANEL_BUS_QSPI_CONFIG(sclk, d0, d1, d2, d3, max_trans_sz) \
    {                                                                             \
        .data0_io_num = d0,                                                       \
        .data1_io_num = d1,                                                       \
        .sclk_io_num = sclk,                                                      \
        .data2_io_num = d2,                                                       \
        .data3_io_num = d3,                                                       \
        .max_transfer_sz = max_trans_sz,                                          \
    }

#endif // _BOARD_CONFIG_H_
