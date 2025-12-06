#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>
#include <driver/spi_master.h>

/* --------- АУДИО (оставил как было) --------- */
#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 24000

#define BOOT_BUTTON_GPIO        GPIO_NUM_0
#define PWR_BUTTON_GPIO         GPIO_NUM_6
#define PWR_Control_PIN         GPIO_NUM_7

#define AUDIO_I2S_MIC_GPIO_WS   GPIO_NUM_2
#define AUDIO_I2S_MIC_GPIO_SCK  GPIO_NUM_15
#define AUDIO_I2S_MIC_GPIO_DIN  GPIO_NUM_39
#define AUDIO_I2S_SPK_GPIO_DOUT GPIO_NUM_47
#define AUDIO_I2S_SPK_GPIO_BCLK GPIO_NUM_48
#define AUDIO_I2S_SPK_GPIO_LRCK GPIO_NUM_38

/* ------------------------------------------------------------------
 *  I2C ДЛЯ ТАЧСКРИНА CST816D (ВНЕШНИЙ ДИСПЛЕЙ)
 *
 *  TP_SDA → GPIO13
 *  TP_SCL → GPIO15
 *  TP_INT → GPIO4
 * ------------------------------------------------------------------ */
#define I2C_SCL_IO          GPIO_NUM_15      // SCL тача CST816D
#define I2C_SDA_IO          GPIO_NUM_13      // SDA тача CST816D

#define I2C_ADDRESS         ESP_IO_EXPANDER_I2C_TCA9554_ADDRESS_000  // можешь не использовать, если экспандера нет

/* ------------------------------------------------------------------
 *  ВНЕШНИЙ 2" ДИСПЛЕЙ ST7789T3 (SPI)
 *  ФИЗИЧЕСКОЕ ПОДКЛЮЧЕНИЕ (как мы делали в Arduino):
 *
 *  LCD_MOSI → GPIO14
 *  LCD_SCLK → GPIO16
 *  LCD_CS   → GPIO17
 *  LCD_DC   → (настраивается в коде, тут не нужен)
 *  LCD_RST  → GPIO12
 *  LCD_BL   → GPIO1
 * ------------------------------------------------------------------ */

#define DISPLAY_WIDTH       240
#define DISPLAY_HEIGHT      320
#define DISPLAY_MIRROR_X    false
#define DISPLAY_MIRROR_Y    false
#define DISPLAY_SWAP_XY     false

#define QSPI_LCD_H_RES           (240)
#define QSPI_LCD_V_RES           (320)
#define QSPI_LCD_BIT_PER_PIXEL   (16)

/*
 * Мы по-прежнему используем SPI2_HOST, но
 * трактуем "QSPI" как обычный SPI:
 *
 * QSPI_PIN_NUM_LCD_PCLK  → SCLK
 * QSPI_PIN_NUM_LCD_DATA0 → MOSI
 * QSPI_PIN_NUM_LCD_CS    → CS
 * QSPI_PIN_NUM_LCD_BL    → подсветка
 * QSPI_PIN_NUM_LCD_RST   → reset дисплея
 *
 * DATA1/DATA2/DATA3 не используются.
 */

#define QSPI_LCD_HOST           SPI2_HOST
#define QSPI_PIN_NUM_LCD_PCLK   GPIO_NUM_16   // SCLK
#define QSPI_PIN_NUM_LCD_CS     GPIO_NUM_17   // CS
#define QSPI_PIN_NUM_LCD_DATA0  GPIO_NUM_14   // MOSI

#define QSPI_PIN_NUM_LCD_DATA1  GPIO_NUM_NC   // не используется
#define QSPI_PIN_NUM_LCD_DATA2  GPIO_NUM_NC   // не используется
#define QSPI_PIN_NUM_LCD_DATA3  GPIO_NUM_NC   // не используется

#define QSPI_PIN_NUM_LCD_RST    GPIO_NUM_12   // RST внешнего дисплея
#define QSPI_PIN_NUM_LCD_BL     GPIO_NUM_1    // Подсветка (BL)

/* Смещение (если вдруг нужен отступ) */
#define DISPLAY_OFFSET_X  0
#define DISPLAY_OFFSET_Y  0

/* ------------------------------------------------------------------
 *  ТАЧСКРИН CST816D НА ЭТОЙ ЖЕ I2C-ШИНЕ
 * ------------------------------------------------------------------ */
#define TP_PORT          (I2C_NUM_1)
#define TP_PIN_NUM_SDA   (I2C_SDA_IO)        // GPIO13
#define TP_PIN_NUM_SCL   (I2C_SCL_IO)        // GPIO15
#define TP_PIN_NUM_RST   (GPIO_NUM_NC)       // если подключишь RST — поставим сюда реальный GPIO
#define TP_PIN_NUM_INT   (GPIO_NUM_4)        // INT тача (подключи TP_INT сюда)

/* Подсветка дисплея */
#define DISPLAY_BACKLIGHT_PIN           QSPI_PIN_NUM_LCD_BL
#define DISPLAY_BACKLIGHT_OUTPUT_INVERT false

/* Макрос для QSPI-конфига (оставляем на случай использования в коде.
 * DATA1/DATA2/DATA3 будут просто не задействованы в однолинейном SPI)
 */
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
