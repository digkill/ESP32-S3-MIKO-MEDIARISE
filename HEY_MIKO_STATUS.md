# Статус настройки "Hey Miko"

## ✅ Что уже настроено

В `sdkconfig` уже настроено:
```ini
CONFIG_USE_CUSTOM_WAKE_WORD=y
CONFIG_CUSTOM_WAKE_WORD="Hey Miko"
CONFIG_CUSTOM_WAKE_WORD_DISPLAY="Hey Miko"
CONFIG_CUSTOM_WAKE_WORD_THRESHOLD=20
```

## ❌ Проблема

**Модель отсутствует!** В `models/custom_wake_word/` нет директории `hey_miko/` с файлом модели `.bin`.

## 🔧 Решение

### Вариант 1: Использовать AFE Wake Word (WakeNet) - ВРЕМЕННО

Пока вы обучаете модель, используйте AFE Wake Word с английской моделью:

```bash
# Откройте menuconfig
idf.py menuconfig
```

**Настройки:**
1. **Xiaozhi Assistant → Wake Word Implementation Type**
   - Выберите: **Wakenet model with AFE** (вместо Multinet model)
2. **ESP Speech Recognition → WakeNet Model Configuration**
   - Выберите английскую модель (например, `CONFIG_SR_WN_WN9_HIESP=y`)

**Или отредактируйте sdkconfig напрямую:**
```ini
# Отключить Multinet
# CONFIG_USE_CUSTOM_WAKE_WORD is not set

# Включить AFE Wake Word
CONFIG_USE_AFE_WAKE_WORD=y

# Выбрать английскую модель
CONFIG_SR_WN_WN9_HIESP=y
```

### Вариант 2: Обучить модель для Multinet (для "Hey Miko")

Для работы с `USE_CUSTOM_WAKE_WORD` нужна модель Multinet:

1. **Обучите модель** (см. `ESP_SR_TRAINING_GUIDE.md` или Issue #88)

2. **Создайте структуру:**
   ```bash
   mkdir -p models/custom_wake_word/hey_miko
   ```

3. **Поместите модель:**
   ```bash
   cp путь/к/обученной/модели.bin models/custom_wake_word/hey_miko/hey_miko.bin
   ```

4. **Создайте конфигурацию:**
   ```bash
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

5. **Пересоберите:**
   ```bash
   idf.py build flash monitor
   ```

## 📋 Текущая ситуация

- ✅ Настройки в sdkconfig правильные
- ❌ Модель отсутствует
- ⚠️ Включена китайская модель WakeNet (конфликт)

## 🎯 Рекомендация

**Сейчас:**
1. Переключитесь на AFE Wake Word с английской моделью (Вариант 1)
2. Протестируйте с "Hi ESP"

**Потом:**
1. Обучите модель для "Hey Miko" (Issue #88)
2. Переключитесь обратно на Multinet (Вариант 2)

## 🔍 Проверка

После размещения модели проверьте:
```bash
# Модель должна быть здесь:
ls -la models/custom_wake_word/hey_miko/hey_miko.bin

# В логах должно быть:
# [WAKE_WORD] Loaded model: hey_miko
# [WAKE_WORD] Registered wake word: 'hey miko'
```



