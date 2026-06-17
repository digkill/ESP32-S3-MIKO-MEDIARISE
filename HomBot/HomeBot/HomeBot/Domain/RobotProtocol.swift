import Foundation

enum HomeBotBLEProfile {
    static let serviceUUID = "7C100001-180E-4A41-8F4C-8B60C6B9C001"
    static let commandUUID = "7C100002-180E-4A41-8F4C-8B60C6B9C001"
    static let eventUUID = "7C100003-180E-4A41-8F4C-8B60C6B9C001"
}

struct RobotCommandEnvelope<Payload: Encodable>: Encodable {
    let version = 1
    let id = UUID().uuidString
    let type: String
    let payload: Payload
}

enum MediaAction: String, Codable {
    case previous
    case playPause = "play_pause"
    case next
    case stop
}

enum RobotCommand {
    struct WiFiPayload: Encodable {
        let ssid: String
        let password: String
    }

    struct TimePayload: Encodable {
        let unixMilliseconds: Int64
        let timeZone: String
    }

    struct EmotionPayload: Encodable {
        let name: String
    }

    struct ScenePayload: Encodable {
        let name: String
    }

    struct MediaPayload: Encodable {
        let action: MediaAction
    }

    struct VolumePayload: Encodable {
        let percent: Int
    }

    struct BrightnessPayload: Encodable {
        let percent: Int
    }

    struct SpeechPayload: Encodable {
        let text: String
    }

    struct DialogPayload: Encodable {
        let text: String
        let language: String
    }

    struct XiaoServoPayload: Encodable {
        let yaw: Int
        let pitch: Int
    }

    struct XiaoPosePayload: Encodable {
        let name: String
    }

    struct XiaoLedPayload: Encodable {
        let red: Int
        let green: Int
        let blue: Int
    }

    struct XiaoActionPayload: Encodable {
        let action: String
    }

    struct EmptyPayload: Encodable {}

    case configureWiFi(ssid: String, password: String)
    case resetWiFi
    case setTime(date: Date, timeZone: TimeZone)
    case setEmotion(String)
    case playScene(String)
    case media(MediaAction)
    case setVolume(Int)
    case setBrightness(Int)
    case readText(String)
    case sendDialog(text: String, language: String)
    case setXiaoServo(yaw: Int, pitch: Int)
    case setXiaoPose(String)
    case setXiaoLED(red: Int, green: Int, blue: Int)
    case xiaoLEDAction(String)
    case requestXiaoStatus
    case requestXiaoDistance
    case requestXiaoRadar

    func encoded() throws -> Data {
        let encoder = JSONEncoder()
        switch self {
        case let .configureWiFi(ssid, password):
            return try encoder.encode(
                RobotCommandEnvelope(type: "wifi.configure", payload: WiFiPayload(ssid: ssid, password: password))
            )
        case .resetWiFi:
            return try encoder.encode(
                RobotCommandEnvelope(type: "wifi.reset", payload: EmptyPayload())
            )
        case let .setTime(date, timeZone):
            return try encoder.encode(
                RobotCommandEnvelope(
                    type: "time.set",
                    payload: TimePayload(
                        unixMilliseconds: Int64(date.timeIntervalSince1970 * 1000),
                        timeZone: timeZone.identifier
                    )
                )
            )
        case let .setEmotion(name):
            return try encoder.encode(
                RobotCommandEnvelope(type: "emotion.set", payload: EmotionPayload(name: name))
            )
        case let .playScene(name):
            return try encoder.encode(
                RobotCommandEnvelope(type: "scene.play", payload: ScenePayload(name: name))
            )
        case let .media(action):
            return try encoder.encode(
                RobotCommandEnvelope(type: "media.control", payload: MediaPayload(action: action))
            )
        case let .setVolume(value):
            return try encoder.encode(
                RobotCommandEnvelope(type: "audio.volume", payload: VolumePayload(percent: min(max(value, 0), 100)))
            )
        case let .setBrightness(value):
            return try encoder.encode(
                RobotCommandEnvelope(type: "display.brightness", payload: BrightnessPayload(percent: min(max(value, 5), 100)))
            )
        case let .readText(text):
            return try encoder.encode(
                RobotCommandEnvelope(type: "speech.read", payload: SpeechPayload(text: text))
            )
        case let .sendDialog(text, language):
            return try encoder.encode(
                RobotCommandEnvelope(type: "dialog.send", payload: DialogPayload(text: text, language: language))
            )
        case let .setXiaoServo(yaw, pitch):
            return try encoder.encode(
                RobotCommandEnvelope(
                    type: "xiao.servo",
                    payload: XiaoServoPayload(
                        yaw: min(max(yaw, 40), 80),
                        pitch: min(max(pitch, 40), 80)
                    )
                )
            )
        case let .setXiaoPose(name):
            return try encoder.encode(
                RobotCommandEnvelope(type: "xiao.pose", payload: XiaoPosePayload(name: name))
            )
        case let .setXiaoLED(red, green, blue):
            return try encoder.encode(
                RobotCommandEnvelope(
                    type: "xiao.led",
                    payload: XiaoLedPayload(
                        red: min(max(red, 0), 255),
                        green: min(max(green, 0), 255),
                        blue: min(max(blue, 0), 255)
                    )
                )
            )
        case let .xiaoLEDAction(action):
            return try encoder.encode(
                RobotCommandEnvelope(type: "xiao.led_action", payload: XiaoActionPayload(action: action))
            )
        case .requestXiaoStatus:
            return try encoder.encode(
                RobotCommandEnvelope(type: "xiao.status", payload: EmptyPayload())
            )
        case .requestXiaoDistance:
            return try encoder.encode(
                RobotCommandEnvelope(type: "xiao.distance", payload: EmptyPayload())
            )
        case .requestXiaoRadar:
            return try encoder.encode(
                RobotCommandEnvelope(type: "xiao.radar", payload: EmptyPayload())
            )
        }
    }
}

struct RobotEvent: Decodable {
    let type: String
    let message: String?
    let status: String?
    let batteryPercent: Int?
    let wifiSSID: String?
}
