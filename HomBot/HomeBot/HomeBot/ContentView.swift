import SwiftUI

struct ContentView: View {
    @EnvironmentObject private var session: HomeBotSession

    var body: some View {
        TabView {
            DashboardView(transport: session.transport, controls: session.controls)
                .tabItem { Label("Главная", systemImage: "house") }
            ConnectionView(transport: session.transport)
                .tabItem { Label("Связь", systemImage: "dot.radiowaves.left.and.right") }
            DialogView(dialog: session.dialog, transport: session.transport)
                .tabItem { Label("Диалог", systemImage: "bubble.left.and.bubble.right") }
            ControlsView(controls: session.controls)
                .tabItem { Label("Пульт", systemImage: "slider.horizontal.3") }
            XiaoControlView(xiao: session.xiao, transport: session.transport)
                .tabItem { Label("XIAO", systemImage: "cpu") }
            WiFiSetupView(provisioning: session.provisioning, transport: session.transport)
                .tabItem { Label("Wi-Fi", systemImage: "wifi") }
        }
        .tint(.cyan)
    }
}

private struct DashboardView: View {
    @ObservedObject var transport: BluetoothRobotTransport
    let controls: RobotControlService

    var body: some View {
        NavigationStack {
            ScrollView {
                VStack(alignment: .leading, spacing: 18) {
                    statusCard
                    if let event = transport.lastEvent {
                        GroupBox("Последнее событие") {
                            VStack(alignment: .leading, spacing: 6) {
                                Text(event.message ?? event.type)
                                if let ssid = event.wifiSSID {
                                    Text("Wi-Fi: \(ssid)").foregroundStyle(.secondary)
                                }
                                if let battery = event.batteryPercent {
                                    Text("Аккумулятор: \(battery)%").foregroundStyle(.secondary)
                                }
                            }
                            .frame(maxWidth: .infinity, alignment: .leading)
                        }
                    }
                    GroupBox("Быстрые действия") {
                        HStack {
                            Button("Улыбка") { controls.setEmotion("happy") }
                            Spacer()
                            Button("Синхр. время") { controls.syncTime() }
                        }
                        .buttonStyle(.bordered)
                    }
                }
                .padding()
            }
            .navigationTitle("HomeBot")
        }
    }

    private var statusCard: some View {
        HStack(spacing: 14) {
            Image(systemName: transport.state.isReady ? "checkmark.circle.fill" : "antenna.radiowaves.left.and.right.slash")
                .font(.title)
                .foregroundStyle(transport.state.isReady ? .green : .secondary)
            VStack(alignment: .leading) {
                Text("ESP32-S3").font(.headline)
                Text(transport.state.title).font(.subheadline).foregroundStyle(.secondary)
            }
            Spacer()
        }
        .padding()
        .background(.thinMaterial, in: RoundedRectangle(cornerRadius: 18))
    }
}

#Preview {
    ContentView()
        .environmentObject(HomeBotSession())
}
