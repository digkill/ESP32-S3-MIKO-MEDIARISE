#include "protocol.h"

#include <esp_log.h>

#define TAG "Protocol"

void Protocol::OnIncomingJson(std::function<void(const cJSON* root)> callback) {
    on_incoming_json_ = callback;
}

void Protocol::OnIncomingAudio(std::function<void(std::unique_ptr<AudioStreamPacket> packet)> callback) {
    on_incoming_audio_ = callback;
}

void Protocol::OnAudioChannelOpened(std::function<void()> callback) {
    on_audio_channel_opened_ = callback;
}

void Protocol::OnAudioChannelClosed(std::function<void()> callback) {
    on_audio_channel_closed_ = callback;
}

void Protocol::OnNetworkError(std::function<void(const std::string& message)> callback) {
    on_network_error_ = callback;
}

void Protocol::OnConnected(std::function<void()> callback) {
    on_connected_ = callback;
}

void Protocol::OnDisconnected(std::function<void()> callback) {
    on_disconnected_ = callback;
}

void Protocol::SetError(const std::string& message) {
    error_occurred_ = true;
    if (on_network_error_ != nullptr) {
        on_network_error_(message);
    }
}

void Protocol::SendAbortSpeaking(AbortReason reason) {
    std::string message = "{\"session_id\":\"" + session_id_ + "\",\"type\":\"abort\"";
    if (reason == kAbortReasonWakeWordDetected) {
        message += ",\"reason\":\"wake_word_detected\"";
    }
    message += "}";
    SendText(message);
}

void Protocol::SendWakeWordDetected(const std::string& wake_word) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "session_id", session_id_.c_str());
    cJSON_AddStringToObject(root, "type", "listen");
    cJSON_AddStringToObject(root, "state", "detect");
    cJSON_AddStringToObject(root, "text", wake_word.c_str());
    char* encoded = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (encoded == nullptr) {
        ESP_LOGE(TAG, "[PROTOCOL] Failed to encode detected text");
        return;
    }
    std::string json(encoded);
    cJSON_free(encoded);
    ESP_LOGI(TAG, "[PROTOCOL] Sending wake word detected: %s", wake_word.c_str());
    SendText(json);
}

void Protocol::SendTtsRequest(const std::string& text) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "session_id", session_id_.c_str());
    cJSON_AddStringToObject(root, "type", "tts");
    cJSON_AddStringToObject(root, "state", "start");
    cJSON_AddStringToObject(root, "text", text.c_str());
    char* encoded = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (encoded == nullptr) {
        ESP_LOGE(TAG, "[PROTOCOL] Failed to encode TTS request");
        return;
    }
    std::string json(encoded);
    cJSON_free(encoded);
    ESP_LOGI(TAG, "[PROTOCOL] Sending direct TTS request to server");
    SendText(json);
}

void Protocol::SendChatText(const std::string& text) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "session_id", session_id_.c_str());
    cJSON_AddStringToObject(root, "type", "listen");
    cJSON_AddStringToObject(root, "state", "start");
    cJSON_AddStringToObject(root, "mode", "manual");
    cJSON_AddStringToObject(root, "text", text.c_str());
    char* encoded = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (encoded == nullptr) {
        ESP_LOGE(TAG, "[PROTOCOL] Failed to encode chat message");
        return;
    }
    std::string json(encoded);
    cJSON_free(encoded);
    ESP_LOGI(TAG, "[PROTOCOL] Sending text dialog request to server");
    SendText(json);
}

void Protocol::SendCharacterEvent(const std::string& event, const std::string& context_json) {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "session_id", session_id_.c_str());
    cJSON_AddStringToObject(root, "type", "event");
    cJSON_AddStringToObject(root, "event", event.c_str());
    cJSON* context = cJSON_Parse(context_json.c_str());
    if (context == nullptr) {
        context = cJSON_CreateObject();
    }
    cJSON_AddItemToObject(root, "context", context);
    char* encoded = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (encoded == nullptr) {
        ESP_LOGE(TAG, "[PROTOCOL] Failed to encode character event");
        return;
    }
    std::string json(encoded);
    cJSON_free(encoded);
    ESP_LOGI(TAG, "[PROTOCOL] Sending character event: %s", event.c_str());
    SendText(json);
}

void Protocol::SendStartListening(ListeningMode mode) {
    std::string message = "{\"session_id\":\"" + session_id_ + "\"";
    message += ",\"type\":\"listen\",\"state\":\"start\"";
    const char* mode_str = "";
    if (mode == kListeningModeRealtime) {
        message += ",\"mode\":\"realtime\"";
        mode_str = "realtime";
    } else if (mode == kListeningModeAutoStop) {
        message += ",\"mode\":\"auto\"";
        mode_str = "auto";
    } else {
        message += ",\"mode\":\"manual\"";
        mode_str = "manual";
    }
    message += "}";
    ESP_LOGI(TAG, "[PROTOCOL] Sending start listening command, mode: %s", mode_str);
    SendText(message);
}

void Protocol::SendStopListening() {
    std::string message = "{\"session_id\":\"" + session_id_ + "\",\"type\":\"listen\",\"state\":\"stop\"}";
    ESP_LOGI(TAG, "[PROTOCOL] Sending stop listening command");
    SendText(message);
}

void Protocol::SendMcpMessage(const std::string& payload) {
    std::string message = "{\"session_id\":\"" + session_id_ + "\",\"type\":\"mcp\",\"payload\":" + payload + "}";
    SendText(message);
}

bool Protocol::IsTimeout() const {
    const int kTimeoutSeconds = 120;
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - last_incoming_time_);
    bool timeout = duration.count() > kTimeoutSeconds;
    if (timeout) {
        ESP_LOGE(TAG, "Channel timeout %ld seconds", (long)duration.count());
    }
    return timeout;
}
