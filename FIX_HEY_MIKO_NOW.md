# Быстрое исправление: "Hey Miko" не работает

## 🔍 Проблема

В `sdkconfig` настроено:
- ✅ `CONFIG_USE_CUSTOM_WAKE_WORD=y` (Multinet)
- ✅ `CONFIG_CUSTOM_WAKE_WORD="Hey Miko"`
- ❌ **Модель отсутствует** в `models/custom_wake_word/hey_miko/`

## ⚡ Быстрое решение (2 варианта)

### Вариант 1: Переключиться на AFE Wake Word (РЕКОМЕНДУЕТСЯ сейчас)

Используйте готовую английскую модель WakeNet:

```bash
# Откройте menuconfig
idf.py menuconfig
```

**Настройки:**
1. **Xiaozhi Assistant → Wake Word Implementation Type**
   - Выберите: **Wakenet model with AFE** (вместо Multinet)
2. **ESP Speech Recognition → WakeNet Model Configuration**
   - Выберите: `CONFIG_SR_WN_WN9_HIESP=y` (модель "Hi ESP")

**Или отредактируйте sdkconfig напрямую:**

```bash
# Отредактируйте sdkconfig
```

Замените строки 730-734:
```ini
# Было:
# CONFIG_USE_AFE_WAKE_WORD is not set
CONFIG_USE_CUSTOM_WAKE_WORD=y
CONFIG_CUSTOM_WAKE_WORD="Hey Miko"
CONFIG_CUSTOM_WAKE_WORD_DISPLAY="Hey Miko"
CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=20

# Станет:
CONFIG_USE_AFE_WAKE_WORD=y
# CONFIG_USE_CUSTOM_WAKE_WORD is not set
```

И убедитесь, что в строке 786:
```ini
CONFIG_SR_WN_WN9_HIESP=y
# CONFIG_SR_WN_WN9_NIHAOXIAOZHI_TTS is not set
```

**Затем:**
```bash
idf.py build flash monitor
# Попробуйте сказать "Hi ESP"
```

### Вариант 2: Создать структуру для будущей модели

Если вы планируете обучить модель:

```bash
# Создайте структуру
mkdir -p models/custom_wake_word/hey_miko

# Создайте конфигурацию (модель добавите позже)
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

**НО:** Пока модель не будет обучена и помещена в `models/custom_wake_word/hey_miko/hey_miko.bin`, wake word не будет работать!

## 📋 Что делать дальше

### Для работы СЕЙЧАС:
1. Используйте **Вариант 1** (AFE Wake Word с "Hi ESP")
2. Протестируйте работу wake word

### Для работы с "Hey Miko":
1. Обучите модель (см. `ESP_SR_TRAINING_GUIDE.md` или Issue #88)
2. Поместите модель: `models/custom_wake_word/hey_miko/hey_miko.bin`
3. Переключитесь обратно на Multinet в menuconfig
4. Пересоберите проект

## ✅ Проверка после исправления

После переключения на AFE Wake Word в логах должно быть:
```
[WAKE_WORD] Loaded model: wn9_hiesp
[WAKE_WORD] Registered wake word: 'Hi ESP'
[WAKE_WORD] Wake word detection is now ACTIVE
```

Попробуйте сказать "Hi ESP" - должно работать!



