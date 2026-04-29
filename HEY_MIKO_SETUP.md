# Настройка Wake Word "Hey Miko"

## Вариант 1: Использование английской модели из ESP-SR SDK (быстро)

ESP-SR SDK содержит несколько английских моделей wake word. Хотя точной модели "Hey Miko" нет, можно использовать похожую.

### Шаг 1: Выбор модели через menuconfig

1. Запустите menuconfig:
   ```bash
   idf.py menuconfig
   ```

2. Перейдите в раздел **Component config → ESP Speech Recognition**

3. Найдите секцию **WakeNet Model Configuration**

4. Выберите одну из английских моделей:
   - `CONFIG_SR_WN_WN9_HIESP` - "Hi ESP"
   - `CONFIG_SR_WN_WN9_ALEXA` - "Alexa"
   - `CONFIG_SR_WN_WN9_JARVIS_TTS` - "Jarvis"
   - `CONFIG_SR_WN_WN9_COMPUTER_TTS` - "Computer"
   - `CONFIG_SR_WN_WN9_HEYWILLOW_TTS` - "Hey Willow"
   - `CONFIG_SR_WN_WN9_HIMFIVE` - "Hi Five"
   - Или любую другую английскую модель

5. Отключите китайскую модель:
   - Снимите галочку с `CONFIG_SR_WN_WN9_NIHAOXIAOZHI_TTS`

6. Сохраните и выйдите (S, затем Q)

### Шаг 2: Изменение в sdkconfig.defaults.esp32s3

Или отредактируйте файл `sdkconfig.defaults.esp32s3` напрямую:

```bash
# Отключить китайскую модель
# CONFIG_SR_WN_WN9_NIHAOXIAOZHI_TTS=y

# Включить английскую модель (выберите одну)
CONFIG_SR_WN_WN9_HIESP=y
# или
# CONFIG_SR_WN_WN9_ALEXA=y
# или
# CONFIG_SR_WN_WN9_JARVIS_TTS=y
```

### Шаг 3: Пересборка

```bash
idf.py build flash monitor
```

**Недостаток:** Модель не будет точно распознавать "Hey Miko", только похожие английские фразы.

---

## Вариант 2: Создание кастомной модели "Hey Miko" (рекомендуется)

Для точного распознавания "Hey Miko" нужна кастомная модель.

### Шаг 1: Подготовка данных

1. Создайте структуру директорий:
   ```bash
   mkdir -p training_data/wake_word
   mkdir -p training_data/background
   mkdir -p training_data/negative
   ```

2. Запишите аудио:
   - **wake_word/**: Минимум 100-200 записей "Hey Miko"
     - Разные дикторы (мужские, женские голоса)
     - Разные условия (тихо, громко, с шумом)
     - Разные расстояния от микрофона
     - Формат: WAV, 16kHz, моно, 16-bit PCM
   
   - **background/**: Фоновый шум (10-20 записей)
   
   - **negative/**: Другие слова/фразы (50-100 записей)

### Шаг 2: Использование ESP-SR Model Maker (самый простой способ)

1. Перейдите на: https://github.com/espressif/esp-sr-model-maker
   
2. Или используйте локально:
   ```bash
   git clone https://github.com/espressif/esp-sr-model-maker.git
   cd esp-sr-model-maker
   ```

3. Загрузите ваши аудиозаписи через веб-интерфейс или используйте CLI

4. Обучите модель для "Hey Miko"

5. Скачайте готовую модель в формате `.bin`

### Шаг 3: Размещение модели в проекте

1. Создайте директорию для модели:
   ```bash
   mkdir -p models/custom_wake_word/hey_miko
   ```

2. Скопируйте файл модели:
   ```bash
   cp downloaded_model.bin models/custom_wake_word/hey_miko/hey_miko.bin
   ```

3. Создайте конфигурацию (опционально):
   ```bash
   cat > models/custom_wake_word/hey_miko/model_config.json << EOF
   {
     "model_name": "hey_miko",
     "wake_word": "hey miko",
     "sample_rate": 16000,
     "chunk_size": 512,
     "description": "Custom wake word model for Hey Miko"
   }
   EOF
   ```

### Шаг 4: Настройка через menuconfig

1. Запустите `idf.py menuconfig`

2. Перейдите в **Xiaozhi Assistant → Wake Word Implementation Type**

3. Выберите **Multinet model (Custom Wake Word)**

4. В разделе **Custom Wake Word**:
   - **Custom Wake Word**: `hey miko`
   - **Custom Wake Word Display**: `Hey Miko`
   - **Custom Wake Word Threshold**: `20` (можно настроить, 0-100)

5. Сохраните и выйдите

### Шаг 5: Пересборка

```bash
idf.py build flash monitor
```

### Шаг 6: Проверка в логах

После загрузки ищите в логах:
```
[WAKE_WORD] Loaded model: hey_miko
[WAKE_WORD] Registered wake word: 'hey miko'
[WAKE_WORD] Wake word detection is now ACTIVE
```

---

## Вариант 3: Использование Multinet с готовой моделью (если доступна)

Если у вас уже есть обученная модель Multinet для "Hey Miko":

1. Поместите модель в `models/custom_wake_word/hey_miko/`

2. Настройте как в Варианте 2, шаг 4

3. Пересоберите проект

---

## Рекомендации

- **Для быстрого тестирования**: Используйте Вариант 1 с моделью "Hi ESP" или "Alexa"
- **Для продакшена**: Используйте Вариант 2 с кастомной моделью для точного распознавания
- **Точность**: Кастомная модель даст лучшую точность, но требует обучения

## Дополнительные ресурсы

- ESP-SR SDK: https://github.com/espressif/esp-sr
- ESP-SR Model Maker: https://github.com/espressif/esp-sr-model-maker
- Документация по обучению: `docs/custom-wake-word-training.md`



