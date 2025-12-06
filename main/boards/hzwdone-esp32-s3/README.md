# HZWDONE ESP32-S3

## Описание

Плата разработки HZWDONE ESP32-S3 с экраном 2 дюйма (320x240).

## Характеристики

- **Микроконтроллер**: ESP32-S3
- **Дисплей**: 2 дюйма, 320x240 пикселей, ST7789
- **Аудио кодек**: ES8311
- **Тачскрин**: Опционально (CST816D)
- **Зарядка батареи**: Поддерживается

## Конфигурация

### Пины дисплея (SPI)
- SCLK: GPIO4
- MOSI: GPIO2
- CS: GPIO5
- DC: GPIO47
- RESET: GPIO38
- Backlight: GPIO42

### Пины аудио (I2S)
- MCLK: GPIO16
- BCLK: GPIO9
- WS (LRCK): GPIO45
- DOUT: GPIO8
- DIN: GPIO10

### Пины аудио кодек (I2C)
- SDA: GPIO15
- SCL: GPIO14
- PA: GPIO46

### Пины тачскрина (I2C, опционально)
- SDA: GPIO11
- SCL: GPIO7
- RST: GPIO6
- INT: GPIO12

### Другие пины
- Boot Button: GPIO0
- LED: GPIO48
- Battery ADC: GPIO1
- Battery Charging: GPIO41

## Использование

1. Откройте menuconfig:
   ```bash
   idf.py menuconfig
   ```

2. Выберите плату:
   ```
   Xiaozhi Assistant → Board Type → HZWDONE ESP32-S3 (2 inch display)
   ```

3. Соберите проект:
   ```bash
   idf.py build
   ```

4. Прошейте устройство:
   ```bash
   idf.py flash monitor
   ```

## Примечания

- Разрешение дисплея: 320x240 пикселей
- Драйвер дисплея: ST7789
- Аудио кодек: ES8311
- Поддержка тачскрина опциональна (CST816D)




