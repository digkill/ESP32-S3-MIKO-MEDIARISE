import SwiftUI

struct ConnectionView: View {
    @ObservedObject var transport: BluetoothRobotTransport

    var body: some View {
        NavigationStack {
            List {
                Section {
                    Label(transport.state.title, systemImage: transport.state.isReady ? "checkmark.circle.fill" : "dot.radiowaves.left.and.right")
                        .foregroundStyle(transport.state.isReady ? .green : .primary)
                    if transport.state.isReady {
                        Button("Отключить", role: .destructive) { transport.disconnect() }
                    } else {
                        Button(transport.state == .scanning ? "Остановить поиск" : "Найти HomeBot") {
                            transport.state == .scanning ? transport.stopScan() : transport.startScan()
                        }
                    }
                } header: {
                    Text("Bluetooth LE")
                }

                Section("Найденные устройства") {
                    if transport.devices.isEmpty {
                        Text("Устройств пока нет").foregroundStyle(.secondary)
                    }
                    ForEach(transport.devices) { robot in
                        Button {
                            transport.connect(to: robot)
                        } label: {
                            HStack {
                                VStack(alignment: .leading) {
                                    Text(robot.name)
                                    Text(robot.id.uuidString).font(.caption2).foregroundStyle(.secondary)
                                }
                                Spacer()
                                Text("\(robot.rssi) dBm").font(.caption).foregroundStyle(.secondary)
                            }
                        }
                    }
                }

                Section("Журнал") {
                    ForEach(transport.activity, id: \.self) { item in
                        Text(item).font(.footnote)
                    }
                }
            }
            .navigationTitle("Подключение")
        }
    }
}
