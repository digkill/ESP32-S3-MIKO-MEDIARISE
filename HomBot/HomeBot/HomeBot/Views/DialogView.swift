import SwiftUI

struct DialogView: View {
    @ObservedObject var dialog: RobotDialogService
    @ObservedObject var transport: BluetoothRobotTransport
    @FocusState private var composerFocused: Bool

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                settingsBar
                Divider()
                messageList
                Divider()
                composer
            }
            .navigationTitle("Диалог")
            .toolbar {
                ToolbarItemGroup(placement: .keyboard) {
                    Spacer()
                    Button("Готово") {
                        composerFocused = false
                    }
                }
            }
        }
    }

    private var settingsBar: some View {
        HStack {
            Label("Язык ответа", systemImage: "globe")
                .font(.subheadline)
                .foregroundStyle(.secondary)
            Spacer()
            Picker("Язык", selection: $dialog.selectedLanguage) {
                ForEach(RobotDialogService.languages, id: \.self) { language in
                    Text(language).tag(language)
                }
            }
            .pickerStyle(.menu)
        }
        .padding(.horizontal)
        .padding(.vertical, 10)
    }

    private var messageList: some View {
        ScrollViewReader { proxy in
            ScrollView {
                if dialog.messages.isEmpty {
                    ContentUnavailableView(
                        "Начните диалог",
                        systemImage: "bubble.left.and.bubble.right",
                        description: Text("Введите текст. Робот ответит голосом через сервер.")
                    )
                    .padding(.top, 70)
                } else {
                    LazyVStack(spacing: 12) {
                        ForEach(dialog.messages) { message in
                            DialogBubble(message: message)
                                .id(message.id)
                        }
                        if dialog.isAwaitingReply {
                            HStack {
                                ProgressView()
                                Text("Робот формирует ответ...")
                                    .font(.footnote)
                                    .foregroundStyle(.secondary)
                                Spacer()
                            }
                            .padding(.horizontal)
                        }
                    }
                    .padding()
                }
            }
            .onTapGesture {
                composerFocused = false
            }
            .onChange(of: dialog.messages.count) {
                guard let last = dialog.messages.last else { return }
                withAnimation {
                    proxy.scrollTo(last.id, anchor: .bottom)
                }
            }
        }
    }

    private var composer: some View {
        VStack(alignment: .leading, spacing: 8) {
            if let error = dialog.errorMessage {
                Text(error)
                    .font(.footnote)
                    .foregroundStyle(.red)
            }
            HStack(alignment: .bottom, spacing: 10) {
                TextField("Сообщение роботу", text: $dialog.draft, axis: .vertical)
                    .lineLimit(1...4)
                    .textFieldStyle(.roundedBorder)
                    .focused($composerFocused)
                    .submitLabel(.send)
                    .onSubmit { sendMessage() }
                Button {
                    sendMessage()
                } label: {
                    Image(systemName: "arrow.up.circle.fill")
                        .font(.title)
                }
                .disabled(
                    !transport.state.isReady ||
                    dialog.isAwaitingReply ||
                    dialog.draft.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
                )
            }
            if !transport.state.isReady {
                Text("Подключите робота во вкладке «Связь».")
                    .font(.caption)
                    .foregroundStyle(.secondary)
            }
        }
        .padding()
    }

    private func sendMessage() {
        dialog.send()
        composerFocused = false
    }
}

private struct DialogBubble: View {
    let message: DialogMessage

    var body: some View {
        HStack {
            if message.speaker == .user {
                Spacer(minLength: 38)
            }
            Text(message.text)
                .padding(.horizontal, 12)
                .padding(.vertical, 9)
                .foregroundStyle(message.speaker == .user ? .white : .primary)
                .background(
                    message.speaker == .user ? Color.cyan : Color.secondary.opacity(0.16),
                    in: RoundedRectangle(cornerRadius: 16)
                )
            if message.speaker == .assistant {
                Spacer(minLength: 38)
            }
        }
    }
}
