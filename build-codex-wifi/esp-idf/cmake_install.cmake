# Install script for directory: /Users/digkill/.espressif/v6.0.1/esp-idf

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/Users/digkill/.espressif/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin/xtensa-esp32s3-elf-objdump")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/xtensa/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_stdio/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_dma/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_gpspi/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_clock/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_mspi/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_blockdev/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_security/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/bootloader/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esptool_py/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/partition_table/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_app_format/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_bootloader_format/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/app_update/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_partition/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/efuse/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_security/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_gpio/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_uart/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_pm/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_mm/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_dma/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/mbedtls/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_timg/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_wdt/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_ana_conv/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_rtc_timer/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/bootloader_support/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/spi_flash/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_usb_cdc_rom_console/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_system/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_common/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_rom/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/hal/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/log/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/heap/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/soc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_gpio/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_usb/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_pmu/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_touch_sens/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_i2s/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hw_support/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/freertos/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_libc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/pthread/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/cxx/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_timer/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_ringbuf/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_psram/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_uart/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_gptimer/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/app_trace/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_event/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/nvs_sec_provider/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/nvs_flash/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_phy/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_usb_serial_jtag/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/vfs/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/lwip/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_netif_stack/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_netif/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/wpa_supplicant/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_coex/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_wifi/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_spi/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_gdbstub/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/bt/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/unity/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/cmock/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/console/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_i2c/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_twai/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/driver/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/http_parser/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp-tls/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_adc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_blockdev_util/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_ana_cmpr/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_ana_cmpr/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_bitscrambler/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_cam/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_isp/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_cam/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_dac/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_i2c/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_i2s/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_i3c/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_jpeg/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_jpeg/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_ledc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_ledc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_mcpwm/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_mcpwm/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_parlio/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_parlio/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_pcnt/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_pcnt/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_ppa/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_ppa/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_rmt/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_rmt/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_sd/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/sdmmc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_sd_intf/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_sdio/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_sdm/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_sdmmc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_sdspi/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_touch_sens/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_tsens/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_driver_twai/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_eth/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_ieee802154/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hal_lcd/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_hid/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/tcp_transport/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_http_client/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_http_server/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_https_ota/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_https_server/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_lcd/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/protobuf-c/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/protocomm/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_local_ctrl/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/esp_trace/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espcoredump/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/wear_levelling/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/fatfs/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/idf_test/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/ieee802154/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/openthread/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/perfmon/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/rt/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/spiffs/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/ulp/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__mqtt/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/78__esp-ml307/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/78__esp-opus/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/78__esp-opus-encoder/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__cjson/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/json/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/78__esp-wifi-connect/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__cmake_utilities/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/78__esp_lcd_nv3023/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/lvgl__lvgl/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/78__xiaozhi-fonts/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__adc_battery_estimation/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_codec_dev/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__adc_mic/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__button/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__dl_fft/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp-dsp/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp-sr/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_image_effects/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_io_expander/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_io_expander_tca9554/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_io_expander_tca95xx_16bit/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_lcd_touch/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_lcd_axs15231b/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_lcd_gc9a01/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_lcd_ili9341/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_lcd_panel_io_additions/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_lcd_spd2010/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_lcd_st7701/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_lcd_st77916/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_lcd_st7796/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_lcd_touch_cst816s/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_lcd_touch_ft5x06/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_lcd_touch_gt1151/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_lcd_touch_gt911/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_lcd_touch_st7123/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_lvgl_port/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_mmap_assets/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_new_jpeg/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_sccb_intf/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_cam_sensor/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__usb/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__usb_host_uvc/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__esp_video/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__knob/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__led_strip/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif__freetype/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif2022__esp_emote_gfx/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/espressif2022__image_player/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/tny-robotics__sh1106-esp-idf/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/txp666__otto-emoji-gif-component/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/waveshare__esp_lcd_sh8601/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/waveshare__esp_lcd_touch_cst9217/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/waveshare__qmi8658/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/wvirgil123__sscma_client/cmake_install.cmake")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for the subdirectory.
  include("/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/main/cmake_install.cmake")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "/Users/digkill/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/build-codex-wifi/esp-idf/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
