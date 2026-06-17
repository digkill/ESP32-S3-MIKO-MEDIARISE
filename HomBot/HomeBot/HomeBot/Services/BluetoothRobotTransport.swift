@preconcurrency import CoreBluetooth
import Combine
import Foundation

struct DiscoveredRobot: Identifiable {
    let id: UUID
    let name: String
    let rssi: Int
    fileprivate let peripheral: CBPeripheral
}

enum RobotConnectionState: Equatable {
    case unavailable(String)
    case idle
    case scanning
    case connecting(String)
    case discovering
    case ready(String)
    case failed(String)

    var title: String {
        switch self {
        case let .unavailable(reason): return reason
        case .idle: return "Не подключено"
        case .scanning: return "Поиск роботов..."
        case let .connecting(name): return "Подключение к \(name)..."
        case .discovering: return "Настройка канала..."
        case let .ready(name): return "Подключено: \(name)"
        case let .failed(message): return message
        }
    }

    var isReady: Bool {
        if case .ready = self { return true }
        return false
    }
}

@MainActor
final class BluetoothRobotTransport: NSObject, ObservableObject {
    @Published private(set) var devices: [DiscoveredRobot] = []
    @Published private(set) var state: RobotConnectionState = .idle
    @Published private(set) var lastEvent: RobotEvent?
    @Published private(set) var activity: [String] = []

    private var central: CBCentralManager!
    private var peripherals: [UUID: CBPeripheral] = [:]
    private var activePeripheral: CBPeripheral?
    private var commandCharacteristic: CBCharacteristic?

    private let serviceUUID = CBUUID(string: HomeBotBLEProfile.serviceUUID)
    private let commandUUID = CBUUID(string: HomeBotBLEProfile.commandUUID)
    private let eventUUID = CBUUID(string: HomeBotBLEProfile.eventUUID)

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: .main)
    }

    func startScan() {
        guard central.state == .poweredOn else {
            state = .unavailable("Bluetooth недоступен")
            return
        }
        devices.removeAll()
        peripherals.removeAll()
        state = .scanning
        log("Запущен поиск HomeBot")
        central.scanForPeripherals(withServices: [serviceUUID], options: [CBCentralManagerScanOptionAllowDuplicatesKey: false])
    }

    func stopScan() {
        central.stopScan()
        if case .scanning = state {
            state = .idle
        }
    }

    func connect(to robot: DiscoveredRobot) {
        stopScan()
        activePeripheral = robot.peripheral
        state = .connecting(robot.name)
        log("Подключение: \(robot.name)")
        central.connect(robot.peripheral)
    }

    func disconnect() {
        guard let activePeripheral else { return }
        central.cancelPeripheralConnection(activePeripheral)
    }

    func send(_ command: RobotCommand) throws {
        guard state.isReady, let activePeripheral, let commandCharacteristic else {
            throw TransportError.notConnected
        }
        let data = try command.encoded()
        activePeripheral.writeValue(data, for: commandCharacteristic, type: .withResponse)
        log("Команда отправлена")
    }

    private func log(_ value: String) {
        activity.insert(value, at: 0)
        activity = Array(activity.prefix(8))
    }

    enum TransportError: LocalizedError {
        case notConnected

        var errorDescription: String? {
            "Сначала подключитесь к HomeBot по Bluetooth."
        }
    }
}

extension BluetoothRobotTransport: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .poweredOn:
            state = .idle
        case .poweredOff:
            state = .unavailable("Bluetooth выключен")
        case .unauthorized:
            state = .unavailable("Нет доступа к Bluetooth")
        case .unsupported:
            state = .unavailable("Bluetooth LE не поддерживается")
        default:
            state = .unavailable("Bluetooth недоступен")
        }
    }

    func centralManager(
        _ central: CBCentralManager,
        didDiscover peripheral: CBPeripheral,
        advertisementData: [String: Any],
        rssi RSSI: NSNumber
    ) {
        let name = peripheral.name ?? advertisementData[CBAdvertisementDataLocalNameKey] as? String ?? "HomeBot"
        peripherals[peripheral.identifier] = peripheral
        let robot = DiscoveredRobot(id: peripheral.identifier, name: name, rssi: RSSI.intValue, peripheral: peripheral)
        devices.removeAll { $0.id == robot.id }
        devices.append(robot)
        devices.sort { $0.rssi > $1.rssi }
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        peripheral.delegate = self
        state = .discovering
        log("BLE соединение установлено")
        peripheral.discoverServices([serviceUUID])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        state = .failed(error?.localizedDescription ?? "Не удалось подключиться")
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        commandCharacteristic = nil
        activePeripheral = nil
        state = error.map { .failed($0.localizedDescription) } ?? .idle
        log("Робот отключен")
    }
}

extension BluetoothRobotTransport: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral, didDiscoverServices error: Error?) {
        if let error {
            state = .failed(error.localizedDescription)
            return
        }
        peripheral.services?.forEach {
            peripheral.discoverCharacteristics([commandUUID, eventUUID], for: $0)
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        if let error {
            state = .failed(error.localizedDescription)
            return
        }
        for characteristic in service.characteristics ?? [] {
            if characteristic.uuid == commandUUID {
                commandCharacteristic = characteristic
            } else if characteristic.uuid == eventUUID {
                peripheral.setNotifyValue(true, for: characteristic)
            }
        }
        if commandCharacteristic != nil {
            state = .ready(peripheral.name ?? "HomeBot")
            log("Канал управления готов")
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didUpdateValueFor characteristic: CBCharacteristic, error: Error?) {
        guard error == nil, characteristic.uuid == eventUUID, let data = characteristic.value else { return }
        if let event = try? JSONDecoder().decode(RobotEvent.self, from: data) {
            lastEvent = event
            log(event.message ?? "Событие: \(event.type)")
        }
    }

    func peripheral(_ peripheral: CBPeripheral, didWriteValueFor characteristic: CBCharacteristic, error: Error?) {
        if let error {
            state = .failed("Ошибка команды: \(error.localizedDescription)")
        }
    }
}
