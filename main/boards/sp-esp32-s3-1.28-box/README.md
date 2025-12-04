# Описание продукта

Spotpear ESP32-S3-1.28-BOX - это устройство на базе ESP32-S3 с круглым дисплеем 1.28 дюйма.

**Характеристики:**
- Поддержка сенсорного экрана
- Поддержка зарядки
- Уникальный дизайн корпуса

**Ссылки на продукт:**
- [Ссылка 1](https://spotpear.cn/shop/ESP32-S3-N16R8-AI-DeepSeek-XiaoZhi-XiaGe-Qwen-DouBao-1.28-inch-LCD.html)
- [Ссылка 2](https://spotpear.cn/shop/ESP32-S3-N16R8-AI-DeepSeek-XiaoZhi-XiaGe-Qwen-DouBao-1.28-inch-Round-LCD-BOX-TouchScreen.html)

# Команды для настройки компиляции

**Настроить цель компиляции для ESP32S3:**

```bash
idf.py set-target esp32s3
```

**Открыть menuconfig:**

```bash
idf.py menuconfig
```

**Выбрать плату:**

```
Xiaozhi Assistant -> Board Type -> Spotpear ESP32-S3-1.28-BOX
```

**Скомпилировать:**

```bash
idf.py build
```
