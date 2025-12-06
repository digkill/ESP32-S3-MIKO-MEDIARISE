#!/bin/bash
# Скрипт для сборки и прошивки платы esp32-s3-touch-lcd-1.46

set -e

# Переходим в корневую директорию проекта
cd "$(dirname "$0")"

echo "=========================================="
echo "Сборка и прошивка для esp32-s3-touch-lcd-1.46"
echo "=========================================="

# Проверяем, что ESP-IDF настроен
if ! command -v idf.py &> /dev/null; then
    echo "Ошибка: ESP-IDF не настроен в текущей оболочке"
    echo "Пожалуйста, выполните:"
    echo "  . \$HOME/esp/esp-idf/export.sh"
    echo "или настройте ESP-IDF согласно документации"
    exit 1
fi

# Устанавливаем целевой чип (если еще не установлен)
echo "Проверка целевого чипа..."
if ! grep -q "CONFIG_IDF_TARGET=\"esp32s3\"" sdkconfig 2>/dev/null; then
    echo "Установка целевого чипа ESP32-S3..."
    idf.py set-target esp32s3
fi

# Проверяем, что выбрана правильная плата
if ! grep -q "CONFIG_BOARD_TYPE_WAVESHARE_S3_TOUCH_LCD_1_46=y" sdkconfig 2>/dev/null; then
    echo "Ошибка: Плата esp32-s3-touch-lcd-1.46 не выбрана в конфигурации"
    echo "Выполните: idf.py menuconfig"
    echo "И выберите: Xiaozhi Assistant -> Board Type -> Waveshare ESP32-S3-Touch-LCD-1.46"
    exit 1
fi

echo "Конфигурация проверена ✓"

# Сборка проекта
echo ""
echo "Начинаем сборку..."
idf.py build

echo ""
echo "=========================================="
echo "Сборка завершена успешно!"
echo "=========================================="
echo ""
echo "Для прошивки выполните:"
echo "  idf.py flash"
echo ""
echo "Для прошивки и мониторинга:"
echo "  idf.py flash monitor"
echo ""
echo "Для прошивки с указанием порта:"
echo "  idf.py -p /dev/ttyUSB0 flash monitor"
echo ""

