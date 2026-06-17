import SwiftUI

struct ControlsView: View {
    @ObservedObject var controls: RobotControlService
    @State private var volume = 50.0
    @State private var speechText = ""
    @FocusState private var speechFieldFocused: Bool

    private let emotions = [
        ("neutral", "Обычно", "face.smiling"),
        ("happy", "Радость", "sun.max"),
        ("sad", "Грусть", "cloud.rain"),
        ("angry", "Злость", "bolt"),
        ("surprised", "Удивление", "sparkles"),
        ("sleepy", "Сон", "moon.zzz"),
        ("coffee", "Кофе", "cup.and.saucer.fill")
    ]

    private let scenes = [
        ("dance", "Танец", "music.note"),
        ("greet", "Приветствие", "hand.wave.fill"),
        ("curious", "Любопытство", "questionmark.bubble.fill"),
        ("love", "Любовь", "heart.fill"),
        ("celebrate", "Праздник", "party.popper.fill"),
        ("coffee", "Перерыв", "cup.and.saucer.fill"),
        ("sleep", "Сон", "moon.zzz.fill"),
        ("idle", "Спокойно", "pause.circle")
    ]

    var body: some View {
        NavigationStack {
            Form {
                Section("Эмоции") {
                    LazyVGrid(columns: [GridItem(.adaptive(minimum: 92))], spacing: 12) {
                        ForEach(emotions, id: \.0) { item in
                            Button {
                                controls.setEmotion(item.0)
                            } label: {
                                VStack(spacing: 8) {
                                    if item.0 == "coffee" {
                                        CoffeeSteamIcon()
                                    } else {
                                        Image(systemName: item.2).font(.title3)
                                    }
                                    Text(item.1).font(.caption)
                                }
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 10)
                            }
                            .buttonStyle(.bordered)
                        }
                    }
                }

                Section("Сценарии") {
                    Text("Сцена одновременно меняет мордочку, подсветку и движение головы.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                    LazyVGrid(columns: [GridItem(.adaptive(minimum: 100))], spacing: 12) {
                        ForEach(scenes, id: \.0) { scene in
                            Button {
                                controls.playScene(scene.0)
                            } label: {
                                VStack(spacing: 8) {
                                    Image(systemName: scene.2).font(.title3)
                                    Text(scene.1).font(.caption)
                                }
                                .frame(maxWidth: .infinity)
                                .padding(.vertical, 10)
                            }
                            .buttonStyle(.bordered)
                        }
                    }
                    if let sceneMessage = controls.sceneMessage {
                        Label(sceneMessage, systemImage: "sparkles")
                            .font(.footnote)
                            .foregroundStyle(.secondary)
                    }
                }

                Section("Мультимедиа") {
                    HStack {
                        mediaButton(.previous, "backward.fill")
                        Spacer()
                        mediaButton(.playPause, "playpause.fill")
                        Spacer()
                        mediaButton(.stop, "stop.fill")
                        Spacer()
                        mediaButton(.next, "forward.fill")
                    }
                    VStack(alignment: .leading) {
                        Text("Громкость: \(Int(volume))%")
                        Slider(value: $volume, in: 0...100, step: 1) { editing in
                            if !editing { controls.setVolume(Int(volume)) }
                        }
                    }
                }

                Section("Экран") {
                    VStack(alignment: .leading) {
                        Text("Яркость дисплея: \(Int(controls.screenBrightness))%")
                        Slider(value: $controls.screenBrightness, in: 5...100, step: 1) { editing in
                            if !editing { controls.setBrightness(Int(controls.screenBrightness)) }
                        }
                    }
                    Text("При сне робот временно снижает яркость до 10%, а после пробуждения возвращает выбранное значение.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }

                Section("Время") {
                    Text("Передать текущее время и часовой пояс iPhone на дисплей робота.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                    Button("Синхронизировать время") { controls.syncTime() }
                }

                Section("Сообщение голосом") {
                    TextField("Что робот должен прочитать", text: $speechText, axis: .vertical)
                        .lineLimit(2...4)
                        .focused($speechFieldFocused)
                    Text("До 100 символов. Текст будет отправлен роботу через защищённый BLE-канал.")
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                    Button {
                        controls.readText(speechText)
                        speechFieldFocused = false
                    } label: {
                        Label("Прочитать вслух", systemImage: "waveform.badge.mic")
                    }
                }

                if let error = controls.errorMessage {
                    Section {
                        Text(error).foregroundStyle(.red)
                    }
                }
            }
            .navigationTitle("Управление")
            .scrollDismissesKeyboard(.interactively)
            .toolbar {
                ToolbarItemGroup(placement: .keyboard) {
                    Spacer()
                    Button("Готово") {
                        speechFieldFocused = false
                    }
                }
            }
        }
    }

    private func mediaButton(_ action: MediaAction, _ icon: String) -> some View {
        Button { controls.media(action) } label: {
            Image(systemName: icon).font(.title3).frame(width: 40, height: 40)
        }
        .buttonStyle(.bordered)
    }
}

private struct CoffeeSteamIcon: View {
    var body: some View {
        TimelineView(.animation(minimumInterval: 0.7)) { timeline in
            let lifted = Int(timeline.date.timeIntervalSinceReferenceDate * 2).isMultiple(of: 2)
            ZStack {
                Image(systemName: "cup.and.saucer.fill")
                Text("~")
                    .font(.caption2.weight(.bold))
                    .offset(x: 2, y: lifted ? -12 : -9)
                    .opacity(lifted ? 0.35 : 0.9)
            }
            .font(.title3)
        }
    }
}
