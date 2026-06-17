# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "2.0.5")

# Add this line to disable the specific warning
add_compile_options(-Wno-missing-field-initializers)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(xiaozhi)

execute_process(
    COMMAND bash "${CMAKE_SOURCE_DIR}/scripts/apply_esp_ml307_websocket_fix.sh"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    RESULT_VARIABLE ESP_ML307_WEBSOCKET_PATCH_RESULT
)
if(NOT ESP_ML307_WEBSOCKET_PATCH_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to apply esp-ml307 websocket watchdog patch")
endif()

if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    if(TARGET __idf_espressif__esp-dsp)
        target_compile_options(__idf_espressif__esp-dsp PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-include>
            $<$<COMPILE_LANGUAGE:CXX>:cmath>
        )
    endif()

    foreach(lcd_component
            78__esp_lcd_nv3023
            espressif__esp_lcd_axs15231b
            espressif__esp_lcd_gc9a01
            espressif__esp_lcd_ili9341
            espressif__esp_lcd_st7701
            espressif__esp_lcd_st7796)
        if(TARGET __idf_${lcd_component})
            target_compile_definitions(__idf_${lcd_component} PRIVATE
                color_space=rgb_ele_order
                rgb_endian=rgb_ele_order
                ESP_LCD_COLOR_SPACE_RGB=LCD_RGB_ELEMENT_ORDER_RGB
                ESP_LCD_COLOR_SPACE_BGR=LCD_RGB_ELEMENT_ORDER_BGR
                LCD_RGB_ENDIAN_RGB=LCD_RGB_ELEMENT_ORDER_RGB
                LCD_RGB_ENDIAN_BGR=LCD_RGB_ELEMENT_ORDER_BGR
            )
        endif()
    endforeach()

    if(TARGET __idf_espressif__esp_io_expander_tca9554 AND TARGET idf::esp_driver_i2c)
        target_link_libraries(__idf_espressif__esp_io_expander_tca9554 PUBLIC idf::esp_driver_i2c)
    endif()

    if(TARGET __idf_waveshare__qmi8658 AND TARGET idf::esp_driver_i2c)
        target_link_libraries(__idf_waveshare__qmi8658 PUBLIC idf::esp_driver_i2c)
    endif()

    if(TARGET __idf_wvirgil123__sscma_client)
        target_link_libraries(__idf_wvirgil123__sscma_client PRIVATE
            idf::esp_driver_gpio
            idf::esp_driver_i2c
            idf::esp_driver_spi
            idf::esp_driver_uart
        )
    endif()
endif()
