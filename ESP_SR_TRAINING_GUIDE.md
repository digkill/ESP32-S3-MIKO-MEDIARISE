# Полное руководство: Обучение модели "Hey Miko" с ESP-SR V2.0

## 📚 Обзор ESP-SR V2.0

ESP-SR (Espressif Speech Recognition) - это фреймворк для создания AI решений распознавания речи.

### Основные компоненты:
- **WakeNet** - обнаружение wake word (например, "Hey Miko")
- **MultiNet** - распознавание команд (до 300 команд)
- **AFE (Audio Front-End)** - обработка аудио (AEC, VAD, BSS, NS)
- **VADNet** - обнаружение активности голоса

### Два способа кастомизации wake word:

1. **Espressif Speech Wake Words Customization Process** - официальный процесс
2. **Training Wake Words by TTS sample** - обучение через TTS (issue #88)

---

## 🎯 Способ 1: Обучение через TTS Pipeline V2.0 (РЕКОМЕНДУЕТСЯ для начала)

Это самый простой способ для создания кастомной модели wake word. **Не требует реальных аудиозаписей!**

### Шаг 1: Установка зависимостей для обучения

**ВАЖНО:** ESP-SR - это компонент для ESP-IDF, а не инструмент обучения!

Для обучения через TTS установите базовые зависимости:

```bash
# Установите базовые зависимости для обучения
pip install tensorflow>=2.8.0
pip install numpy>=1.20.0
pip install scipy>=1.7.0
pip install librosa>=0.9.0
pip install soundfile>=0.10.0

# Для TTS Pipeline (если нужно)
pip install torch>=1.10.0
pip install torchaudio>=0.10.0
```

**Примечание:** В ESP-SR НЕТ requirements.txt в корне - это нормально!

### Шаг 2: Изучение процесса обучения через TTS

**⭐ ГЛАВНЫЙ РЕСУРС:** GitHub Issue #88
- **Ссылка**: https://github.com/espressif/esp-sr/issues/88
- Это **официальный способ** обучения через TTS Pipeline V2.0
- Не требует записи реальных голосов
- Использует синтетические TTS данные
- **Там есть все инструкции и скрипты!**

### Шаг 3: Процесс TTS обучения (из Issue #88)

**Следуйте ТОЧНЫМ инструкциям из Issue #88!**

В issue обычно есть:
1. **Готовые скрипты** для генерации TTS данных
2. **Инструкции** по настройке TTS Pipeline V2.0
3. **Команды** для обучения модели
4. **Скрипты** для конвертации в формат .bin

**Процесс обычно включает:**
- Генерацию TTS аудио для "Hey Miko" (множество вариантов)
- Обучение WakeNet модели на TTS данных
- Конвертацию в формат .bin для ESP32

**ВАЖНО:** Все детали находятся в Issue #88 - откройте его и следуйте инструкциям!

3. **Преимущества TTS обучения:**
   - ✅ Не нужно записывать 100-200 реальных записей
   - ✅ Быстро получить результат
   - ✅ Хорошая точность для большинства случаев
   - ✅ Можно генерировать тысячи вариантов

4. **Недостатки:**
   - ⚠️ Может быть менее точной для реальных голосов
   - ⚠️ Для продакшена лучше использовать реальные записи

---

## 🎯 Способ 2: Официальный процесс кастомизации (для продакшена)

Этот способ требует реальных аудиозаписей, но дает **лучшую точность** для продакшена.

### Шаг 1: Изучение официальной документации

**ВАЖНО:** Изучите официальную документацию:
- **ESP32-S3**: https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/wake_word_engine/ESP_Wake_Words_Customization.html
- **ESP32-P4**: https://docs.espressif.com/projects/esp-sr/en/latest/esp32p4/wake_word_engine/ESP_Wake_Words_Customization.html

**Требования к данным (из документации):**
- Минимум **20,000 записей** от **500+ человек**
- Включая мужчин, женщин и детей разных возрастов
- Разные условия записи
- Формат: WAV, 16kHz, моно, 16-bit PCM

**Примечание:** Для такого объема данных Espressif предлагает услуги по обучению модели. Свяжитесь с ними для обсуждения.

### Шаг 2: Подготовка данных

1. **Структура директорий:**
   ```bash
   cd ~/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32
   mkdir -p training_data/hey_miko/{wake_word,background,negative}
   ```

2. **Требования к аудио:**
   - **Формат**: WAV, 16kHz, моно, 16-bit PCM
   - **Wake word**: Минимум 100-200 записей "Hey Miko"
     - Разные дикторы (мужские, женские голоса)
     - Разные условия (тихо, громко, с шумом)
     - Разные расстояния от микрофона
   - **Background**: 10-20 записей фонового шума
   - **Negative**: 50-100 записей других слов/фраз

3. **Конвертация аудио (если нужно):**
   ```bash
   python scripts/wake_word_training/convert_audio.py \
     training_data/hey_miko/wake_word/ \
     training_data/hey_miko/wake_word_converted/
   ```

### Шаг 3: Обучение модели

```bash
cd ~/Projects/ESP32/esp-sr

# Предобработка данных
python tools/wakenet_training/prepare_data.py \
  --wake_word_dir ../mediarise-robot-console/xiaozhi-esp32/training_data/hey_miko/wake_word \
  --background_dir ../mediarise-robot-console/xiaozhi-esp32/training_data/hey_miko/background \
  --negative_dir ../mediarise-robot-console/xiaozhi-esp32/training_data/hey_miko/negative \
  --output_dir ../mediarise-robot-console/xiaozhi-esp32/training_data/hey_miko/processed_data

# Обучение модели
python tools/wakenet_training/train_model.py \
  --data_dir ../mediarise-robot-console/xiaozhi-esp32/training_data/hey_miko/processed_data \
  --model_name hey_miko \
  --epochs 100 \
  --batch_size 32

# Конвертация в формат для ESP32
python tools/wakenet_training/convert_model.py \
  --model_path trained_model/hey_miko.h5 \
  --output_path ../mediarise-robot-console/xiaozhi-esp32/models/custom_wake_word/hey_miko/hey_miko.bin
```

---

## 🔧 Интеграция обученной модели в проект

### Шаг 1: Размещение модели

```bash
cd ~/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32

# Создайте директорию (если не создана)
mkdir -p models/custom_wake_word/hey_miko

# Скопируйте модель
cp путь/к/обученной/модели.bin models/custom_wake_word/hey_miko/hey_miko.bin

# Создайте конфигурацию
cat > models/custom_wake_word/hey_miko/model_config.json << 'EOF'
{
  "model_name": "hey_miko",
  "wake_word": "hey miko",
  "sample_rate": 16000,
  "chunk_size": 512,
  "description": "Custom wake word model for Hey Miko trained with ESP-SR V2.0"
}
EOF
```

### Шаг 2: Настройка проекта

```bash
# Запустите menuconfig
idf.py menuconfig
```

**Настройки:**
1. Перейдите: **Xiaozhi Assistant → Wake Word Implementation Type**
2. Выберите: **Multinet model (Custom Wake Word)** (для MultiNet) 
   ИЛИ **Wakenet model with AFE** (для WakeNet)
3. В разделе **Custom Wake Word**:
   - **Custom Wake Word**: `hey miko`
   - **Custom Wake Word Display**: `Hey Miko`
   - **Custom Wake Word Threshold**: `20` (начните с 20, настройте по результатам)

### Шаг 3: Сборка и прошивка

```bash
# Очистите предыдущую сборку
idf.py fullclean

# Соберите проект
idf.py build

# Прошейте устройство
idf.py flash monitor
```

### Шаг 4: Проверка

В логах ищите:
```
[WAKE_WORD] Loaded model: hey_miko
[WAKE_WORD] Registered wake word: 'hey miko'
[WAKE_WORD] Wake word detection is now ACTIVE
```

---

## 📋 Выбор типа модели

### WakeNet vs MultiNet

**WakeNet** (рекомендуется для wake word):
- Специально для обнаружения wake word
- Низкое потребление ресурсов
- Высокая точность для одной фразы
- Используйте: **Wakenet model with AFE** в menuconfig

**MultiNet** (для команд):
- Для распознавания множества команд (до 300)
- Можно добавить команды без переобучения
- Больше потребление ресурсов
- Используйте: **Multinet model (Custom Wake Word)** в menuconfig

**Для "Hey Miko" рекомендуется WakeNet!**

---

## 🎓 Детальная инструкция по TTS обучению (Issue #88)

### Процесс обучения через TTS:

1. **Генерация TTS данных:**
   - Используйте TTS Pipeline V2.0 для генерации аудио
   - Генерируйте множество вариантов произношения "Hey Miko"
   - Разные голоса, скорости, интонации

2. **Обучение модели:**
   - Используйте сгенерированные TTS данные
   - Процесс аналогичен обучению на реальных записях
   - Но не требует записи реальных голосов

3. **Преимущества:**
   - Не нужно записывать 100-200 реальных записей
   - Быстрее получить результат
   - Хорошая точность для большинства случаев

4. **Недостатки:**
   - Может быть менее точной для реальных голосов
   - Для продакшена лучше использовать реальные записи

---

## 🔍 Полезные ссылки

### Документация:
- **ESP-SR Documentation**: https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/index.html
- **Migration Guide V1.* → V2.***: https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/audio_front_end/migration_guide.html
- **Wake Word Customization**: https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/wake_word_engine/ESP_Wake_Words_Customization.html
- **TTS Training (Issue #88)**: https://github.com/espressif/esp-sr/issues/88

### Примеры:
- **ESP-Skainet Examples**: https://github.com/espressif/esp-skainet/tree/master/examples/wake_word_detection

### Репозитории:
- **ESP-SR**: https://github.com/espressif/esp-sr
- **ESP-Skainet**: https://github.com/espressif/esp-skainet

---

## ⚙️ Настройка порога чувствительности

После обучения и интеграции модели:

1. Откройте `idf.py menuconfig`
2. Измените **Custom Wake Word Threshold**:
   - **0-15**: Очень чувствительно (много ложных срабатываний)
   - **20-25**: Нормально (рекомендуется начать с 20)
   - **30-50**: Менее чувствительно (меньше ложных срабатываний)
   - **50+**: Очень строго (может пропускать реальные срабатывания)

3. Тестируйте и настраивайте по результатам

---

## ✅ Чеклист для обучения модели "Hey Miko"

### Подготовка:
- [ ] Установлен ESP-SR V2.0
- [ ] Изучена документация по кастомизации
- [ ] Выбран способ обучения (TTS или реальные записи)

### Для TTS обучения:
- [ ] Изучен issue #88
- [ ] Настроен TTS Pipeline V2.0
- [ ] Сгенерированы TTS данные для "Hey Miko"
- [ ] Модель обучена и конвертирована

### Для обучения на реальных записях:
- [ ] Записано 100-200+ примеров "Hey Miko"
- [ ] Подготовлены записи фонового шума
- [ ] Подготовлены negative samples
- [ ] Аудио конвертировано в нужный формат
- [ ] Модель обучена и конвертирована

### Интеграция:
- [ ] Модель размещена в `models/custom_wake_word/hey_miko/`
- [ ] Создан `model_config.json`
- [ ] Настроен menuconfig (Wake Word Type)
- [ ] Проект пересобран и прошит
- [ ] Проверено в логах, что модель загружена
- [ ] Протестировано распознавание "Hey Miko"
- [ ] Настроен порог чувствительности

---

## 🚀 Быстрый старт (TL;DR)

```bash
# 1. Установка ESP-SR
cd ~/Projects/ESP32
git clone https://github.com/espressif/esp-sr.git
cd esp-sr
pip install -r requirements.txt

# 2. Изучите issue #88 для TTS обучения
# Или подготовьте реальные записи

# 3. Обучите модель (следуйте инструкциям из issue #88 или документации)

# 4. Разместите модель
cd ~/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32
mkdir -p models/custom_wake_word/hey_miko
cp путь/к/модели.bin models/custom_wake_word/hey_miko/hey_miko.bin

# 5. Настройте menuconfig
idf.py menuconfig
# Wake Word Type -> Wakenet model with AFE
# Custom Wake Word -> hey miko

# 6. Соберите и прошейте
idf.py build flash monitor
```

---

## 💡 Советы

1. **Начните с TTS обучения** - это быстрее и проще
2. **Для продакшена** используйте реальные записи от разных дикторов
3. **Тестируйте** на разных условиях (шум, расстояние, громкость)
4. **Настраивайте порог** постепенно, начиная с 20
5. **Используйте WakeNet** для wake word (не MultiNet)

---

## 📞 Поддержка

Если возникнут проблемы:
- Проверьте документацию ESP-SR
- Изучите примеры в esp-skainet
- Откройте issue в репозитории ESP-SR: https://github.com/espressif/esp-sr/issues

