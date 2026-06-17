import Combine
import Foundation

@MainActor
final class HomeBotSession: ObservableObject {
    let transport = BluetoothRobotTransport()
    lazy var controls = RobotControlService(transport: transport)
    lazy var xiao = XiaoControlService(transport: transport)
    lazy var dialog = RobotDialogService(transport: transport)
    lazy var provisioning = WiFiProvisioningService(transport: transport)
}

@MainActor
final class RobotControlService: ObservableObject {
    @Published var errorMessage: String?
    @Published private(set) var sceneMessage: String?
    @Published var screenBrightness = 100.0

    private let transport: BluetoothRobotTransport
    private var eventSubscription: AnyCancellable?

    init(transport: BluetoothRobotTransport) {
        self.transport = transport
        eventSubscription = transport.$lastEvent
            .compactMap { $0 }
            .sink { [weak self] event in
                guard let self else { return }
                if event.type == "scene" {
                    self.sceneMessage = event.status == "ready"
                        ? "Сценарий запущен"
                        : (event.message ?? "Не удалось запустить сценарий")
                    if event.status != "ready" {
                        self.errorMessage = self.sceneMessage
                    }
                } else if event.type == "scene.error" {
                    self.errorMessage = event.message ?? "Ошибка сценария"
                }
            }
    }

    func setEmotion(_ emotion: String) {
        execute(.setEmotion(emotion))
    }

    func playScene(_ scene: String) {
        execute(.playScene(scene))
    }

    func syncTime() {
        execute(.setTime(date: Date(), timeZone: .current))
    }

    func media(_ action: MediaAction) {
        execute(.media(action))
    }

    func setVolume(_ percent: Int) {
        execute(.setVolume(percent))
    }

    func setBrightness(_ percent: Int) {
        execute(.setBrightness(percent))
    }

    func readText(_ text: String) {
        let message = String(text.trimmingCharacters(in: .whitespacesAndNewlines).prefix(100))
        guard !message.isEmpty else {
            errorMessage = "Введите сообщение для робота."
            return
        }
        execute(.readText(message))
    }

    private func execute(_ command: RobotCommand) {
        do {
            try transport.send(command)
            errorMessage = nil
        } catch {
            errorMessage = error.localizedDescription
        }
    }
}

@MainActor
final class XiaoControlService: ObservableObject {
    @Published var yaw = 60.0
    @Published var pitch = 60.0
    @Published private(set) var statusMessage = "Нет данных"
    @Published private(set) var distanceMessage = "Нет данных"
    @Published private(set) var radarMessage = "Нет данных"
    @Published var errorMessage: String?

    private let transport: BluetoothRobotTransport
    private var eventSubscription: AnyCancellable?

    init(transport: BluetoothRobotTransport) {
        self.transport = transport
        eventSubscription = transport.$lastEvent
            .compactMap { $0 }
            .sink { [weak self] event in
                guard let self else { return }
                switch event.type {
                case "xiao.status":
                    self.statusMessage = event.message ?? "Нет ответа"
                case "xiao.distance":
                    self.distanceMessage = event.message ?? "Нет ответа"
                case "xiao.radar":
                    self.radarMessage = event.message ?? "Нет ответа"
                case "xiao.error":
                    self.errorMessage = event.message ?? "Ошибка XIAO"
                default:
                    break
                }
                if event.type.hasPrefix("xiao."), event.status == "timeout" {
                    self.errorMessage = "XIAO не отвечает по ESP-NOW."
                }
            }
    }

    func sendServos() {
        // The installed robot has the turn/nod servo leads connected in reverse.
        send(.setXiaoServo(yaw: Int(pitch), pitch: Int(yaw)))
    }

    func setPose(_ name: String) {
        let swappedPoses = [
            "left": "up",
            "right": "down",
            "up": "left",
            "down": "right",
            "nod": "shake",
            "shake": "nod"
        ]
        send(.setXiaoPose(swappedPoses[name] ?? name))
    }

    func sendLED(red: Int, green: Int, blue: Int) {
        send(.setXiaoLED(red: red, green: green, blue: blue))
    }

    func ledAction(_ action: String) {
        send(.xiaoLEDAction(action))
    }

    func requestStatus() {
        send(.requestXiaoStatus)
    }

    func requestDistance() {
        send(.requestXiaoDistance)
    }

    func requestRadar() {
        send(.requestXiaoRadar)
    }

    private func send(_ command: RobotCommand) {
        do {
            try transport.send(command)
            errorMessage = nil
        } catch {
            errorMessage = error.localizedDescription
        }
    }
}

struct DialogMessage: Identifiable {
    enum Speaker {
        case user
        case assistant
    }

    let id = UUID()
    let speaker: Speaker
    let text: String
}

@MainActor
final class RobotDialogService: ObservableObject {
    static let languages = ["Русский", "English", "Deutsch", "Espanol", "中文"]

    @Published var draft = ""
    @Published var selectedLanguage = "Русский"
    @Published private(set) var messages: [DialogMessage] = []
    @Published private(set) var isAwaitingReply = false
    @Published var errorMessage: String?

    private let transport: BluetoothRobotTransport
    private var eventSubscription: AnyCancellable?
    private var incomingReply = ""

    init(transport: BluetoothRobotTransport) {
        self.transport = transport
        eventSubscription = transport.$lastEvent
            .compactMap { $0 }
            .sink { [weak self] event in
                guard let self else { return }
                if event.type == "dialog.reply", let reply = event.message, !reply.isEmpty {
                    self.incomingReply += reply
                    if event.status == "ready" {
                        self.messages.append(DialogMessage(speaker: .assistant, text: self.incomingReply))
                        self.incomingReply = ""
                        self.isAwaitingReply = false
                        self.errorMessage = nil
                    }
                } else if event.type == "dialog.error" {
                    self.incomingReply = ""
                    self.isAwaitingReply = false
                    self.errorMessage = event.message ?? "Не удалось получить ответ робота."
                }
            }
    }

    func send() {
        let message = String(draft.trimmingCharacters(in: .whitespacesAndNewlines).prefix(80))
        guard !message.isEmpty else {
            errorMessage = "Введите сообщение."
            return
        }

        do {
            try transport.send(.sendDialog(text: message, language: selectedLanguage))
            messages.append(DialogMessage(speaker: .user, text: message))
            draft = ""
            incomingReply = ""
            isAwaitingReply = true
            errorMessage = nil
        } catch {
            errorMessage = error.localizedDescription
        }
    }
}

@MainActor
final class WiFiProvisioningService: ObservableObject {
    enum NetworkSource: String, CaseIterable, Identifiable {
        case currentNetwork = "Сеть Wi-Fi"
        case personalHotspot = "Точка доступа iPhone"

        var id: Self { self }
    }

    @Published var source: NetworkSource = .currentNetwork
    @Published var ssid = ""
    @Published var password = ""
    @Published var errorMessage: String?
    @Published var didSubmit = false
    @Published private(set) var statusMessage: String?

    private let transport: BluetoothRobotTransport
    private var eventSubscription: AnyCancellable?

    init(transport: BluetoothRobotTransport) {
        self.transport = transport
        eventSubscription = transport.$lastEvent
            .compactMap { $0 }
            .sink { [weak self] event in
                guard let self else { return }
                if event.type == "wifi" {
                    self.statusMessage = event.status == "restarting"
                        ? "Настройки сохранены. Робот перезагружается и подключается к сети."
                        : (event.message ?? "Команда Wi-Fi выполнена.")
                    self.didSubmit = event.status == "restarting"
                    self.errorMessage = nil
                } else if event.type == "error", event.status == "invalid_wifi" {
                    self.errorMessage = "Робот отклонил SSID или пароль."
                    self.didSubmit = false
                }
            }
    }

    func configure() {
        let trimmedSSID = ssid.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !trimmedSSID.isEmpty else {
            errorMessage = "Введите имя Wi-Fi сети."
            return
        }
        do {
            try transport.send(.configureWiFi(ssid: trimmedSSID, password: password))
            didSubmit = false
            statusMessage = "Настройки отправлены, ожидаю подтверждение робота..."
            errorMessage = nil
        } catch {
            errorMessage = error.localizedDescription
        }
    }

    func resetConfiguration() {
        do {
            try transport.send(.resetWiFi)
            didSubmit = false
            statusMessage = "Команда сброса отправлена."
            errorMessage = nil
        } catch {
            errorMessage = error.localizedDescription
        }
    }
}
