set(BRIDGE_REQUIRES
    esp_wifi
    esp_event
    esp_netif
    nvs_flash
    esp_http_server
    esp_http_client
    esp_timer
    esp_psram
    driver
    esp_driver_gpio
    esp_driver_uart
    esp_driver_i2c
    esp_driver_ledc
    freertos
    log
    espressif__mdns
    espressif__esp32-camera
    espressif__led_strip
)

# Classic ESP32 (AI-Thinker ESP32-CAM) has no USB-Serial/JTAG peripheral.
if(NOT IDF_TARGET STREQUAL "esp32")
    list(APPEND BRIDGE_REQUIRES esp_driver_usb_serial_jtag)
endif()

idf_component_register(
    SRCS
        "main.cc"
        "camera.cc"
        "led_ring.cc"
        "servo.cc"
        "vl53l0x.cc"
        "c1001.cc"
        "espnow_bridge.cc"
        "http_server.cc"
        "command.cc"
        "vision_upload.cc"
    INCLUDE_DIRS "."
    REQUIRES ${BRIDGE_REQUIRES}
)

# Camera-only build for the AI-Thinker ESP32-CAM board:
#   idf.py -B build-esp32cam -D SDKCONFIG=sdkconfig.esp32cam \
#     -D SDKCONFIG_DEFAULTS=sdkconfig.esp32cam.defaults -D BOARD_ESP32CAM=1 build
if(BOARD_ESP32CAM)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE BOARD_ESP32CAM=1)
endif()
