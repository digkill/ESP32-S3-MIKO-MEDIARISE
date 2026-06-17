import SwiftUI

struct WiFiSetupView: View {
    @ObservedObject var provisioning: WiFiProvisioningService
    @ObservedObject var transport: BluetoothRobotTransport
    @FocusState private var focusedField: Field?

    private enum Field {
        case ssid
        case password
    }

    private var connected: Bool {
        transport.state.isReady
    }

    var body: some View {
        NavigationStack {
            Form {
                Section("Источник интернета") {
                    Picker("Подключить робота к", selection: $provisioning.source) {
                        ForEach(WiFiProvisioningService.NetworkSource.allCases) { source in
                            Text(source.rawValue).tag(source)
                        }
                    }
                    .pickerStyle(.segmented)
                }

                if provisioning.source == .personalHotspot {
                    Section("Точка доступа iPhone") {
                        Text("1. Откройте «Настройки» -> «Режим модема» и включите «Разрешать другим».")
                        Text("2. Включите «Максимальная совместимость», чтобы ESP32 подключался по 2.4 GHz.")
                        Text("3. Введите ниже имя iPhone как SSID и пароль Wi-Fi из настроек режима модема.")
                        Text("Точка доступа iPhone передает роботу мобильный интернет, а не подключение к текущей Wi-Fi сети.")
                            .foregroundStyle(.secondary)
                    }
                    .font(.footnote)
                } else {
                    Section {
                        Text("Введите данные роутера, к которому должен подключиться робот. Если iPhone уже в этой сети, используйте тот же SSID и пароль.")
                        Text("iOS не передает приложению сохраненный пароль Wi-Fi автоматически, поэтому данные сети вводятся вручную.")
                            .foregroundStyle(.secondary)
                    }
                    .font(.footnote)
                }

                Section("Настройка сети") {
                    TextField(
                        provisioning.source == .personalHotspot ? "Имя точки доступа iPhone (SSID)" : "Название сети (SSID)",
                        text: $provisioning.ssid
                    )
                        .textInputAutocapitalization(.never)
                        .autocorrectionDisabled()
                        .focused($focusedField, equals: .ssid)
                    SecureField(
                        provisioning.source == .personalHotspot ? "Пароль режима модема" : "Пароль Wi-Fi",
                        text: $provisioning.password
                    )
                        .focused($focusedField, equals: .password)
                    Button(
                        provisioning.source == .personalHotspot
                            ? "Подключить робота к iPhone"
                            : "Подключить робота к сети"
                    ) {
                        provisioning.configure()
                        focusedField = nil
                    }
                        .disabled(!connected)
                }

                Section {
                    Text(connected
                         ? "Данные отправляются через защищенное BLE-соединение. После сохранения новой сети робот перезагрузится."
                         : "Для настройки Wi-Fi сначала подключитесь к роботу на вкладке «Связь».")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }

                if let status = provisioning.statusMessage {
                    Section {
                        Label(status, systemImage: provisioning.didSubmit ? "checkmark.circle.fill" : "antenna.radiowaves.left.and.right")
                            .foregroundStyle(provisioning.didSubmit ? .green : .secondary)
                    }
                }

                if let error = provisioning.errorMessage {
                    Section {
                        Text(error).foregroundStyle(.red)
                    }
                }

                Section("Сброс") {
                    Button("Перевести робот в режим новой настройки", role: .destructive) {
                        provisioning.resetConfiguration()
                    }
                    .disabled(!connected)
                }
            }
            .navigationTitle("Wi-Fi")
            .scrollDismissesKeyboard(.interactively)
            .toolbar {
                ToolbarItemGroup(placement: .keyboard) {
                    Spacer()
                    Button("Готово") {
                        focusedField = nil
                    }
                }
            }
        }
    }
}
