# Правильный способ работы с ESP-SR для обучения "Hey Miko"

## ⚠️ ВАЖНОЕ ПОНЯТИЕ

**ESP-SR** - это **компонент для ESP-IDF**, а НЕ инструмент для обучения моделей!

- ESP-SR предоставляет **готовые модели** wake word
- ESP-SR **уже используется** в вашем проекте через managed_components
- Для **обучения СВОЕЙ модели** нужны другие инструменты

---

## ✅ Правильный подход для обучения "Hey Miko"

### Вариант 1: TTS обучение через Issue #88 (РЕКОМЕНДУЕТСЯ)

**Это самый простой способ!**

1. **Откройте Issue #88:**
   ```
   https://github.com/espressif/esp-sr/issues/88
   ```

2. **Следуйте инструкциям в issue:**
   - Там есть пошаговые инструкции по TTS Pipeline V2.0
   - Не требует установки ESP-SR SDK
   - Может быть веб-интерфейс или готовые скрипты

3. **Процесс обычно включает:**
   - Генерацию TTS данных для "Hey Miko"
   - Обучение модели через TTS Pipeline
   - Конвертацию в формат .bin

### Вариант 2: ESP-SR Model Maker

```bash
cd ~/Projects/ESP32

# Клонируйте Model Maker (отдельный инструмент)
git clone https://github.com/espressif/esp-sr-model-maker.git
cd esp-sr-model-maker

# Проверьте README для инструкций
cat README.md

# Установите зависимости (если есть requirements.txt)
if [ -f requirements.txt ]; then
    pip install -r requirements.txt
else
    # Установите базовые зависимости
    pip install tensorflow numpy scipy librosa soundfile
fi
```

### Вариант 3: Услуги Espressif

Для продакшена (требует 20,000+ записей):
- Свяжитесь с Espressif для обучения модели
- См. документацию: https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/wake_word_engine/ESP_Wake_Words_Customization.html

---

## 🔧 Что делать СЕЙЧАС

### Шаг 1: Установите базовые зависимости (если нужно)

```bash
# Для обучения моделей (если будете использовать инструменты)
pip install tensorflow numpy scipy librosa soundfile

# Для MultiNet инструментов (если будете использовать)
cd ~/Projects/ESP32/esp-sr/tool
pip install g2p-en pypinyin pypinyin_dict
```

### Шаг 2: Изучите Issue #88

**Это главный ресурс для обучения через TTS!**

```bash
# Откройте в браузере:
# https://github.com/espressif/esp-sr/issues/88
```

В issue #88 вы найдете:
- Пошаговые инструкции
- Готовые скрипты или инструменты
- Примеры использования TTS Pipeline V2.0

### Шаг 3: Используйте временно английскую модель

Пока разбираетесь с обучением, используйте английскую модель:

```bash
cd ~/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32

# Уже настроено в sdkconfig.defaults.esp32s3:
# CONFIG_SR_WN_WN9_HIESP=y

# Пересоберите и протестируйте
idf.py build flash monitor

# Попробуйте сказать "Hi ESP"
```

---

## 📋 Структура ESP-SR (для понимания)

```
esp-sr/
├── README.md              # Основная документация
├── tool/                  # Инструменты для MultiNet
│   ├── requirements       # Зависимости для MultiNet (без .txt!)
│   └── multinet_*.py      # Скрипты для MultiNet
├── test_apps/            # Тестовые приложения
│   └── requirements.txt   # Только для тестов
├── docs/                  # Документация
│   └── requirements.txt   # Только для документации
└── model/                 # Готовые модели wake word
    └── wakenet_model/     # Модели WakeNet
```

**Важно:** В корне ESP-SR НЕТ requirements.txt - это нормально!

---

## 🎯 Резюме

1. **ESP-SR не требует установки** для использования в проекте (уже подключен)
2. **Для обучения модели** используйте Issue #88 или ESP-SR Model Maker
3. **Issue #88** - главный ресурс для TTS обучения
4. **Временно** используйте английскую модель (уже настроена)

---

## 📚 Полезные ссылки

- **Issue #88 (TTS Training)**: https://github.com/espressif/esp-sr/issues/88 ⭐ **ГЛАВНЫЙ**
- **ESP-SR Model Maker**: https://github.com/espressif/esp-sr-model-maker
- **ESP-SR Documentation**: https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/index.html
- **Wake Word Customization**: https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/wake_word_engine/ESP_Wake_Words_Customization.html

---

## ✅ Быстрое решение проблемы

```bash
# 1. Не нужно устанавливать ESP-SR - он уже подключен как компонент!

# 2. Для обучения установите базовые зависимости:
pip install tensorflow numpy scipy librosa soundfile

# 3. Изучите Issue #88:
# https://github.com/espressif/esp-sr/issues/88

# 4. Или используйте временно английскую модель:
cd ~/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32
idf.py build flash monitor
```



