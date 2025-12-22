#include "afe_audio_processor.h"
#include <esp_log.h>
#include <esp_bit_defs.h>

#define PROCESSOR_RUNNING BIT0
#define PROCESSOR_EXIT_REQUEST BIT1
#define PROCESSOR_TASK_EXITED BIT2

#define TAG "AfeAudioProcessor"
static constexpr TickType_t kProcessorWarmupDelayTicks = pdMS_TO_TICKS(200);

AfeAudioProcessor::AfeAudioProcessor()
    : afe_data_(nullptr) {
    event_group_ = xEventGroupCreate();
}

void AfeAudioProcessor::Initialize(AudioCodec* codec, int frame_duration_ms, srmodel_list_t* models_list) {
    codec_ = codec;
    frame_samples_ = frame_duration_ms * 16000 / 1000;

    // Pre-allocate output buffer capacity
    output_buffer_.reserve(frame_samples_);

    int ref_num = codec_->input_reference() ? 1 : 0;

    std::string input_format;
    for (int i = 0; i < codec_->input_channels() - ref_num; i++) {
        input_format.push_back('M');
    }
    for (int i = 0; i < ref_num; i++) {
        input_format.push_back('R');
    }

    srmodel_list_t *models;
    if (models_list == nullptr) {
        models = esp_srmodel_init("model");
    } else {
        models = models_list;
    }

    char* ns_model_name = esp_srmodel_filter(models, ESP_NSNET_PREFIX, NULL);
    char* vad_model_name = esp_srmodel_filter(models, ESP_VADN_PREFIX, NULL);
    
    afe_config_t* afe_config = afe_config_init(input_format.c_str(), NULL, AFE_TYPE_VC, AFE_MODE_HIGH_PERF);
    afe_config->aec_mode = AEC_MODE_VOIP_HIGH_PERF;
    afe_config->vad_mode = VAD_MODE_0;
    afe_config->vad_min_noise_ms = 100;
    if (vad_model_name != nullptr) {
        afe_config->vad_model_name = vad_model_name;
    }

    if (ns_model_name != nullptr) {
        afe_config->ns_init = true;
        afe_config->ns_model_name = ns_model_name;
        afe_config->afe_ns_mode = AFE_NS_MODE_NET;
    } else {
        afe_config->ns_init = false;
    }

    afe_config->agc_init = false;
    afe_config->memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;

#ifdef CONFIG_USE_DEVICE_AEC
    afe_config->aec_init = true;
    afe_config->vad_init = false;
#else
    afe_config->aec_init = false;
    afe_config->vad_init = true;
#endif

    afe_iface_ = esp_afe_handle_from_config(afe_config);
    afe_data_ = afe_iface_->create_from_config(afe_config);
    
    if (processor_task_handle_ != nullptr) {
        ESP_LOGW(TAG, "Audio processor task already running");
    } else {
        BaseType_t result = xTaskCreatePinnedToCore([](void* arg) {
            auto this_ = static_cast<AfeAudioProcessor*>(arg);
            this_->AudioProcessorTask();
            xEventGroupSetBits(this_->event_group_, PROCESSOR_TASK_EXITED);
            this_->processor_task_handle_ = nullptr;
            vTaskDelete(NULL);
        }, "audio_communication", 4096, this, 6, &processor_task_handle_, 1);  // Core 1: не мешаем main_event_loop и захвату аудио

        if (result != pdPASS) {
            ESP_LOGE(TAG, "Failed to create audio processor task");
            processor_task_handle_ = nullptr;
        }
    }
}

AfeAudioProcessor::~AfeAudioProcessor() {
    RequestTaskExit();
    if (afe_data_ != nullptr) {
        afe_iface_->destroy(afe_data_);
    }
    vEventGroupDelete(event_group_);
}

size_t AfeAudioProcessor::GetFeedSize() {
    if (afe_data_ == nullptr) {
        return 0;
    }
    return afe_iface_->get_feed_chunksize(afe_data_);
}

void AfeAudioProcessor::Feed(std::vector<int16_t>&& data) {
    if (afe_data_ == nullptr) {
        return;
    }
    feed_received_since_start_ = true;
    afe_iface_->feed(afe_data_, data.data());
}

void AfeAudioProcessor::Start() {
    warmup_pending_ = true;
    warmup_deadline_ticks_ = xTaskGetTickCount() + kProcessorWarmupDelayTicks;
    feed_received_since_start_ = false;
    xEventGroupClearBits(event_group_, PROCESSOR_EXIT_REQUEST | PROCESSOR_TASK_EXITED);
    xEventGroupSetBits(event_group_, PROCESSOR_RUNNING);
}

void AfeAudioProcessor::Stop() {
    xEventGroupClearBits(event_group_, PROCESSOR_RUNNING);
    if (afe_data_ != nullptr) {
        afe_iface_->reset_buffer(afe_data_);
    }
    output_buffer_.clear();
    output_buffer_.reserve(frame_samples_);
    warmup_pending_ = false;
    warmup_deadline_ticks_ = 0;
    feed_received_since_start_ = false;
}

bool AfeAudioProcessor::IsRunning() {
    return xEventGroupGetBits(event_group_) & PROCESSOR_RUNNING;
}

void AfeAudioProcessor::OnOutput(std::function<void(std::vector<int16_t>&& data)> callback) {
    output_callback_ = callback;
}

void AfeAudioProcessor::OnVadStateChange(std::function<void(bool speaking)> callback) {
    vad_state_change_callback_ = callback;
}

void AfeAudioProcessor::AudioProcessorTask() {
    auto fetch_size = afe_iface_->get_fetch_chunksize(afe_data_);
    auto feed_size = afe_iface_->get_feed_chunksize(afe_data_);
    ESP_LOGI(TAG, "Audio communication task started, feed size: %d fetch size: %d",
        feed_size, fetch_size);

    while (true) {
        // Используем таймаут вместо portMAX_DELAY, чтобы периодически сбрасывать watchdog
        auto bits = xEventGroupWaitBits(event_group_,
            PROCESSOR_RUNNING | PROCESSOR_EXIT_REQUEST,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(1000));

        // Если событие не получено, продолжаем цикл (сбросим watchdog)
        if (bits & PROCESSOR_EXIT_REQUEST) {
            break;
        }

        if (!(bits & PROCESSOR_RUNNING)) {
            continue;
        }

        if (warmup_pending_) {
            TickType_t now = xTaskGetTickCount();
            if (warmup_deadline_ticks_ > now) {
                vTaskDelay(warmup_deadline_ticks_ - now);
                continue;
            }
            warmup_pending_ = false;
        }

        if (!feed_received_since_start_) {
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        // Используем блокирующий fetch (таймаут внутри AFE ~2000ms),
        // чтобы не спамить логами/не крутиться в холостую между порциями feed().
        auto res = afe_iface_->fetch(afe_data_);

        if (xEventGroupGetBits(event_group_) & PROCESSOR_EXIT_REQUEST) {
            break;
        }

        if ((xEventGroupGetBits(event_group_) & PROCESSOR_RUNNING) == 0) {
            continue;
        }
        if (res == nullptr || res->ret_value == ESP_FAIL) {
            if (res != nullptr) {
                // Логируем только периодически, чтобы не засорять лог
                static int error_count = 0;
                if (++error_count % 100 == 0) {
                    ESP_LOGW(TAG, "AFE fetch error code: %d (count: %d)", res->ret_value, error_count);
                }
            }
            vTaskDelay(pdMS_TO_TICKS(5));
            continue;
        }

        // VAD state change
        if (vad_state_change_callback_) {
            if (res->vad_state == VAD_SPEECH && !is_speaking_) {
                is_speaking_ = true;
                vad_state_change_callback_(true);
            } else if (res->vad_state == VAD_SILENCE && is_speaking_) {
                is_speaking_ = false;
                vad_state_change_callback_(false);
            }
        }

        if (output_callback_) {
            size_t samples = res->data_size / sizeof(int16_t);
            
            // Add data to buffer
            output_buffer_.insert(output_buffer_.end(), res->data, res->data + samples);
            
            // Output complete frames when buffer has enough data
            while (output_buffer_.size() >= frame_samples_) {
                if (output_buffer_.size() == frame_samples_) {
                    // If buffer size equals frame size, move the entire buffer
                    output_callback_(std::move(output_buffer_));
                    output_buffer_.clear();
                    output_buffer_.reserve(frame_samples_);
                } else {
                    // If buffer size exceeds frame size, copy one frame and remove it
                    output_callback_(std::vector<int16_t>(output_buffer_.begin(), output_buffer_.begin() + frame_samples_));
                    output_buffer_.erase(output_buffer_.begin(), output_buffer_.begin() + frame_samples_);
                }
            }
        }
    }

    xEventGroupClearBits(event_group_, PROCESSOR_RUNNING);
}

void AfeAudioProcessor::EnableDeviceAec(bool enable) {
    if (enable) {
#if CONFIG_USE_DEVICE_AEC
        afe_iface_->disable_vad(afe_data_);
        afe_iface_->enable_aec(afe_data_);
#else
        ESP_LOGE(TAG, "Device AEC is not supported");
#endif
    } else {
        afe_iface_->disable_aec(afe_data_);
        afe_iface_->enable_vad(afe_data_);
    }
}

void AfeAudioProcessor::RequestTaskExit() {
    if (processor_task_handle_ == nullptr) {
        return;
    }

    xEventGroupSetBits(event_group_, PROCESSOR_EXIT_REQUEST);

    const TickType_t wait_ticks = pdMS_TO_TICKS(50);
    constexpr int max_waits = 40;  // ~2 seconds
    for (int i = 0; i < max_waits; ++i) {
        if (xEventGroupWaitBits(event_group_, PROCESSOR_TASK_EXITED, pdTRUE, pdFALSE, wait_ticks) & PROCESSOR_TASK_EXITED) {
            break;
        }
    }

    if (processor_task_handle_ != nullptr) {
        vTaskDelete(processor_task_handle_);
        processor_task_handle_ = nullptr;
    }

    xEventGroupClearBits(event_group_, PROCESSOR_EXIT_REQUEST);
}
