# Быстрый старт: Кастомная модель wake word

## Краткое руководство

### Шаг 1: Подготовка данных

1. Соберите аудиозаписи вашего wake word (минимум 100-200 записей)
2. Конвертируйте в нужный формат:
   ```bash
   python scripts/wake_word_training/convert_audio.py your_audio_dir/ training_data/wake_word/
   ```

### Шаг 2: Обучение модели

1. Установите ESP-SR SDK:
   ```bash
   git clone https://github.com/espressif/esp-sr.git
   cd esp-sr
   pip install -r requirements.txt
   ```

2. Обучите модель (см. `docs/custom-wake-word-training.md` для деталей)

3. Скопируйте обученную модель в поддиректорию:
   ```bash
   mkdir -p ../xiaozhi-esp32/models/custom_wake_word/my_custom_wake_word
   cp my_custom_wake_word.bin ../xiaozhi-esp32/models/custom_wake_word/my_custom_wake_word/
   ```
   
   **Важно:** Модель должна быть в поддиректории с именем модели, и файл должен иметь расширение `.bin`.

### Шаг 3: Интеграция в проект

1. Настройте wake word type в menuconfig:
   ```bash
   idf.py menuconfig
   # Xiaozhi Assistant -> Wake Word Implementation Type -> Wakenet model with AFE
   ```

2. Соберите проект:
   ```bash
   idf.py build
   ```

3. Прошейте устройство:
   ```bash
   idf.py flash monitor
   ```

## Структура директории модели

```
models/custom_wake_word/
└── my_custom_wake_word/
    ├── my_custom_wake_word.bin    # Бинарный файл модели
    └── model_config.json           # Конфигурация (опционально)
```

## Примечания

- Кастомные модели автоматически обнаруживаются при сборке
- Модель должна быть в формате, совместимом с ESP-SR SDK
- Подробные инструкции см. в `docs/custom-wake-word-training.md`

