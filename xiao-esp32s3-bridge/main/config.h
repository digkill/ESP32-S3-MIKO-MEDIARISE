#pragma once

// ── Local overrides (credentials, feature flags; not committed to git) ───────
#if __has_include("config_local.h")
#include "config_local.h"
#endif

// ── Board: AI-Thinker ESP32-CAM (classic ESP32) ──────────────────────────────
// Camera node with head peripherals. Free GPIOs on this module are scarce
// (camera + flash + UART0 take the rest): servos on 13 (yaw) / 15 (pitch) —
// NOT 12, that strapping pin selects the flash voltage at reset. WS2812 ring
// on 4 (shares the net with the onboard flash LED). C1001 radar: its TX →
// GPIO14, its RX → GPIO2. VL53 stays off until wired.
// Build with -DBOARD_ESP32CAM=1 (see main/CMakeLists.txt).
#ifdef BOARD_ESP32CAM
#undef ENABLE_VL53
#undef ENABLE_C1001
#undef ENABLE_SERVOS
#undef ENABLE_SD_RECORDING
#undef ENABLE_LED_RING
#define ENABLE_VL53          0
#define ENABLE_C1001         1
#define ENABLE_SERVOS        1
#define ENABLE_SD_RECORDING  0
#define ENABLE_LED_RING      1
#define SERVO_YAW_GPIO       13
#define SERVO_PITCH_GPIO     15
#define LED_RING_GPIO        4
#define C1001_TX_GPIO        2
#define C1001_RX_GPIO        14
#ifndef WIFI_HOSTNAME
#define WIFI_HOSTNAME "esp32-cam"
#endif
// OV2640 pin map (CAMERA_MODEL_AI_THINKER)
#define CAM_PIN_PWDN   32
#define CAM_PIN_RESET  -1
#define CAM_PIN_XCLK   0
#define CAM_PIN_SIOD   26
#define CAM_PIN_SIOC   27
#define CAM_PIN_D7     35
#define CAM_PIN_D6     34
#define CAM_PIN_D5     39
#define CAM_PIN_D4     36
#define CAM_PIN_D3     21
#define CAM_PIN_D2     19
#define CAM_PIN_D1     18
#define CAM_PIN_D0     5
#define CAM_PIN_VSYNC  25
#define CAM_PIN_HREF   23
#define CAM_PIN_PCLK   22
#endif // BOARD_ESP32CAM

// ── Feature flags ─────────────────────────────────────────────────────────────
#ifndef ENABLE_CAMERA
#define ENABLE_CAMERA 1
#endif
#ifndef ENABLE_LED_RING
#define ENABLE_LED_RING 1
#endif
#ifndef ENABLE_VL53
#define ENABLE_VL53 1
#endif
#ifndef ENABLE_VL53_SCAN_ON_FAIL
#define ENABLE_VL53_SCAN_ON_FAIL 0
#endif
#ifndef ENABLE_SERVOS
#define ENABLE_SERVOS 1
#endif
#ifndef ENABLE_C1001
#define ENABLE_C1001 1
#endif
#ifndef ENABLE_SD_RECORDING
#define ENABLE_SD_RECORDING 1
#endif
#ifndef ENABLE_ESPNOW
#define ENABLE_ESPNOW 1
#endif
#ifndef ENABLE_UART_LINK
#define ENABLE_UART_LINK 0
#endif
#ifndef ESPNOW_FALLBACK_CHANNEL
#define ESPNOW_FALLBACK_CHANNEL 1
#endif
#ifndef XIAO_POWER_SAVE_TIMEOUT_MS
#define XIAO_POWER_SAVE_TIMEOUT_MS (30UL * 60UL * 1000UL)
#endif
#ifndef ENABLE_VISION_UPLOAD
#define ENABLE_VISION_UPLOAD 1
#endif
#ifndef VISION_UPLOAD_INTERVAL_MS
#define VISION_UPLOAD_INTERVAL_MS 15000UL
#endif
#ifndef VISION_UPLOAD_URL
#define VISION_UPLOAD_URL "http://90.156.254.46:8080/api/robot/vision/frame"
#endif
#ifndef VISION_DEVICE_ID
#define VISION_DEVICE_ID "homebot-xiao-camera"
#endif

// ── WiFi credentials ──────────────────────────────────────────────────────────
#ifndef WIFI_SSID
#define WIFI_SSID "muza 2.4"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "s1h4sr39@nlwg43wd"
#endif
#ifndef WIFI_HOSTNAME
#define WIFI_HOSTNAME "miko-cam"
#endif
#define WIFI_CONNECT_TIMEOUT_MS 25000

// ── GPIO: XIAO ESP32-S3 Sense pin map ─────────────────────────────────────────
#define GPIO_D0   1
#define GPIO_D1   2
#define GPIO_D2   3
#define GPIO_D3   4
#define GPIO_D4   5
#define GPIO_D5   6
#define GPIO_D6   43
#define GPIO_D7   44
#define GPIO_D8   7
#define GPIO_D9   8
#define GPIO_D10  9

// ── LED ring (WS2812B ×8; D10 на XIAO = GPIO9, не GPIO10) ───────────────────
#ifndef LED_RING_GPIO
#define LED_RING_GPIO        GPIO_D10   // GPIO9
#endif
#define LED_RING_COUNT       8
#define LED_RING_RMT_CHANNEL 0
#define LED_DEFAULT_R        0
#define LED_DEFAULT_G        120
#define LED_DEFAULT_B        255

// ── I2C (VL53L0X on D4/D5) ───────────────────────────────────────────────────
#define I2C_PORT    0
#define I2C_SDA_GPIO  GPIO_D4
#define I2C_SCL_GPIO  GPIO_D5
#define I2C_FREQ_HZ   100000

// ── UART0 (legacy link, not used when ESP-NOW is active) ──────────────────────
#define LINK_UART_NUM   UART_NUM_2
#define LINK_TX_GPIO    GPIO_D2
#define LINK_RX_GPIO    GPIO_D3
#define LINK_UART_BAUD  115200

// ── UART1 (DFRobot C1001 radar on D6/D7) ─────────────────────────────────────
// D6/D7 = GPIO43/44 = USB Serial/JTAG on ESP32-S3. Firmware releases that
// driver before opening UART1; USB monitor stops after boot (~3 s).
#define C1001_UART_NUM  UART_NUM_1
#ifndef C1001_TX_GPIO
#define C1001_TX_GPIO   GPIO_D6
#define C1001_RX_GPIO   GPIO_D7
#endif
#define C1001_UART_BAUD 115200

// ── Servos (LEDC PWM) ─────────────────────────────────────────────────────────
#ifndef SERVO_YAW_GPIO
#define SERVO_YAW_GPIO      GPIO_D1
#define SERVO_PITCH_GPIO    GPIO_D2
#endif
#define SERVO_LEDC_TIMER    LEDC_TIMER_0
#define SERVO_LEDC_MODE     LEDC_LOW_SPEED_MODE
#define SERVO_LEDC_CH_YAW   LEDC_CHANNEL_6
#define SERVO_LEDC_CH_PITCH LEDC_CHANNEL_7
#define SERVO_PWM_FREQ_HZ   50
#define SERVO_PWM_RES_BITS  LEDC_TIMER_14_BIT
#define SERVO_MIN_US        500
#define SERVO_MAX_US        2500
#define SERVO_HOME_ANGLE    60
#define SERVO_MAX_DEVIATION 20
#define SERVO_MIN_ANGLE     (SERVO_HOME_ANGLE - SERVO_MAX_DEVIATION)
#define SERVO_MAX_ANGLE     (SERVO_HOME_ANGLE + SERVO_MAX_DEVIATION)
#define SERVO_CALIB_MIN_ANGLE 0
#define SERVO_CALIB_MAX_ANGLE 120
#ifndef SERVO_DUTY_QUANTUM
#define SERVO_DUTY_QUANTUM  8
#endif
#ifndef SERVO_RELAX_MS
#define SERVO_RELAX_MS      0
#endif
#ifndef SERVO_MOVE_INTERVAL_MS
#define SERVO_MOVE_INTERVAL_MS 20
#endif
#ifndef SERVO_STEP_DEG
#define SERVO_STEP_DEG      1
#endif

// ── Camera pin map (XIAO ESP32-S3 Sense; ESP32-CAM overrides above) ──────────
#ifndef CAM_PIN_XCLK
#define CAM_PIN_PWDN   -1
#define CAM_PIN_RESET  -1
#define CAM_PIN_XCLK   10
#define CAM_PIN_SIOD   40
#define CAM_PIN_SIOC   39
#define CAM_PIN_D7     48
#define CAM_PIN_D6     11
#define CAM_PIN_D5     12
#define CAM_PIN_D4     14
#define CAM_PIN_D3     16
#define CAM_PIN_D2     18
#define CAM_PIN_D1     17
#define CAM_PIN_D0     15
#define CAM_PIN_VSYNC  38
#define CAM_PIN_HREF   47
#define CAM_PIN_PCLK   13
#endif // CAM_PIN_XCLK

// ── ESP-NOW protocol ──────────────────────────────────────────────────────────
#define ESPNOW_MAGIC         0x574e4248u
#define ESPNOW_VERSION       1
#define ESPNOW_COMMAND       1
#define ESPNOW_RESPONSE      2
#define ESPNOW_PAYLOAD_SIZE  192
#define ESPNOW_QUEUE_LEN     6

// ── Heartbeat / timers ────────────────────────────────────────────────────────
#define HEARTBEAT_INTERVAL_MS  5000
#define STREAM_DEFAULT_MS      500

// Fallback: single-AP from WIFI_SSID/WIFI_PASSWORD if no config_local.h
#ifndef WIFI_AP_COUNT
#define WIFI_AP_COUNT   1
#define WIFI_AP_1_SSID  WIFI_SSID
#define WIFI_AP_1_PASS  WIFI_PASSWORD
#endif
