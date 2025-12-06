#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

// HZWDONE ESP32-S3 Board configuration
// Плата разработки с экраном 2 дюйма

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

// I2S Audio pins
#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_16  // MCLK
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_45  // LRCK
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_9   // SCLK
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_10  // DOUT
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_8   // DIN

// Audio Codec (ES8311)
#define AUDIO_CODEC_PA_PIN       GPIO_NUM_46
#define AUDIO_CODEC_I2C_SDA_PIN  GPIO_NUM_15
#define AUDIO_CODEC_I2C_SCL_PIN  GPIO_NUM_14
#define AUDIO_CODEC_ES8311_ADDR  ES8311_CODEC_DEFAULT_ADDR

// Buttons and LEDs
#define BUILTIN_LED_GPIO        GPIO_NUM_48
#define BOOT_BUTTON_GPIO        GPIO_NUM_0

// Display configuration (2 inch, typically 320x240)
#define DISPLAY_WIDTH   320
#define DISPLAY_HEIGHT  240
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

// Display SPI pins
#define DISPLAY_SPI_SCLK_PIN    GPIO_NUM_4
#define DISPLAY_SPI_MOSI_PIN    GPIO_NUM_2
#define DISPLAY_SPI_CS_PIN      GPIO_NUM_5
#define DISPLAY_SPI_DC_PIN      GPIO_NUM_47
#define DISPLAY_SPI_RESET_PIN   GPIO_NUM_38

// Display backlight
#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_42
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

// Touchpad pins (if available)
#define TP_PIN_NUM_TP_SDA   (GPIO_NUM_11)
#define TP_PIN_NUM_TP_SCL   (GPIO_NUM_7)
#define TP_PIN_NUM_TP_RST   (GPIO_NUM_6)
#define TP_PIN_NUM_TP_INT   (GPIO_NUM_12)

// SPI frequency
#define DISPLAY_SPI_SCLK_HZ     (40 * 1000 * 1000)

// Battery monitoring (if available)
#define BATTERY_ADC_PIN         GPIO_NUM_1   // ADC pin for battery voltage
#define BATTERY_CHARGING_PIN    GPIO_NUM_41  // Pin for charging status

#endif // _BOARD_CONFIG_H_




