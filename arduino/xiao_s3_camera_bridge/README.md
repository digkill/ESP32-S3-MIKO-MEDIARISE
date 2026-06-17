# XIAO ESP32-S3 Camera Bridge

Arduino-скетч для Seeed Studio XIAO ESP32-S3 Sense как внешнего модуля камеры, датчиков, диодов и сервоприводов. Команды от Waveshare ESP32-S3 AMOLED передаются по ESP-NOW.

Официальные примеры камеры (расклад пинов `CAMERA_MODEL_XIAO_ESP32S3`, тот же `init`, что в `camera_pins.h`):  
https://github.com/limengdu/SeeedStudio-XIAO-ESP32S3-Sense-camera — для **Arduino ESP32 3.x** брать каталог **`CameraWebServer_for_esp-arduino_3.0.x`**.

Локальный клон (сверка `camera_pins.h` / прошивка эталона):  
`/Users/digkill/Projects/ESP32/SeeedStudio-XIAO-ESP32S3-Sense-camera`

Раскладка `CAM_PIN_*` в скетче совпадает с блоком **`CAMERA_MODEL_XIAO_ESP32S3`** в  
`CameraWebServer_for_esp-arduino_3.0.x/camera_pins.h`.

## Распознавание / детект лиц (CameraWebServer)

В **Arduino ESP32 3.1 и новее** (в т.ч. **3.3.7**) заголовки вроде `human_face_detect_msr01.hpp` **убраны из пакета** — их не положить «с сайта отдельным файлом», это была связка **ядро + esp-dl**, её с новым IDF собрали только под **ESP32-P4**. Разъяснение maintainers и обход: **[espressif/arduino-esp32#10881](https://github.com/espressif/arduino-esp32/issues/10881)** — там же не «репозиторий библиотеки», а ответ: последняя ветка ядра с этими заголовками в комплекте — **3.0.7**.

Чтобы в веб-примере Seeed снова были **Face Detection / Recognition**:

1. **Arduino IDE → Boards Manager → esp32** → выставить версию **3.0.7** (не 3.1+).
2. Открыть **`CameraWebServer_for_esp-arduino_2.0.x`** (не `3.0.x` — там face для S3 специально выключен).
3. **Tools:** PSRAM **OPI**, схема разделов с **большим APP** (как в README примера / «Maximum app»), при необходимости увеличить под размер прошивки.
4. Собрать и залить; в веб-интерфейсе появятся опции face.

Наш бридж `xiao_s3_camera_bridge.ino` **не содержит** моделей лиц — только стрим JPEG по UART. Логика «узнать лицо» либо **CameraWebServer на второй прошивке** (ядро 3.0.7), либо **на ПК/сервере** по принимаемым кадрам.

## Связь с AMOLED по ESP-NOW

По умолчанию `ENABLE_ESPNOW=1`, а проводной `ENABLE_UART_LINK=0`. UART отключён намеренно: его прежний `D2` конфликтует с pitch-сервоприводом.

Обе ESP32-S3 должны работать на одном Wi-Fi-канале. В текущей конфигурации для этого достаточно подключить Waveshare и XIAO к одной сети Wi-Fi 2.4 GHz. MAC прописывать вручную не нужно:

- Waveshare после подключения к Wi-Fi отправляет broadcast `PING`;
- XIAO принимает команду и отвечает на MAC Waveshare;
- команды `SERVO`, `LED`, `DIST?`, `RADAR?`, `STATUS?` далее передаются по ESP-NOW.

Кадры камеры по ESP-NOW не передаются: для JPEG используйте HTTP XIAO (`/capture` или `/stream`). Старый UART-режим можно вернуть флагом `ENABLE_UART_LINK=1` после устранения конфликта пинов.

Для AI-зрения XIAO по умолчанию раз в 15 секунд отправляет JPEG на backend:
`http://90.156.254.46:8080/api/robot/vision/frame`. Настраивается define-параметрами
`ENABLE_VISION_UPLOAD`, `VISION_UPLOAD_INTERVAL_MS`, `VISION_UPLOAD_URL` и
`VISION_DEVICE_ID`. Отправка работает только когда XIAO подключён к Wi-Fi; через
ESP-NOW JPEG передать нельзя из-за размера кадра.

Текущая настройка Wi-Fi из iPhone применяется к основной AMOLED-плате, а сеть
XIAO задаётся `WIFI_SSID`/`WIFI_PASSWORD` в этом скетче перед прошивкой. Пароль
не следует пересылать в XIAO через существующий ESP-NOW broadcast-канал: он не
настроен на шифрование.

В serial monitor XIAO успешная связь видна по строкам:

```text
ESP-NOW: ready mac=.. channel=..
ESP-NOW cmd=PING reply=PONG
```

В monitor AMOLED:

```text
ESP-NOW XIAO bridge enabled on Wi-Fi channel ...
ESP-NOW XIAO peer online: PONG
```

## Подключение периферии

Распиновка по умолчанию задана в начале `xiao_s3_camera_bridge.ino`.

```text
Кольцо LED DIN        -> D10
VL53L0X SDA           -> D4
VL53L0X SCL           -> D5
Серво поворота головы -> D8
Серво наклона головы  -> D9
VCC сервоприводов     -> внешний 5V, не 3V3 от XIAO
```

## Библиотеки Arduino

- ESP32 board package
- `esp32-camera`, идет вместе с ESP32 Arduino core
- `Adafruit NeoPixel`
- `Adafruit VL53L0X`

Плата в Arduino IDE: `Seeed XIAO ESP32S3` / XIAO ESP32-S3 Sense.

## Wi‑Fi и веб‑стрим

В начале `xiao_s3_camera_bridge.ino` задайте (или передайте через **Sketch → Advanced** / флаги компиляции):

```text
WIFI_SSID     — имя сети
WIFI_PASSWORD — пароль
WIFI_HOSTNAME — необязательно, по умолчанию xiao-cam
```

Для ESP-NOW вместе с AMOLED задайте XIAO ту же сеть Wi-Fi 2.4 GHz, что использует робот: подключение фиксирует общий радиоканал.

После прошивки в Serial Monitor будет строка с IP. В браузере:

- `http://<IP>/` — страница с потоком MJPEG;
- `http://<IP>/stream` — поток кадров `multipart/x-mixed-replace` (для `<img src=…>` или VLC);
- `http://<IP>/capture` — один кадр JPEG.

При наличии mDNS: `http://xiao-cam.local/` (имя из `WIFI_HOSTNAME`).

Параллельно UART‑команда `FRAME` / `STREAM` и браузер используют одну камеру: доступ синхронизирован мьютексом (при занятости возможен ответ `CAMERA_BUSY` или задержка кадра).

## UART-команды

Команды текстовые, заканчиваются `\n`.

```text
PING
STATUS?
WIFI?
DIST?
DIST_RAW?
DIST_INFO?
DIST_STREAM ON [interval_ms]
DIST_STREAM OFF
SERVO <yaw> <pitch>
YAW <angle>
PITCH <angle>
LED <r> <g> <b>
PIX <idx> <r> <g> <b>
LEDOFF
FRAME
STREAM ON [interval_ms]
STREAM OFF
HELP
```

Углы сервоприводов ограничены диапазоном `40..80` градусов: центр `60`, максимум `-20/+20`.

Через 30 минут без команды мост автоматически переходит в режим экономии:
выключает LED-кольцо, останавливает стримы, центрирует и отключает PWM серв.
Команды `POWER SLEEP` и `POWER WAKE` позволяют основной плате синхронизировать этот режим.

По умолчанию в скетче включены **VL53L0X** (`ENABLE_VL53 1`) и **сервоприводы** (`ENABLE_SERVOS 1`). Отключение: в начале файла выставить `0` и перепрошить.

- **DIST?** — дистанция в мм (или `ERR` при вне диапазона).
- **DIST_RAW?** — сырой код статуса VL53 и мм (удобно для отладки).
- **DIST_INFO?** — готовность сенсора и пины I2C.
- **DIST_STREAM ON [мс]** — периодически слать строки `DIST <mm>` на USB Serial и на UART линк к основной плате (интервал 50…10000 мс).

## Пакеты камеры

`FRAME` возвращает один JPEG-пакет:

```text
@FRAME <id> <length>\n
<length raw JPEG bytes>
\n@END <id>\n
```

`STREAM ON 500` отправляет такие же JPEG-пакеты каждые 500 мс по UART.

По UART нельзя гнать тяжелый видеопоток. Скетч использует QVGA при наличии PSRAM и QQVGA без PSRAM.
