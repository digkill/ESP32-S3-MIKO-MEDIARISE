#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <driver/uart.h>

#define AUDIO_INPUT_SAMPLE_RATE 24000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define AUDIO_INPUT_REFERENCE    true

#define AUDIO_I2S_GPIO_MCLK GPIO_NUM_16
#define AUDIO_I2S_GPIO_WS GPIO_NUM_45
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_41
#define AUDIO_I2S_GPIO_DIN GPIO_NUM_42
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_40

#define AUDIO_CODEC_PA_PIN GPIO_NUM_46
#define AUDIO_CODEC_I2C_SDA_PIN GPIO_NUM_15
#define AUDIO_CODEC_I2C_SCL_PIN GPIO_NUM_14
#define AUDIO_CODEC_ES8311_ADDR ES8311_CODEC_DEFAULT_ADDR
#define AUDIO_CODEC_ES7210_ADDR  ES7210_CODEC_DEFAULT_ADDR

#define BOOT_BUTTON_GPIO GPIO_NUM_0

#define EXAMPLE_PIN_NUM_LCD_CS GPIO_NUM_12
#define EXAMPLE_PIN_NUM_LCD_PCLK GPIO_NUM_11
#define EXAMPLE_PIN_NUM_LCD_DATA0 GPIO_NUM_4
#define EXAMPLE_PIN_NUM_LCD_DATA1 GPIO_NUM_5
#define EXAMPLE_PIN_NUM_LCD_DATA2 GPIO_NUM_6
#define EXAMPLE_PIN_NUM_LCD_DATA3 GPIO_NUM_7
#define EXAMPLE_PIN_NUM_LCD_RST GPIO_NUM_8
#define DISPLAY_WIDTH 410
#define DISPLAY_HEIGHT 502
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false

#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

#define DISPLAY_BACKLIGHT_PIN GPIO_NUM_NC
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

// Waveshare's Arduino examples define TP_RESET=9 but do not pass it to the
// FT3168 driver. Leave it unmanaged unless a board revision explicitly needs it.
#define TOUCH_RST_PIN GPIO_NUM_9
#define TOUCH_RST_ENABLED 0
#define TOUCH_INT_PIN GPIO_NUM_38

#define SDCARD_MOUNT_POINT "/sdcard"
#define SDCARD_CLK_PIN GPIO_NUM_2
#define SDCARD_CMD_PIN GPIO_NUM_1
#define SDCARD_D0_PIN GPIO_NUM_3

#define IMU_QMI8658_ADDR QMI8658_ADDRESS_HIGH

/*
 * Optional external vibration motor control. The Waveshare 2.06 documentation
 * lists an IMU but no onboard motor. Drive a motor through a transistor/MOSFET
 * and enable/set the selected control pin after wiring is known.
 */
#define VIBRATION_MOTOR_ENABLED    0
#define VIBRATION_MOTOR_GPIO       GPIO_NUM_NC
#define VIBRATION_MOTOR_ACTIVE_HIGH true

/*
 * External Seeed XIAO ESP32-S3 camera/servo/LED/proximity bridge.
 * Wire Waveshare TX -> XIAO RX and Waveshare RX -> XIAO TX.
 */
#define XIAO_UART_PORT_NUM       UART_NUM_1
#define XIAO_UART_TX_PIN         GPIO_NUM_17
#define XIAO_UART_RX_PIN         GPIO_NUM_18
/* 921600 often fails on jumper wires; use the same value on the XIAO bridge (LINK_UART_BAUD). */
#define XIAO_UART_BAUD_RATE      115200

/*
 * Wireless XIAO control link. ESP-NOW commands share the station Wi-Fi channel;
 * keep both S3 boards associated with the same 2.4 GHz AP.
 * UART stays available as fallback and for JPEG frame transfer.
 */
#define XIAO_ESPNOW_ENABLED      1

#define SERVO_HEAD_YAW_NUM       1
#define SERVO_HEAD_PITCH_NUM     2

#define SERVO_HOME_ANGLE         60
#define SERVO_MAX_DEVIATION      20
#define SERVO_MIN_ANGLE          (SERVO_HOME_ANGLE - SERVO_MAX_DEVIATION)
#define SERVO_MAX_ANGLE          (SERVO_HOME_ANGLE + SERVO_MAX_DEVIATION)

#define XIAO_LED_DEFAULT_R       0
#define XIAO_LED_DEFAULT_G       120
#define XIAO_LED_DEFAULT_B       255

#endif // _BOARD_CONFIG_H_
