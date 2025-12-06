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

- **显示屏**: 1.46" Touch LCD (240x320, SPD2010)
- **触摸屏**: CST816D (I2C)
- **I2C配置**: 
  - SDA: GPIO13
  - SCL: GPIO15
  - INT: GPIO4
- **内置麦克风**: I2S接口 (встроенный микрофон)
  - WS (LRCLK): GPIO2 (I2S_PIN_WS)
  - SCK (BCLK): GPIO15 (I2S_PIN_BCK)
  - DIN: GPIO39 (I2S_PIN_DIN)
- **扬声器**: I2S接口
  - DOUT: GPIO47
  - BCLK: GPIO48
  - LRCK: GPIO38

## 注意事项

- I2C已配置内部上拉电阻，无需外部上拉
- GPIO15同时用于I2C SCL (тачскрин) 和 I2S MIC SCK (микрофон)
- I2C和I2S使用不同的时间，因此不会产生冲突
- 麦克风使用标准I2S接口，配置已验证工作正常
- **Встроенный микрофон пины**: WS=GPIO2, SCK=GPIO15, DIN=GPIO39