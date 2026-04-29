#!/bin/bash

# Скрипт для настройки кастомной модели "Hey Miko"
# Использование: ./setup_hey_miko.sh

set -e

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MODEL_NAME="hey_miko"
MODEL_DIR="$PROJECT_DIR/models/custom_wake_word/$MODEL_NAME"

echo "=========================================="
echo "Настройка кастомной модели 'Hey Miko'"
echo "=========================================="
echo ""

# Шаг 1: Создание структуры директорий
echo "📁 Создание структуры директорий..."
mkdir -p "$MODEL_DIR"
mkdir -p "$PROJECT_DIR/training_data/$MODEL_NAME/wake_word"
mkdir -p "$PROJECT_DIR/training_data/$MODEL_NAME/background"
mkdir -p "$PROJECT_DIR/training_data/$MODEL_NAME/negative"
echo "✅ Директории созданы"
echo ""

# Шаг 2: Создание конфигурации модели
echo "⚙️  Создание конфигурации модели..."
cat > "$MODEL_DIR/model_config.json" << EOF
{
  "model_name": "$MODEL_NAME",
  "wake_word": "hey miko",
  "sample_rate": 16000,
  "chunk_size": 512,
  "description": "Custom wake word model for Hey Miko"
}
EOF
echo "✅ Конфигурация создана: $MODEL_DIR/model_config.json"
echo ""

# Шаг 3: Информация о следующих шагах
echo "=========================================="
echo "📋 Следующие шаги:"
echo "=========================================="
echo ""
echo "1. 📝 Запишите аудио:"
echo "   - Поместите 100-200+ записей 'Hey Miko' в:"
echo "     $PROJECT_DIR/training_data/$MODEL_NAME/wake_word/"
echo "   - Поместите фоновый шум в:"
echo "     $PROJECT_DIR/training_data/$MODEL_NAME/background/"
echo "   - Поместите другие слова в:"
echo "     $PROJECT_DIR/training_data/$MODEL_NAME/negative/"
echo ""
echo "2. 🔄 Конвертируйте аудио (если нужно):"
echo "   python scripts/wake_word_training/convert_audio.py \\"
echo "     training_data/$MODEL_NAME/wake_word/ \\"
echo "     training_data/$MODEL_NAME/wake_word_converted/"
echo ""
echo "3. 🎓 Обучите модель:"
echo "   - Используйте ESP-SR Model Maker:"
echo "     https://github.com/espressif/esp-sr-model-maker"
echo "   - Или ESP-SR SDK:"
echo "     https://github.com/espressif/esp-sr"
echo ""
echo "4. 📦 Поместите обученную модель:"
echo "   cp your_model.bin $MODEL_DIR/$MODEL_NAME.bin"
echo ""
echo "5. ⚙️  Настройте menuconfig:"
echo "   idf.py menuconfig"
echo "   -> Xiaozhi Assistant"
echo "   -> Wake Word Implementation Type: Multinet model (Custom Wake Word)"
echo "   -> Custom Wake Word: hey miko"
echo "   -> Custom Wake Word Display: Hey Miko"
echo ""
echo "6. 🔨 Соберите и прошейте:"
echo "   idf.py build flash monitor"
echo ""
echo "=========================================="
echo "📚 Подробная инструкция:"
echo "   См. файл: HEY_MIKO_STEP_BY_STEP.md"
echo "=========================================="



