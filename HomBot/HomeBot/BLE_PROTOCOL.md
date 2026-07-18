# HomeBot BLE Protocol v1

The iOS application and ESP32-C6/ESP32-S3 firmware communicate through this BLE GATT
service alongside the existing Wi-Fi/MCP communication.

## GATT Profile

| Item | UUID | Direction |
| --- | --- | --- |
| HomeBot service | `7C100001-180E-4A41-8F4C-8B60C6B9C001` | Advertised service |
| Command characteristic | `7C100002-180E-4A41-8F4C-8B60C6B9C001` | iPhone writes with response |
| Event characteristic | `7C100003-180E-4A41-8F4C-8B60C6B9C001` | ESP32 notifies |

The ESP32 advertises the service using the local name `HomeBot-C6` or `HomeBot-S3`.
Wi-Fi credentials must only be accepted after BLE pairing/bonding is enabled.

The iOS Wi-Fi screen supports credentials for either a normal router network or
the iPhone Personal Hotspot. Personal Hotspot must be enabled by the user in
iPhone Settings; iOS does not expose an API for this app to enable it or read
saved Wi-Fi passwords. For ESP32 clients, enable Maximize Compatibility on the
iPhone hotspot so it advertises a compatible 2.4 GHz/WPA2 network.

## Command Envelope

Every command is UTF-8 JSON:

```json
{
  "version": 1,
  "id": "UUID",
  "type": "emotion.set",
  "payload": { "name": "happy" }
}
```

Supported command types:

| Type | Payload | Firmware action |
| --- | --- | --- |
| `wifi.configure` | `{"ssid":"...","password":"..."}` | Store station credentials and restart to connect |
| `wifi.reset` | `{}` | Clear saved credentials |
| `time.set` | `{"unixMilliseconds":0,"timeZone":"Asia/Yekaterinburg"}` | Set clock/display time |
| `emotion.set` | `{"name":"happy"}` | Set display emotion |
| `emotion.set` | `{"name":"coffee"}` | Show the animated coffee cat state |
| `emotion.video` | `{"name":"happy"}` | Play the full-screen MJPEG clip `/sdcard/<name>_emotion.mp4` (e.g. `happy_emotion.mp4`); UI is paused for the clip duration. Boards without an SD/video display reply `unsupported` |
| `scene.play` | `{"name":"dance"}` | Run a composed face/head/LED scene: `dance`, `greet`, `curious`, `love`, `celebrate`, `coffee`, `sleep` or `idle` |
| `media.control` | `{"action":"stop"}` | Stop current speech/audio activity |
| `audio.volume` | `{"percent":50}` | Set output volume from 0 to 100 |
| `audio.mute` | `{"muted":true}` | Mute all sound (output volume 0). `{"muted":false}` restores the previous volume |
| `power.save` | `{"enabled":true}` | Enter low-power mode: dim screen to 5%, pause display and Wi-Fi. `{"enabled":false}` restores brightness and normal mode |
| `display.brightness` | `{"percent":75}` | Set and save AMOLED brightness from 5 to 100 percent |
| `speech.read` | `{"text":"..."}` | Send a direct `tts` request so the connected voice service reads the supplied text |
| `dialog.send` | `{"text":"...","language":"Русский"}` | Ask the voice service for a spoken assistant reply |
| `xiao.servo` | `{"yaw":60,"pitch":60}` | Move head servos within `40..80` degrees (`60 +/- 20`) |
| `xiao.pose` | `{"name":"home"}` | Apply `home`, `left`, `right`, `up`, `down`, `nod`, `shake` or `dance` pose |
| `xiao.led` | `{"red":0,"green":120,"blue":255}` | Set XIAO-connected RGB ring |
| `xiao.led_action` | `{"action":"default"}` | Apply `default`, `off` or `test` LED action |
| `xiao.status` | `{}` | Request bridge/peripheral status through ESP-NOW |
| `xiao.distance` | `{}` | Request VL53L0X distance through ESP-NOW |
| `xiao.radar` | `{}` | Request radar presence information through ESP-NOW |

After 30 minutes without activity the robot shows the sleeping face at 10%
AMOLED brightness and puts XIAO actuators into low-power mode. A BLE command,
button/touch event, shake reaction or regular interaction wakes it and restores
the saved brightness.

## Events

Events use JSON notifications. For example, after encrypted pairing completes:

```json
{
  "type": "status",
  "message": "Secure BLE channel ready",
  "status": "ready"
}
```

The ESP32 adapter validates commands and forwards available operations to
the existing display/audio/Wi-Fi implementations. The current audio pipeline
supports `media.control` action `stop`; other transport actions return
`unsupported`.

XIAO request responses are sent as `xiao.status`, `xiao.distance` and
`xiao.radar` events. Failed ESP-NOW requests use status `timeout`; unavailable
or invalid operations use `xiao.error`.
