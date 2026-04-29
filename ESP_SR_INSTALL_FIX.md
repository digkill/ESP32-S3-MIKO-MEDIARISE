# Решение проблемы с установкой ESP-SR

## ❌ Проблема: `requirements.txt` не найден

Если вы получили ошибку:
```
ERROR: Could not open requirements file: [Errno 2] No such file or directory: 'requirements.txt'
```

Это означает, что файл `requirements.txt` отсутствует в репозитории ESP-SR или находится в другом месте.

---

## ✅ Решение 1: Проверка структуры ESP-SR

ESP-SR **НЕ имеет** `requirements.txt` в корне! Это нормально.

Проверьте структуру:

```bash
cd ~/Projects/ESP32/esp-sr
ls -la

# Ищите requirements.txt в поддиректориях:
find . -name "requirements.txt" -type f
```

### Найденные файлы:
- `test_apps/requirements.txt` - для тестов (pytest и т.д.)
- `docs/requirements.txt` - для документации
- `tool/requirements` - для инструментов MultiNet (без расширения .txt!)

**ВАЖНО:** ESP-SR - это компонент для ESP-IDF, а не отдельный Python пакет для обучения!

---

## ✅ Решение 2: Установка зависимостей вручную

ESP-SR не требует установки зависимостей для использования в проекте. Но для **обучения моделей** нужны зависимости:

```bash
# Основные зависимости для обучения wake word
pip install tensorflow>=2.8.0
pip install numpy>=1.20.0
pip install scipy>=1.7.0
pip install librosa>=0.9.0
pip install soundfile>=0.10.0
pip install matplotlib>=3.5.0
pip install scikit-learn>=1.0.0

# Для TTS Pipeline (если используете)
pip install torch>=1.10.0
pip install torchaudio>=0.10.0

# Для инструментов MultiNet (если используете)
cd ~/Projects/ESP32/esp-sr/tool
cat requirements  # Посмотрите, что там указано
pip install -r requirements  # Установите (без .txt)
```

---

## ✅ Решение 3: Использование ESP-SR Model Maker (РЕКОМЕНДУЕТСЯ)

Вместо установки всего ESP-SR SDK, используйте **ESP-SR Model Maker** - это отдельный инструмент:

```bash
cd ~/Projects/ESP32

# Клонируйте Model Maker (отдельный репозиторий)
git clone https://github.com/espressif/esp-sr-model-maker.git
cd esp-sr-model-maker

# Проверьте наличие requirements.txt
ls -la requirements.txt

# Если есть - установите
pip install -r requirements.txt

# Если нет - установите зависимости вручную (см. Решение 2)
```

---

## ✅ Решение 4: Обучение через TTS (Issue #88) - БЕЗ установки SDK

**ВАЖНО:** Для обучения через TTS Pipeline V2.0 (Issue #88) может **НЕ требоваться** полная установка ESP-SR SDK!

### Шаги:

1. **Изучите Issue #88:**
   ```bash
   # Откройте в браузере:
   # https://github.com/espressif/esp-sr/issues/88
   ```

2. **Следуйте инструкциям из Issue:**
   - В issue обычно есть готовые скрипты или ссылки на инструменты
   - Может быть веб-интерфейс или готовые Docker-образы
   - Может быть отдельный репозиторий для TTS обучения

3. **Проверьте документацию ESP-SR:**
   - https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/index.html
   - Ищите раздел про TTS training

---

## ✅ Решение 5: Использование готовых инструментов

### Вариант A: ESP-SR Model Maker (веб-интерфейс)

Если доступен веб-интерфейс:
```bash
cd ~/Projects/ESP32/esp-sr-model-maker
python app.py  # или другой способ запуска
# Откройте браузер и используйте веб-интерфейс
```

### Вариант B: Docker (если доступен)

```bash
# Если есть Dockerfile
docker build -t esp-sr-training .
docker run -it esp-sr-training
```

---

## 🔍 Проверка установки ESP-SR

После установки зависимостей проверьте:

```bash
# Проверьте структуру ESP-SR
cd ~/Projects/ESP32/esp-sr
ls -la

# Ищите инструменты для обучения
find . -name "*train*" -type f
find . -name "*wakenet*" -type d
find . -name "*tts*" -type d

# Проверьте документацию
ls -la docs/
ls -la README.md
```

---

## 📋 Минимальные зависимости для обучения wake word

Если вы хотите обучить модель самостоятельно, установите минимум:

```bash
pip install tensorflow numpy scipy librosa soundfile
```

Этого должно хватить для базового обучения модели.

---

## 🎯 Рекомендуемый подход

**Для "Hey Miko" рекомендую:**

1. **Сначала изучите Issue #88:**
   - https://github.com/espressif/esp-sr/issues/88
   - Там может быть готовое решение или инструкции

2. **Попробуйте ESP-SR Model Maker:**
   ```bash
   git clone https://github.com/espressif/esp-sr-model-maker.git
   cd esp-sr-model-maker
   # Следуйте инструкциям в README
   ```

3. **Если ничего не работает:**
   - Используйте услуги Espressif для обучения модели
   - Или используйте временно английскую модель из SDK (уже настроена)

---

## 🔧 Альтернатива: Использование готовой модели

Пока вы разбираетесь с обучением, можете использовать английскую модель:

```bash
cd ~/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32

# Уже настроено в sdkconfig.defaults.esp32s3:
# CONFIG_SR_WN_WN9_HIESP=y (модель "Hi ESP")

# Пересоберите проект
idf.py build flash monitor

# Попробуйте сказать "Hi ESP" - должно работать
```

---

## 📞 Полезные ссылки

- **ESP-SR GitHub**: https://github.com/espressif/esp-sr
- **ESP-SR Model Maker**: https://github.com/espressif/esp-sr-model-maker
- **Issue #88 (TTS Training)**: https://github.com/espressif/esp-sr/issues/88
- **ESP-SR Documentation**: https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/index.html

---

## ✅ Быстрое решение

Если нужно быстро начать работу:

```bash
# 1. Установите базовые зависимости для обучения
pip install tensorflow numpy scipy librosa soundfile

# 2. Изучите Issue #88 - это ГЛАВНЫЙ ресурс!
# https://github.com/espressif/esp-sr/issues/88
# Там есть пошаговые инструкции по TTS обучению

# 3. Или используйте временно английскую модель
# (уже настроена в sdkconfig.defaults.esp32s3)
cd ~/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32
idf.py build flash monitor
# Попробуйте сказать "Hi ESP"
```

## ⚠️ ВАЖНО: ESP-SR - это компонент, а не инструмент обучения!

**ESP-SR** - это компонент для ESP-IDF, который предоставляет готовые модели wake word. 

**Для обучения СВОЕЙ модели** используйте:
1. **Issue #88** - TTS обучение (самый простой способ)
2. **ESP-SR Model Maker** - отдельный инструмент
3. **Услуги Espressif** - для продакшена (требует 20,000+ записей)

