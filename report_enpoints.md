# Отчет по Endpoints и Протоколам Коммуникации

## Обзор

Данный отчет описывает все endpoints и протоколы коммуникации, используемые в проекте Xiaozhi ESP32. Устройство взаимодействует с внешними серверами через несколько протоколов: HTTP, WebSocket, MQTT и UDP.

---

## 1. HTTP Endpoints

### 1.1 OTA (Over-The-Air) Endpoint

**Назначение:** Проверка наличия обновлений прошивки, получение конфигурации сервера и активация устройства.

**URL:** 
- По умолчанию: `https://api.tenclass.net/xiaozhi/ota/`
- Настраивается через: `Settings("wifi")->GetString("ota_url")`

**Методы:**

#### 1.1.1 Проверка версии (GET/POST)

**Endpoint:** `{OTA_URL}`

**Метод:** GET или POST (POST если отправляются данные о системе)

**Заголовки:**
```
Activation-Version: 1 или 2 (в зависимости от наличия серийного номера)
Device-Id: MAC-адрес устройства
Client-Id: UUID устройства
Serial-Number: Серийный номер (если доступен)
User-Agent: Информация об устройстве
Accept-Language: Код языка (например, "ru", "en")
Content-Type: application/json
```

**Тело запроса (POST):**
JSON с информацией о системе устройства (возвращается `Board::GetSystemInfoJson()`)

**Ответ (200 OK):**
```json
{
  "firmware": {
    "version": "1.0.0",
    "url": "https://example.com/firmware.bin",
    "force": 0  // 1 = принудительное обновление
  },
  "activation": {
    "message": "Текст сообщения",
    "code": "код активации",
    "challenge": "challenge строка",
    "timeout_ms": 30000
  },
  "mqtt": {
    "endpoint": "mqtt.example.com:8883",
    "client_id": "device_id",
    "username": "user",
    "password": "pass",
    "publish_topic": "device/commands",
    "keepalive": 240
  },
  "websocket": {
    "url": "wss://example.com/ws",
    "token": "bearer_token",
    "version": 3
  },
  "server_time": {
    "timestamp": 1234567890000,  // миллисекунды
    "timezone_offset": 180  // минуты
  }
}
```

**Обработка ответа:**
- Проверка наличия новой версии прошивки
- Сохранение конфигурации MQTT и WebSocket в настройках
- Установка системного времени
- Обработка данных активации

#### 1.1.2 Активация устройства (POST)

**Endpoint:** `{OTA_URL}/activate`

**Метод:** POST

**Заголовки:** Те же, что и для проверки версии

**Тело запроса:**
```json
{
  "algorithm": "hmac-sha256",
  "serial_number": "серийный_номер",
  "challenge": "challenge_строка_из_ответа_проверки_версии",
  "hmac": "hex_строка_hmac_результата"
}
```

**HMAC вычисление:**
- Используется `esp_hmac_calculate()` с `HMAC_KEY0`
- Входные данные: `challenge` строка
- Результат: SHA-256 HMAC в hex формате

**Ответ:**
- `200 OK` - активация успешна
- `202 Accepted` - активация в процессе (таймаут)
- Другие коды - ошибка активации

#### 1.1.3 Загрузка прошивки (GET)

**Endpoint:** URL из ответа проверки версии (`firmware.url`)

**Метод:** GET

**Описание:** Загружает бинарный файл прошивки для OTA обновления. Прогресс загрузки отслеживается через callback функцию.

---

### 1.2 Загрузка Ресурсов (Assets)

**Назначение:** Загрузка обновленных ресурсов (изображения, звуки, языковые файлы и т.д.)

**URL:** Настраивается через `Settings("assets")->GetString("download_url")`

**Метод:** GET

**Описание:** 
- Загружает бинарный файл ресурсов
- Записывает данные в специальный раздел flash памяти
- Поддерживает прогресс загрузки
- После загрузки переинициализирует раздел ресурсов

**Ответ:** Бинарный файл ресурсов

---

### 1.3 Загрузка Скриншота Экрана

**Назначение:** Загрузка скриншота экрана устройства на внешний сервер

**URL:** Динамический, передается через MCP инструмент `self.screen.snapshot`

**Метод:** POST

**Content-Type:** `multipart/form-data`

**Тело запроса:**
```
------ESP32_SCREEN_SNAPSHOT_BOUNDARY
Content-Disposition: form-data; name="file"; filename="screenshot.jpg"
Content-Type: image/jpeg

[JPEG данные]
------ESP32_SCREEN_SNAPSHOT_BOUNDARY--
```

**Параметры:**
- `url` (string) - URL для загрузки
- `quality` (integer, 1-100) - Качество JPEG (по умолчанию 80)

**Ответ:** 
- `200 OK` - успешная загрузка
- Другие коды - ошибка

**Использование:** Вызывается через MCP протокол, инструмент `self.screen.snapshot`

---

## 2. WebSocket Protocol

**Назначение:** Основной протокол для двусторонней коммуникации с сервером, включая передачу аудио и JSON сообщений.

**URL:** Настраивается через `Settings("websocket")->GetString("url")`

**Протокол:** WebSocket (ws:// или wss://)

**Версия протокола:** Настраивается через `Settings("websocket")->GetInt("version")` (1, 2 или 3)

### 2.1 Заголовки WebSocket

При подключении отправляются следующие заголовки:

```
Authorization: Bearer {token}  // Если токен настроен
Protocol-Version: {version}     // Версия протокола (1, 2 или 3)
Device-Id: {MAC-адрес}          // MAC-адрес устройства
Client-Id: {UUID}               // UUID устройства
```

### 2.2 Hello Сообщение (Устройство → Сервер)

Отправляется сразу после подключения:

```json
{
  "type": "hello",
  "version": 1,  // или 2, или 3
  "features": {
    "aec": true,  // если CONFIG_USE_SERVER_AEC включен
    "mcp": true   // всегда true
  },
  "transport": "websocket",
  "audio_params": {
    "format": "opus",
    "sample_rate": 16000,
    "channels": 1,
    "frame_duration": 60  // миллисекунды
  }
}
```

### 2.3 Hello Ответ (Сервер → Устройство)

```json
{
  "type": "hello",
  "transport": "websocket",
  "session_id": "уникальный_идентификатор_сессии",
  "audio_params": {
    "sample_rate": 16000,  // или другой, если сервер требует
    "frame_duration": 60
  }
}
```

### 2.4 JSON Сообщения (Текстовые фреймы)

#### 2.4.1 Устройство → Сервер

**Listen (Начало/Остановка прослушивания):**
```json
{
  "session_id": "...",
  "type": "listen",
  "state": "start",  // или "stop", или "detect"
  "mode": "manual"   // или "auto", или "realtime"
}
```

**Wake Word Detected:**
```json
{
  "session_id": "...",
  "type": "listen",
  "state": "detect",
  "text": "Hey Miko"
}
```

**Abort (Прерывание речи):**
```json
{
  "session_id": "...",
  "type": "abort",
  "reason": "wake_word_detected"  // опционально
}
```

**MCP Сообщение:**
```json
{
  "session_id": "...",
  "type": "mcp",
  "payload": {
    "jsonrpc": "2.0",
    "method": "...",
    "params": {...},
    "id": 1
  }
}
```

#### 2.4.2 Сервер → Устройство

**STT (Speech-to-Text):**
```json
{
  "session_id": "...",
  "type": "stt",
  "text": "распознанный текст"
}
```

**TTS (Text-to-Speech):**
```json
{
  "session_id": "...",
  "type": "tts",
  "state": "start"  // или "stop", или "sentence_start"
}
```

**LLM (Эмоции):**
```json
{
  "session_id": "...",
  "type": "llm",
  "emotion": "happy",
  "text": "😀"
}
```

**MCP:**
```json
{
  "session_id": "...",
  "type": "mcp",
  "payload": {
    "jsonrpc": "2.0",
    "method": "tools/call",
    "params": {
      "name": "self.light.set_rgb",
      "arguments": {"r": 255, "g": 0, "b": 0}
    },
    "id": 1
  }
}
```

**System:**
```json
{
  "session_id": "...",
  "type": "system",
  "command": "..."
}
```

### 2.5 Бинарные Сообщения (Аудио)

#### 2.5.1 Версия 1 (По умолчанию)
Прямая передача Opus аудио данных без дополнительных заголовков.

#### 2.5.2 Версия 2
```c
struct BinaryProtocol2 {
    uint16_t version;        // Версия протокола (network byte order)
    uint16_t type;           // Тип сообщения (0: OPUS, 1: JSON)
    uint32_t reserved;       // Зарезервировано
    uint32_t timestamp;      // Временная метка в миллисекундах (для AEC)
    uint32_t payload_size;   // Размер полезной нагрузки (network byte order)
    uint8_t payload[];       // Opus аудио данные
} __attribute__((packed));
```

#### 2.5.3 Версия 3
```c
struct BinaryProtocol3 {
    uint8_t type;            // Тип сообщения
    uint8_t reserved;        // Зарезервировано
    uint16_t payload_size;   // Размер полезной нагрузки (network byte order)
    uint8_t payload[];      // Opus аудио данные
} __attribute__((packed));
```

**Направление передачи:**
- Устройство → Сервер: Записанный аудио поток (Opus)
- Сервер → Устройство: Синтезированная речь (Opus)

---

## 3. MQTT Protocol

**Назначение:** Альтернативный протокол для коммуникации с сервером, использует MQTT для JSON сообщений и UDP для аудио.

**Endpoint:** Настраивается через `Settings("mqtt")->GetString("endpoint")` (формат: `host:port`)

**Порт по умолчанию:** 8883 (TLS)

**Параметры подключения:**
- `client_id`: `Settings("mqtt")->GetString("client_id")`
- `username`: `Settings("mqtt")->GetString("username")`
- `password`: `Settings("mqtt")->GetString("password")`
- `keepalive`: `Settings("mqtt")->GetInt("keepalive", 240)` (секунды)
- `publish_topic`: `Settings("mqtt")->GetString("publish_topic")`

### 3.1 Hello Сообщение (Устройство → Сервер)

Отправляется в MQTT топик после подключения:

```json
{
  "type": "hello",
  "version": 3,
  "transport": "udp",
  "features": {
    "aec": true,  // если CONFIG_USE_SERVER_AEC включен
    "mcp": true
  },
  "audio_params": {
    "format": "opus",
    "sample_rate": 16000,
    "channels": 1,
    "frame_duration": 60
  }
}
```

### 3.2 Hello Ответ (Сервер → Устройство)

```json
{
  "type": "hello",
  "transport": "udp",
  "session_id": "уникальный_идентификатор_сессии",
  "audio_params": {
    "sample_rate": 16000,
    "frame_duration": 60
  },
  "udp": {
    "server": "192.168.1.100",
    "port": 8888,
    "key": "0123456789ABCDEF0123456789ABCDEF",  // hex строка, 128 бит
    "nonce": "0123456789ABCDEF0123456789ABCDEF"  // hex строка, 16 байт
  }
}
```

### 3.3 JSON Сообщения

Формат JSON сообщений идентичен WebSocket протоколу (см. раздел 2.4).

**Отправка:** Через MQTT `publish_topic`

**Получение:** Через подписку на MQTT топик (настраивается сервером)

### 3.4 Goodbye Сообщение

**Устройство → Сервер:**
```json
{
  "session_id": "...",
  "type": "goodbye"
}
```

**Сервер → Устройство:**
```json
{
  "session_id": "...",
  "type": "goodbye"
}
```

### 3.5 Переподключение

При разрыве соединения устройство автоматически переподключается через 30 секунд (`MQTT_RECONNECT_INTERVAL_MS`).

---

## 4. UDP Audio Channel (для MQTT)

**Назначение:** Передача зашифрованного аудио потока при использовании MQTT протокола.

**Сервер и порт:** Получаются из ответа Hello сообщения (`udp.server`, `udp.port`)

**Шифрование:** AES-128-CTR

**Ключ и Nonce:** Получаются из ответа Hello (`udp.key`, `udp.nonce`)

### 4.1 Формат UDP Пакета

**Устройство → Сервер:**
```
[16 байт nonce][зашифрованные Opus данные]
```

**Nonce структура:**
```
[2 байта: тип и флаги][2 байта: размер payload (network byte order)]
[4 байта: SSRC][4 байта: timestamp (network byte order)]
[4 байта: sequence number (network byte order)]
```

**Сервер → Устройство:**
Аналогичный формат, но с другими sequence numbers.

### 4.2 Обработка

- Каждый пакет содержит sequence number для проверки порядка
- Используется AES-128-CTR для шифрования/дешифрования
- Timestamp используется для синхронизации аудио

---

## 5. MCP (Model Context Protocol)

**Назначение:** Протокол для удаленного управления функциями устройства через JSON-RPC 2.0.

**Транспорт:** Инкапсулирован в WebSocket или MQTT JSON сообщениях с `"type": "mcp"`

**Спецификация:** JSON-RPC 2.0 (https://www.jsonrpc.org/specification)

### 5.1 Формат MCP Сообщения

```json
{
  "session_id": "...",
  "type": "mcp",
  "payload": {
    "jsonrpc": "2.0",
    "method": "...",
    "params": {...},
    "id": 1,
    "result": {...},  // для ответов
    "error": {...}    // для ошибок
  }
}
```

### 5.2 Методы MCP

#### 5.2.1 initialize

**Назначение:** Инициализация MCP сессии

**Запрос (Сервер → Устройство):**
```json
{
  "jsonrpc": "2.0",
  "method": "initialize",
  "params": {
    "capabilities": {
      "vision": {
        "url": "http://example.com/vision",
        "token": "token_string"
      }
    }
  },
  "id": 1
}
```

**Ответ (Устройство → Сервер):**
```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "protocolVersion": "2024-11-05",
    "capabilities": {
      "tools": {}
    },
    "serverInfo": {
      "name": "xiaozhi-esp32",
      "version": "1.0.0"
    }
  }
}
```

#### 5.2.2 tools/list

**Назначение:** Получение списка доступных инструментов

**Запрос (Сервер → Устройство):**
```json
{
  "jsonrpc": "2.0",
  "method": "tools/list",
  "id": 2
}
```

**Ответ (Устройство → Сервер):**
```json
{
  "jsonrpc": "2.0",
  "id": 2,
  "result": {
    "tools": [
      {
        "name": "self.light.set_rgb",
        "description": "Set RGB light color",
        "inputSchema": {
          "type": "object",
          "properties": {
            "r": {"type": "integer", "minimum": 0, "maximum": 255},
            "g": {"type": "integer", "minimum": 0, "maximum": 255},
            "b": {"type": "integer", "minimum": 0, "maximum": 255}
          }
        }
      }
    ]
  }
}
```

#### 5.2.3 tools/call

**Назначение:** Вызов инструмента на устройстве

**Запрос (Сервер → Устройство):**
```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "self.light.set_rgb",
    "arguments": {
      "r": 255,
      "g": 0,
      "b": 0
    }
  },
  "id": 3
}
```

**Ответ (Устройство → Сервер):**
```json
{
  "jsonrpc": "2.0",
  "id": 3,
  "result": {
    "content": [
      {
        "type": "text",
        "text": "Light color set to red"
      }
    ]
  }
}
```

### 5.3 Доступные Инструменты

Устройство предоставляет множество инструментов через MCP, включая:

- `self.light.*` - управление светом
- `self.screen.snapshot` - создание скриншота экрана
- `self.system.*` - системные функции
- И другие (см. `main/mcp_server.cc`)

---

## 6. Таймауты и Обработка Ошибок

### 6.1 Таймауты

- **WebSocket Hello:** 10 секунд ожидания ответа
- **MQTT Hello:** 10 секунд ожидания ответа
- **Аудио канал:** 120 секунд бездействия (timeout)
- **MQTT переподключение:** 30 секунд задержка

### 6.2 Обработка Ошибок

- При ошибке соединения вызывается callback `OnNetworkError`
- При таймауте закрывается аудио канал
- При ошибке отправки сообщения устанавливается флаг `error_occurred_`

---

## 7. Настройки и Конфигурация

### 7.1 Хранение Настроек

Все настройки хранятся в NVS (Non-Volatile Storage) через класс `Settings`:

- `Settings("wifi")` - настройки WiFi и OTA
- `Settings("websocket")` - настройки WebSocket
- `Settings("mqtt")` - настройки MQTT
- `Settings("assets")` - настройки ресурсов

### 7.2 Конфигурация через OTA

Настройки WebSocket и MQTT могут быть обновлены через ответ OTA endpoint (см. раздел 1.1.1).

---

## 8. Безопасность

### 8.1 Аутентификация

- **WebSocket:** Bearer токен в заголовке `Authorization`
- **MQTT:** Username/Password при подключении
- **OTA:** Device-Id, Client-Id, Serial-Number в заголовках

### 8.2 Шифрование

- **WebSocket:** TLS (wss://)
- **MQTT:** TLS (порт 8883)
- **UDP Audio:** AES-128-CTR с ключом и nonce от сервера

### 8.3 Активация Устройства

Для устройств с серийным номером используется HMAC-SHA256 для активации (см. раздел 1.1.2).

---

## 9. Диаграмма Последовательности

### 9.1 Инициализация и Подключение

```
Устройство                    OTA Server              WebSocket/MQTT Server
    |                              |                            |
    |-- GET/POST /ota/ ----------->|                            |
    |<-- 200 OK (config) ----------|                            |
    |                                                           |
    |-- WebSocket Connect ------------------------------------->|
    |-- Hello Message ----------------------------------------->|
    |<-- Hello Response ----------------------------------------|
    |                                                           |
    |-- Audio Channel Ready ----------------------------------->|
```

### 9.2 Взаимодействие с Пользователем

```
Устройство                    Server
    |                            |
    |-- Wake Word Detected ----->|
    |<-- Listen Start -----------|
    |                            |
    |-- Audio Stream (Opus) ----->|
    |                            |
    |<-- STT Result -------------|
    |<-- LLM Response -----------|
    |<-- TTS Start --------------|
    |<-- Audio Stream (Opus) <---|
    |                            |
    |<-- TTS Stop ---------------|
```

---

## 10. Заключение

Проект Xiaozhi ESP32 использует комплексную систему коммуникации с внешними серверами:

1. **HTTP** для OTA обновлений, загрузки ресурсов и скриншотов
2. **WebSocket** для основной двусторонней коммуникации
3. **MQTT** как альтернативный протокол коммуникации
4. **UDP** для эффективной передачи аудио при использовании MQTT
5. **MCP** для удаленного управления функциями устройства

Все протоколы поддерживают шифрование и аутентификацию для обеспечения безопасности коммуникации.

---

**Дата создания отчета:** 2024
**Версия прошивки:** Текущая версия проекта
**Автор:** Анализ кодовой базы

