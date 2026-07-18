#include "audio_codec.h"
#include "board.h"
#include "settings.h"
#include "config.h"

#include <esp_log.h>
#include <cstring>
#include <driver/i2s_common.h>

#define TAG "AudioCodec"

#ifndef AUDIO_DEFAULT_OUTPUT_VOLUME
#define AUDIO_DEFAULT_OUTPUT_VOLUME 70
#endif
// Volumes above this drive the DAC/PA into clipping on small speakers.
#ifndef AUDIO_MAX_OUTPUT_VOLUME
#define AUDIO_MAX_OUTPUT_VOLUME 100
#endif

AudioCodec::AudioCodec() {
}

AudioCodec::~AudioCodec() {
}

void AudioCodec::OutputData(std::vector<int16_t>& data) {
    Write(data.data(), data.size());
}

bool AudioCodec::InputData(std::vector<int16_t>& data) {
    int samples = Read(data.data(), data.size());
    if (samples > 0) {
        return true;
    }
    return false;
}

void AudioCodec::Start() {
    {
        Settings settings("audio", false);
        output_volume_ = settings.GetInt("output_volume", AUDIO_DEFAULT_OUTPUT_VOLUME);
    }
    if (output_volume_ <= 0) {
        ESP_LOGW(TAG, "Output volume value (%d) is invalid, setting to default (%d)", output_volume_, AUDIO_DEFAULT_OUTPUT_VOLUME);
        output_volume_ = AUDIO_DEFAULT_OUTPUT_VOLUME;
    }
    if (output_volume_ > AUDIO_MAX_OUTPUT_VOLUME) {
        ESP_LOGW(TAG, "Clamping stored output volume %d to safe maximum %d", output_volume_, AUDIO_MAX_OUTPUT_VOLUME);
        output_volume_ = AUDIO_MAX_OUTPUT_VOLUME;
        Settings writer("audio", true);
        writer.SetInt("output_volume", output_volume_);
    }
#ifdef AUDIO_FORCE_MAX_OUTPUT_VOLUME
    if (output_volume_ < AUDIO_MAX_OUTPUT_VOLUME) {
        ESP_LOGW(TAG, "Forcing output volume %d up to maximum %d", output_volume_, AUDIO_MAX_OUTPUT_VOLUME);
        output_volume_ = AUDIO_MAX_OUTPUT_VOLUME;
        Settings writer("audio", true);
        writer.SetInt("output_volume", output_volume_);
    }
#endif

    if (tx_handle_ != nullptr) {
        ESP_LOGI(TAG, "Enabling TX channel (speaker)");
        ESP_ERROR_CHECK(i2s_channel_enable(tx_handle_));
    } else {
        ESP_LOGW(TAG, "TX handle is nullptr, speaker channel not enabled");
    }

    if (rx_handle_ != nullptr) {
        ESP_LOGI(TAG, "Enabling RX channel (microphone)");
        ESP_ERROR_CHECK(i2s_channel_enable(rx_handle_));
    } else {
        ESP_LOGW(TAG, "RX handle is nullptr, microphone channel not enabled");
    }

    EnableInput(true);
    EnableOutput(true);
    ESP_LOGI(TAG, "Audio codec started - Input: %s, Output: %s", input_enabled_ ? "enabled" : "disabled", output_enabled_ ? "enabled" : "disabled");
}

void AudioCodec::SetOutputVolume(int volume) {
    if (volume > AUDIO_MAX_OUTPUT_VOLUME) {
        ESP_LOGW(TAG, "Requested volume %d exceeds safe maximum, clamping to %d", volume, AUDIO_MAX_OUTPUT_VOLUME);
        volume = AUDIO_MAX_OUTPUT_VOLUME;
    }
    if (volume < 0) {
        volume = 0;
    }
    output_volume_ = volume;
    ESP_LOGI(TAG, "Set output volume to %d", output_volume_);
    
    Settings settings("audio", true);
    settings.SetInt("output_volume", output_volume_);
}

void AudioCodec::SetInputGain(float gain) {
    input_gain_ = gain;
    ESP_LOGI(TAG, "Set input gain to %.1f", input_gain_);
}

void AudioCodec::EnableInput(bool enable) {
    if (enable == input_enabled_) {
        return;
    }
    input_enabled_ = enable;
    ESP_LOGI(TAG, "Set input enable to %s", enable ? "true" : "false");
}

void AudioCodec::EnableOutput(bool enable) {
    if (enable == output_enabled_) {
        return;
    }
    output_enabled_ = enable;
    ESP_LOGI(TAG, "Set output enable to %s", enable ? "true" : "false");
}
