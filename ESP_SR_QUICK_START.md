# Быстрый старт: Обучение "Hey Miko" с ESP-SR V2.0

## 🚀 Самый быстрый способ (TTS обучение)

### 1. Установка ESP-SR

```bash
cd ~/Projects/ESP32
git clone https://github.com/espressif/esp-sr.git
cd esp-sr
pip install -r requirements.txt
```

### 2. Изучите Issue #88

**Ключевой ресурс:** https://github.com/espressif/esp-sr/issues/88

Это официальный способ обучения wake word через TTS Pipeline V2.0. Не требует реальных записей!

### 3. Следуйте инструкциям из Issue #88

В issue #88 есть пошаговые инструкции по:
- Настройке TTS Pipeline V2.0
- Генерации TTS данных для "Hey Miko"
- Обучению модели
- Конвертации в формат для ESP32

### 4. Разместите модель в проекте

```bash
cd ~/Projects/ESP32/mediarise-robot-console/xiaozhi-esp32
mkdir -p models/custom_wake_word/hey_miko
cp путь/к/обученной/модели.bin models/custom_wake_word/hey_miko/hey_miko.bin
```

### 5. Настройте menuconfig

```bash
idf.py menuconfig
```

Выберите:
- **Wake Word Implementation Type** → **Wakenet model with AFE**
- (Для MultiNet: **Multinet model (Custom Wake Word)**)

### 6. Соберите и прошейте

```bash
idf.py build flash monitor
```

---

## 📚 Полезные ссылки

- **ESP-SR GitHub**: https://github.com/espressif/esp-sr
- **ESP-SR Документация**: https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/index.html
- **TTS Training (Issue #88)**: https://github.com/espressif/esp-sr/issues/88
- **Wake Word Customization**: https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/wake_word_engine/ESP_Wake_Words_Customization.html
- **Migration Guide V1→V2**: https://docs.espressif.com/projects/esp-sr/en/latest/esp32s3/audio_front_end/migration_guide.html

---

## ⚠️ Важные замечания

1. **TTS обучение (Issue #88)** - самый простой способ для начала
2. **Официальный процесс** требует 20,000+ записей от 500+ человек
3. **Для продакшена** лучше использовать реальные записи или услуги Espressif
4. **WakeNet** рекомендуется для wake word (не MultiNet)

---

## 🎯 Выбор модели

### WakeNet (рекомендуется для "Hey Miko")
- Специально для wake word
- Низкое потребление ресурсов
- Высокая точность
- Используйте: **Wakenet model with AFE**

### MultiNet (для команд)
- Для распознавания команд (до 300)
- Можно добавлять команды без переобучения
- Больше ресурсов
- Используйте: **Multinet model (Custom Wake Word)**

---

## 📖 Подробная инструкция

См. файл: `ESP_SR_TRAINING_GUIDE.md`



