# Waveshare ESP32-S3-Touch-LCD-1.46

新增 微雪 开发板: ESP32-S3-Touch-LCD-1.46、ESP32-S3-Touch-LCD-1.46B

产品链接：
- https://www.waveshare.net/shop/ESP32-S3-Touch-LCD-1.46.htm
- https://www.waveshare.net/shop/ESP32-S3-Touch-LCD-1.46B.htm

## 编译配置命令

**配置编译目标为 ESP32-S3：**

```bash
idf.py set-target esp32s3
```

**打开 menuconfig：**

```bash
idf.py menuconfig
```

**选择板子：**

```
Xiaozhi Assistant -> Board Type -> Waveshare ESP32-S3-Touch-LCD-1.46
```

**编译：**

```bash
idf.py build
```

**烧录：**

```bash
idf.py flash monitor
```

## 硬件配置

- **显示屏**: 1.46" Touch LCD (240x320)
- **触摸屏**: CST816D (I2C)
- **I2C配置**: 
  - SDA: GPIO13
  - SCL: GPIO15 (注意：与音频I2S MIC SCK共用，但使用不同时间)
  - INT: GPIO4

## 注意事项

- I2C已配置内部上拉电阻，无需外部上拉
- GPIO15同时用于I2C SCL和音频I2S MIC SCK，但它们在运行时不会冲突