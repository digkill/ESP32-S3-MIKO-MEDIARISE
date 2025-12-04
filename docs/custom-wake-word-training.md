# Создание собственной модели wake word с нуля

## Обзор

Это руководство описывает процесс создания полностью кастомной модели wake word с нуля для использования в проекте xiaozhi-esp32.

## Требования

- ESP-SR SDK (отдельно от ESP-IDF)
- Python 3.x с библиотеками для машинного обучения (TensorFlow, NumPy)
- Аудиозаписи с вашим wake word (минимум 100-200 записей)
- Записи фонового шума и других слов для обучения

## Шаг 1: Установка ESP-SR SDK

1. Клонировать ESP-SR репозиторий:
   ```bash
   git clone https://github.com/espressif/esp-sr.git
   cd esp-sr
   ```

2. Установить зависимости Python:
   ```bash
   pip install -r requirements.txt
   ```
   
   **Примечание:** Файл `requirements.txt` находится в корне репозитория ESP-SR после клонирования.

3. (Опционально) Установить зависимости для скриптов проекта:
   ```bash
   cd /path/to/xiaozhi-esp32
   pip install -r scripts/wake_word_training/requirements.txt
   ```

## Шаг 2: Подготовка данных для обучения

### Структура директорий

Создайте следующую структуру в `training_data/`:

```
training_data/
├── wake_word/          # Записи вашего wake word
│   ├── sample1.wav
│   ├── sample2.wav
│   └── ...
├── background/         # Фоновый шум
│   ├── noise1.wav
│   └── ...
└── negative/          # Другие слова (не wake word)
    ├── other1.wav
    └── ...
```

### Требования к аудио

- **Формат**: WAV, 16kHz, моно, 16-bit PCM
- **Количество**: Минимум 100-200 записей wake word от разных дикторов
- **Разнообразие**: Разные условия (тихо, громко, с шумом, разные расстояния)
- **Длительность**: 1-2 секунды на запись
- **Качество**: Чистые записи без искажений

### Конвертация аудио в нужный формат

Используйте скрипт `scripts/wake_word_training/convert_audio.py` для конвертации аудио в нужный формат.

## Шаг 3: Обучение модели

### 3.1 Предобработка данных

```bash
cd esp-sr
python tools/wakenet_training/prepare_data.py \
  --wake_word_dir ../xiaozhi-esp32/training_data/wake_word \
  --background_dir ../xiaozhi-esp32/training_data/background \
  --negative_dir ../xiaozhi-esp32/training_data/negative \
  --output_dir ../xiaozhi-esp32/training_data/processed_data
```

### 3.2 Обучение модели

```bash
python tools/wakenet_training/train_model.py \
  --data_dir ../xiaozhi-esp32/training_data/processed_data \
  --model_name my_custom_wake_word \
  --epochs 100 \
  --batch_size 32
```

### 3.3 Конвертация в формат для ESP32

```bash
python tools/wakenet_training/convert_model.py \
  --model_path trained_model/my_custom_wake_word.h5 \
  --output_path ../xiaozhi-esp32/models/custom_wake_word/my_custom_wake_word.bin
```

## Шаг 4: Интеграция модели в проект

### 4.1 Размещение файлов модели

Скопируйте файлы модели в директорию проекта:

```bash
cp my_custom_wake_word.bin models/custom_wake_word/
```

### 4.2 Создание конфигурации модели

Создайте файл `models/custom_wake_word/model_config.json`:

```json
{
  "model_name": "my_custom_wake_word",
  "wake_word": "ваше_слово",
  "sample_rate": 16000,
  "chunk_size": 512
}
```

### 4.3 Настройка через menuconfig

1. Запустите `idf.py menuconfig`
2. Перейдите в **Xiaozhi Assistant → Wake Word Implementation Type**
3. Выберите:
   - `Wakenet model with AFE` (для ESP32-S3 с PSRAM) - **рекомендуется для кастомных моделей**
   - `Wakenet model without AFE` (для ESP32-C3/C5/C6 или ESP32 с PSRAM)

**Важно:** Кастомные модели автоматически обнаруживаются при сборке, если они находятся в `models/custom_wake_word/`. Они имеют приоритет над стандартными моделями ESP-SR.

### 4.4 Автоматическое обнаружение моделей

Система сборки автоматически:
- Ищет модели в `models/custom_wake_word/`
- Проверяет наличие файлов `.bin` в поддиректориях
- Включает найденные модели в assets при сборке
- Выводит информацию о найденных моделях в консоль

## Шаг 5: Сборка и тестирование

1. Соберите проект:
   ```bash
   idf.py build
   ```

2. Прошейте устройство:
   ```bash
   idf.py flash monitor
   ```

3. Проверьте логи для подтверждения загрузки модели:
   - Ищите сообщения типа `"Wake word(my_custom_wake_word),freq: 16000, chunksize: 512"`

## Альтернативный подход: ESP-SR Model Maker

Espressif предоставляет онлайн-инструмент ESP-SR Model Maker для упрощения процесса:

1. Перейдите на https://github.com/espressif/esp-sr-model-maker
2. Загрузите аудиозаписи на платформу
3. Обучите модель через веб-интерфейс
4. Скачайте готовую модель в формате для ESP32

## Оптимизация модели

Для лучшей производительности на ESP32:

1. **Квантование**: Используйте 8-bit квантование для уменьшения размера
2. **Размер модели**: Стремитесь к размеру < 200KB
3. **Точность**: Балансируйте между точностью и размером модели

## Устранение проблем

### Модель не загружается

- Проверьте, что файл модели находится в правильной директории
- Убедитесь, что модель совместима с версией ESP-SR SDK
- Проверьте логи на наличие ошибок загрузки

### Низкая точность распознавания

- Увеличьте количество обучающих данных
- Добавьте больше разнообразия в данные (разные дикторы, условия)
- Попробуйте увеличить количество эпох обучения
- Проверьте качество аудиозаписей

### Модель слишком большая

- Используйте квантование модели
- Уменьшите размер архитектуры модели
- Используйте pruning для удаления неважных весов

## Дополнительные ресурсы

- ESP-SR SDK: https://github.com/espressif/esp-sr
- ESP-SR Model Maker: https://github.com/espressif/esp-sr-model-maker
- Документация ESP-SR: https://docs.espressif.com/projects/esp-sr/

