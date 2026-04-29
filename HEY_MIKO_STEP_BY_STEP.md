# Пошаговая инструкция: Создание кастомной модели "Hey Miko"

## 🎯 Цель
Создать кастомную модель wake word для точного распознавания "Hey Miko"

---

## 📋 Вариант A: ESP-SR Model Maker (САМЫЙ ПРОСТОЙ способ)

### Шаг 1: Установка ESP-SR Model Maker

```bash
# Перейдите в удобную директорию (например, рядом с проектом)
cd ~/Projects/ESP32

# Клонируйте репозиторий
git clone https://github.com/espressif/esp-sr-model-maker.git
cd esp-sr-model-maker

# Установите зависимости
pip install -r requirements.txt
```

### Шаг 2: Подготовка аудиозаписей

1. **Запишите "Hey Miko" минимум 100-200 раз:**
   - Разные дикторы (мужские, женские голоса)
   - Разные условия (тихо, громко, с шумом)
   - Разные расстояния от микрофона
   - Формат: WAV, MP3, M4A (любой, конвертируется автоматически)

2. **Создайте структуру директорий:**
   ```bash
   cd ~/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32
   mkdir -p training_data/hey_miko/wake_word
   mkdir -p training_data/hey_miko/background
   mkdir -p training_data/hey_miko/negative
   ```

3. **Поместите записи:**
   - Все записи "Hey Miko" → `training_data/hey_miko/wake_word/`
   - Фоновый шум (10-20 записей) → `training_data/hey_miko/background/`
   - Другие слова/фразы (50-100 записей) → `training_data/hey_miko/negative/`

### Шаг 3: Конвертация аудио (если нужно)

Если ваши записи не в формате WAV 16kHz моно:

```bash
cd ~/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32
python scripts/wake_word_training/convert_audio.py \
  training_data/hey_miko/wake_word/ \
  training_data/hey_miko/wake_word_converted/
```

### Шаг 4: Обучение модели через Model Maker

```bash
cd ~/Projects/ESP32/esp-sr-model-maker

# Запустите веб-интерфейс (если доступен)
python app.py

# ИЛИ используйте CLI:
python train_wake_word.py \
  --wake_word_dir ~/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/training_data/hey_miko/wake_word \
  --background_dir ~/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/training_data/hey_miko/background \
  --negative_dir ~/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/training_data/hey_miko/negative \
  --output_dir ~/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32/models/custom_wake_word/hey_miko \
  --wake_word "hey miko"
```

### Шаг 5: Размещение модели в проекте

После обучения модель будет в формате `.bin`. Убедитесь, что она находится здесь:

```bash
cd ~/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32

# Создайте директорию (если не создана)
mkdir -p models/custom_wake_word/hey_miko

# Скопируйте модель (если еще не там)
# cp путь/к/модели.bin models/custom_wake_word/hey_miko/hey_miko.bin

# Создайте конфигурацию
cat > models/custom_wake_word/hey_miko/model_config.json << 'EOF'
{
  "model_name": "hey_miko",
  "wake_word": "hey miko",
  "sample_rate": 16000,
  "chunk_size": 512,
  "description": "Custom wake word model for Hey Miko"
}
EOF
```

### Шаг 6: Настройка проекта

```bash
cd ~/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32

# Запустите menuconfig
idf.py menuconfig
```

В menuconfig:

1. Перейдите: **Xiaozhi Assistant → Wake Word Implementation Type**
2. Выберите: **Multinet model (Custom Wake Word)**
3. В разделе **Custom Wake Word**:
   - **Custom Wake Word**: `hey miko`
   - **Custom Wake Word Display**: `Hey Miko`
   - **Custom Wake Word Threshold**: `20` (начните с 20, можно настроить 0-100)
4. Сохраните (S) и выйдите (Q)

### Шаг 7: Сборка и прошивка

```bash
# Очистите предыдущую сборку (рекомендуется)
idf.py fullclean

# Соберите проект
idf.py build

# Прошейте устройство
idf.py flash monitor
```

### Шаг 8: Проверка

В логах ищите:
```
[WAKE_WORD] Loaded model: hey_miko
[WAKE_WORD] Registered wake word: 'hey miko'
[WAKE_WORD] Wake word detection is now ACTIVE
```

Попробуйте сказать "Hey Miko" - должно сработать!

---

## 📋 Вариант B: Использование ESP-SR SDK напрямую (для продвинутых)

### Шаг 1: Установка ESP-SR SDK

```bash
cd ~/Projects/ESP32
git clone https://github.com/espressif/esp-sr.git
cd esp-sr
pip install -r requirements.txt
```

### Шаг 2: Подготовка данных

```bash
cd ~/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32
mkdir -p training_data/hey_miko/{wake_word,background,negative}
# Поместите аудио файлы в соответствующие директории
```

### Шаг 3: Предобработка данных

```bash
cd ~/Projects/ESP32/esp-sr
python tools/wakenet_training/prepare_data.py \
  --wake_word_dir ../mediarise-robot-console/xiaozhi-esp32/training_data/hey_miko/wake_word \
  --background_dir ../mediarise-robot-console/xiaozhi-esp32/training_data/hey_miko/background \
  --negative_dir ../mediarise-robot-console/xiaozhi-esp32/training_data/hey_miko/negative \
  --output_dir ../mediarise-robot-console/xiaozhi-esp32/training_data/hey_miko/processed_data
```

### Шаг 4: Обучение модели

```bash
python tools/wakenet_training/train_model.py \
  --data_dir ../mediarise-robot-console/xiaozhi-esp32/training_data/hey_miko/processed_data \
  --model_name hey_miko \
  --epochs 100 \
  --batch_size 32
```

### Шаг 5: Конвертация модели

```bash
python tools/wakenet_training/convert_model.py \
  --model_path trained_model/hey_miko.h5 \
  --output_path ../mediarise-robot-console/xiaozhi-esp32/models/custom_wake_word/hey_miko/hey_miko.bin
```

### Шаг 6-8: Повторите шаги 5-8 из Варианта A

---

## 🔧 Настройка порога чувствительности

Если модель слишком чувствительна или не чувствительна:

1. Откройте `idf.py menuconfig`
2. Измените **Custom Wake Word Threshold**:
   - **Меньше значение (0-15)**: Более чувствительно, больше ложных срабатываний
   - **Больше значение (25-50)**: Менее чувствительно, меньше ложных срабатываний
   - **Рекомендуется**: Начните с 20, затем настройте по результатам

---

## ❓ Частые проблемы

### Модель не загружается
- Проверьте, что файл `.bin` находится в `models/custom_wake_word/hey_miko/`
- Убедитесь, что в menuconfig выбран **Multinet model (Custom Wake Word)**
- Проверьте логи при сборке - должна быть строка о найденной модели

### Низкая точность распознавания
- Увеличьте количество обучающих данных (200+ записей)
- Добавьте больше разнообразия (разные дикторы, условия)
- Попробуйте изменить порог в menuconfig

### Модель слишком большая
- Используйте квантование модели при обучении
- Проверьте размер файла `.bin` (должен быть < 500KB)

---

## 📚 Полезные ссылки

- ESP-SR SDK: https://github.com/espressif/esp-sr
- ESP-SR Model Maker: https://github.com/espressif/esp-sr-model-maker
- Документация ESP-SR: https://docs.espressif.com/projects/esp-sr/

---

## ✅ Чеклист

- [ ] Установлен ESP-SR Model Maker или ESP-SR SDK
- [ ] Записано 100-200+ примеров "Hey Miko"
- [ ] Подготовлены записи фонового шума
- [ ] Подготовлены записи других слов (negative samples)
- [ ] Модель обучена и конвертирована в `.bin`
- [ ] Модель размещена в `models/custom_wake_word/hey_miko/`
- [ ] Настроен menuconfig (Multinet model, wake word: "hey miko")
- [ ] Проект пересобран и прошит
- [ ] Проверено в логах, что модель загружена
- [ ] Протестировано распознавание "Hey Miko"



