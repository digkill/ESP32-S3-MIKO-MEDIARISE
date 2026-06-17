import SwiftUI
import UIKit

struct XiaoControlView: View {
    @ObservedObject var xiao: XiaoControlService
    @ObservedObject var transport: BluetoothRobotTransport
    @State private var selectedColor = Color(red: 0, green: 120 / 255, blue: 1)

    private let poses = [
        ("home", "Центр"),
        ("left", "Влево"),
        ("right", "Вправо"),
        ("up", "Вверх"),
        ("down", "Вниз"),
        ("nod", "Кивок"),
        ("shake", "Нет"),
        ("dance", "Танец")
    ]

    var body: some View {
        NavigationStack {
            Form {
                Section("ESP-NOW мост") {
                    Label(
                        transport.state.isReady ? "BLE связь с роботом активна" : "Подключитесь к HomeBot",
                        systemImage: transport.state.isReady ? "dot.radiowaves.left.and.right" : "antenna.radiowaves.left.and.right.slash"
                    )
                    .foregroundStyle(transport.state.isReady ? .primary : .secondary)
                    Button("Запросить статус XIAO") { xiao.requestStatus() }
                    Text(xiao.statusMessage)
                        .font(.footnote.monospaced())
                        .foregroundStyle(.secondary)
                }

                Section("Голова") {
                    angleSlider("Поворот", value: $xiao.yaw)
                    angleSlider("Наклон", value: $xiao.pitch)
                    Button("Передать положение") { xiao.sendServos() }
                        .buttonStyle(.borderedProminent)

                    LazyVGrid(columns: [GridItem(.adaptive(minimum: 74))], spacing: 8) {
                        ForEach(poses, id: \.0) { pose in
                            Button(pose.1) { xiao.setPose(pose.0) }
                                .buttonStyle(.bordered)
                        }
                    }
                }

                Section("Подсветка") {
                    ColorPicker("Цвет кольца", selection: $selectedColor, supportsOpacity: false)
                    Button("Установить выбранный цвет") { sendSelectedColor() }
                    HStack {
                        Button("Обычная") { xiao.ledAction("default") }
                        Button("Тест") { xiao.ledAction("test") }
                        Button("Выключить") { xiao.ledAction("off") }
                    }
                    .buttonStyle(.bordered)
                }

                Section("Датчики XIAO") {
                    Button("Расстояние") { xiao.requestDistance() }
                    Text(xiao.distanceMessage)
                        .font(.footnote.monospaced())
                        .foregroundStyle(.secondary)
                    Button("Радар присутствия") { xiao.requestRadar() }
                    Text(xiao.radarMessage)
                        .font(.footnote.monospaced())
                        .foregroundStyle(.secondary)
                }

                if let error = xiao.errorMessage {
                    Section {
                        Text(error).foregroundStyle(.red)
                    }
                }
            }
            .navigationTitle("XIAO")
            .disabled(!transport.state.isReady)
        }
    }

    private func angleSlider(_ title: String, value: Binding<Double>) -> some View {
        VStack(alignment: .leading) {
            Text("\(title): \(Int(value.wrappedValue)) degrees")
            Slider(value: value, in: 40...80, step: 1)
        }
    }

    private func sendSelectedColor() {
        let color = UIColor(selectedColor)
        var red: CGFloat = 0
        var green: CGFloat = 0
        var blue: CGFloat = 0
        var alpha: CGFloat = 0
        if color.getRed(&red, green: &green, blue: &blue, alpha: &alpha) {
            xiao.sendLED(
                red: Int((red * 255).rounded()),
                green: Int((green * 255).rounded()),
                blue: Int((blue * 255).rounded())
            )
        }
    }
}
