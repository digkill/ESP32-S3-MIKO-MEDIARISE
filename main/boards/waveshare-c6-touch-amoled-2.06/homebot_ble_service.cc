#include "homebot_ble_service.h"

#include <algorithm>
#include <cstring>
#include <string>
#include <sys/time.h>

#include <cJSON.h>
#include <esp_log.h>
#include <esp_system.h>
#include <host/ble_gap.h>
#include <host/ble_gatt.h>
#include <host/ble_hs.h>
#include <host/ble_sm.h>
#include <host/ble_uuid.h>
#include <host/util/util.h>
#include <nimble/nimble_port.h>
#include <nimble/nimble_port_freertos.h>
#include <os/os_mbuf.h>
#include <services/gap/ble_svc_gap.h>
#include <services/gatt/ble_svc_gatt.h>
#include <ssid_manager.h>

#include "application.h"
#include "audio/audio_codec.h"
#include "boards/common/board.h"
#include "display/display.h"
#include "servo_controller.h"
#include "settings.h"

static const char* TAG = "HomeBotBLE";
#ifndef HOMEBOT_BLE_DEVICE_NAME
#define HOMEBOT_BLE_DEVICE_NAME "HomeBot-C6"
#endif
static constexpr char kDeviceName[] = HOMEBOT_BLE_DEVICE_NAME;

// UUID byte order is reversed for BLE_UUID128_INIT.
static const ble_uuid128_t kServiceUuid =
    BLE_UUID128_INIT(0x01, 0xc0, 0xb9, 0xc6, 0x60, 0x8b, 0x4c, 0x8f,
                     0x41, 0x4a, 0x0e, 0x18, 0x01, 0x00, 0x10, 0x7c);
static const ble_uuid128_t kCommandUuid =
    BLE_UUID128_INIT(0x01, 0xc0, 0xb9, 0xc6, 0x60, 0x8b, 0x4c, 0x8f,
                     0x41, 0x4a, 0x0e, 0x18, 0x02, 0x00, 0x10, 0x7c);
static const ble_uuid128_t kEventUuid =
    BLE_UUID128_INIT(0x01, 0xc0, 0xb9, 0xc6, 0x60, 0x8b, 0x4c, 0x8f,
                     0x41, 0x4a, 0x0e, 0x18, 0x03, 0x00, 0x10, 0x7c);

extern "C" void ble_store_config_init(void);

HomeBotBleService* HomeBotBleService::instance_ = nullptr;
static uint16_t g_event_value_handle = 0;

HomeBotBleService::HomeBotBleService() {
    instance_ = this;
}

void HomeBotBleService::SetXiaoController(ServoController* controller) {
    xiao_controller_ = controller;
}

static const ble_gatt_chr_def kCharacteristics[] = {
    {
        .uuid = &kCommandUuid.u,
        .access_cb = HomeBotBleService::CommandAccess,
        .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_ENC,
    },
    {
        .uuid = &kEventUuid.u,
        .access_cb = HomeBotBleService::EventAccess,
        .flags = BLE_GATT_CHR_F_NOTIFY,
        .val_handle = &g_event_value_handle,
    },
    {0},
};

static const ble_gatt_svc_def kServices[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &kServiceUuid.u,
        .characteristics = kCharacteristics,
    },
    {0},
};

void HomeBotBleService::Start() {
    Application::GetInstance().SetDialogReplyCallback([this](const std::string& reply) {
        SendDialogReply(reply);
    });
    Application::GetInstance().SetDialogErrorCallback([this](const std::string& error) {
        SendEvent("dialog.error", error, "server_error");
    });

    esp_err_t result = nimble_port_init();
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NimBLE: %s", esp_err_to_name(result));
        return;
    }

    ble_hs_cfg.sync_cb = OnHostSync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1;
    ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set(kDeviceName);

    int rc = ble_gatts_count_cfg(kServices);
    if (rc == 0) {
        rc = ble_gatts_add_svcs(kServices);
    }
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to register HomeBot GATT service: %d", rc);
        return;
    }

    ble_store_config_init();
    nimble_port_freertos_init(HostTask);
    ESP_LOGI(TAG, "HomeBot BLE service starting");
}

void HomeBotBleService::HostTask(void* parameter) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void HomeBotBleService::OnHostSync() {
    if (instance_ == nullptr) {
        return;
    }
    if (ble_hs_util_ensure_addr(0) != 0 ||
        ble_hs_id_infer_auto(0, &instance_->address_type_) != 0) {
        ESP_LOGE(TAG, "Failed to determine BLE address");
        return;
    }
    instance_->Advertise();
}

void HomeBotBleService::Advertise() {
    ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = const_cast<ble_uuid128_t*>(&kServiceUuid);
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set BLE advertising fields: %d", rc);
        return;
    }

    ble_hs_adv_fields response_fields = {};
    response_fields.name = reinterpret_cast<const uint8_t*>(kDeviceName);
    response_fields.name_len = sizeof(kDeviceName) - 1;
    response_fields.name_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&response_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to set BLE scan response fields: %d", rc);
        return;
    }

    ble_gap_adv_params parameters = {};
    parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
    parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(address_type_, nullptr, BLE_HS_FOREVER, &parameters, GapEvent, this);
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to advertise BLE service: %d", rc);
    }
}

int HomeBotBleService::GapEvent(ble_gap_event* event, void* arg) {
    auto* service = static_cast<HomeBotBleService*>(arg);
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            service->connection_handle_ = event->connect.conn_handle;
            ESP_LOGI(TAG, "iPhone connected over BLE");
            ble_gap_security_initiate(service->connection_handle_);
        } else {
            service->Advertise();
        }
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        service->connection_handle_ = BLE_HS_CONN_HANDLE_NONE;
        ESP_LOGI(TAG, "BLE disconnected; advertising again");
        service->Advertise();
        return 0;
    case BLE_GAP_EVENT_ENC_CHANGE:
        if (event->enc_change.status == 0) {
            service->SendEvent("status", "Secure BLE channel ready", "ready");
        }
        return 0;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        service->Advertise();
        return 0;
    default:
        return 0;
    }
}

int HomeBotBleService::CommandAccess(
    uint16_t connection_handle,
    uint16_t attribute_handle,
    ble_gatt_access_ctxt* context,
    void* arg) {
    if (context->op != BLE_GATT_ACCESS_OP_WRITE_CHR || instance_ == nullptr) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    const uint16_t length = OS_MBUF_PKTLEN(context->om);
    if (length == 0 || length > 512) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    std::string command(length, '\0');
    uint16_t copied = 0;
    int rc = ble_hs_mbuf_to_flat(context->om, command.data(), length, &copied);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    command.resize(copied);
    instance_->HandleCommand(command);
    return 0;
}

int HomeBotBleService::EventAccess(
    uint16_t connection_handle,
    uint16_t attribute_handle,
    ble_gatt_access_ctxt* context,
    void* arg) {
    return BLE_ATT_ERR_READ_NOT_PERMITTED;
}

void HomeBotBleService::SendEvent(
    const std::string& type,
    const std::string& message,
    const std::string& status) {
    if (connection_handle_ == BLE_HS_CONN_HANDLE_NONE || g_event_value_handle == 0) {
        return;
    }

    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", type.c_str());
    cJSON_AddStringToObject(root, "message", message.c_str());
    cJSON_AddStringToObject(root, "status", status.c_str());
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == nullptr) {
        return;
    }

    os_mbuf* packet = ble_hs_mbuf_from_flat(json, std::strlen(json));
    cJSON_free(json);
    if (packet != nullptr) {
        int rc = ble_gatts_notify_custom(connection_handle_, g_event_value_handle, packet);
        if (rc != 0) {
            ESP_LOGW(TAG, "Failed to notify event: %d", rc);
        }
    }
}

void HomeBotBleService::SendDialogReply(const std::string& reply) {
    constexpr size_t kMaxChunkBytes = 48;
    size_t offset = 0;
    do {
        size_t length = std::min(kMaxChunkBytes, reply.size() - offset);
        while (length > 0 && offset + length < reply.size() &&
               (static_cast<unsigned char>(reply[offset + length]) & 0xc0) == 0x80) {
            --length;
        }
        if (length == 0) {
            length = std::min(kMaxChunkBytes, reply.size() - offset);
        }
        const bool final_chunk = offset + length >= reply.size();
        SendEvent("dialog.reply", reply.substr(offset, length), final_chunk ? "ready" : "continue");
        offset += length;
    } while (offset < reply.size());
}

void HomeBotBleService::HandleCommand(const std::string& json) {
    cJSON* root = cJSON_ParseWithLength(json.c_str(), json.size());
    if (root == nullptr) {
        SendEvent("error", "Invalid JSON command", "invalid_json");
        return;
    }

    cJSON* type = cJSON_GetObjectItem(root, "type");
    cJSON* payload = cJSON_GetObjectItem(root, "payload");
    if (!cJSON_IsString(type) || !cJSON_IsObject(payload)) {
        cJSON_Delete(root);
        SendEvent("error", "Command type or payload is missing", "invalid_command");
        return;
    }

    std::string command_type = type->valuestring;
    Application::GetInstance().Schedule([]() {
        Board::GetInstance().SetPowerSaveMode(false);
    });
    if (command_type == "wifi.configure") {
        cJSON* ssid = cJSON_GetObjectItem(payload, "ssid");
        cJSON* password = cJSON_GetObjectItem(payload, "password");
        if (!cJSON_IsString(ssid) || !cJSON_IsString(password) || std::strlen(ssid->valuestring) == 0) {
            SendEvent("error", "SSID or password is invalid", "invalid_wifi");
        } else {
            SsidManager::GetInstance().AddSsid(ssid->valuestring, password->valuestring);
            SendEvent("wifi", "Wi-Fi settings stored; restarting", "restarting");
            Application::GetInstance().Schedule([]() {
                vTaskDelay(pdMS_TO_TICKS(800));
                esp_restart();
            });
        }
    } else if (command_type == "wifi.reset") {
        SsidManager::GetInstance().Clear();
        SendEvent("wifi", "Saved Wi-Fi networks cleared", "ready");
    } else if (command_type == "time.set") {
        cJSON* timestamp = cJSON_GetObjectItem(payload, "unixMilliseconds");
        cJSON* timezone = cJSON_GetObjectItem(payload, "timeZone");
        if (!cJSON_IsNumber(timestamp)) {
            SendEvent("error", "Timestamp is invalid", "invalid_time");
        } else {
            struct timeval value = {
                .tv_sec = static_cast<time_t>(timestamp->valuedouble / 1000.0),
                .tv_usec = 0,
            };
            settimeofday(&value, nullptr);
            if (cJSON_IsString(timezone)) {
                Settings settings("homebot", true);
                settings.SetString("timezone", timezone->valuestring);
            }
            SendEvent("time", "Clock synchronized", "ready");
        }
    } else if (command_type == "emotion.set") {
        cJSON* name = cJSON_GetObjectItem(payload, "name");
        if (!cJSON_IsString(name)) {
            SendEvent("error", "Emotion is invalid", "invalid_emotion");
        } else {
            std::string emotion = name->valuestring;
            Application::GetInstance().Schedule([emotion]() {
                Board::GetInstance().GetDisplay()->SetEmotion(emotion.c_str());
            });
            SendEvent("emotion", "Emotion updated", "ready");
        }
    } else if (command_type == "scene.play") {
        cJSON* name = cJSON_GetObjectItem(payload, "name");
        if (!cJSON_IsString(name)) {
            SendEvent("scene.error", "Scene is invalid", "invalid_scene");
        } else {
            std::string scene = name->valuestring;
            static const char* kScenes[] = {
                "dance", "greet", "love", "sleep", "coffee", "curious", "celebrate", "idle"
            };
            bool supported = false;
            for (const char* known_scene : kScenes) {
                if (scene == known_scene) {
                    supported = true;
                    break;
                }
            }
            if (!supported) {
                SendEvent("scene.error", "Unknown scene", "invalid_scene");
                cJSON_Delete(root);
                return;
            }
            ServoController* xiao = xiao_controller_;
            Application::GetInstance().Schedule([scene, xiao]() {
                auto* display = Board::GetInstance().GetDisplay();
                if (scene == "dance") {
                    display->SetEmotion("dance");
                    if (xiao) {
                        xiao->SetLed(255, 50, 170);
                        xiao->SetPose("dance");
                    }
                } else if (scene == "greet") {
                    display->SetEmotion("happy");
                    if (xiao) {
                        xiao->SetLed(60, 210, 255);
                        xiao->SetPose("shake");
                    }
                } else if (scene == "love") {
                    display->SetEmotion("love");
                    if (xiao) {
                        xiao->SetLed(255, 60, 150);
                        xiao->SetPose("shake");
                    }
                } else if (scene == "sleep") {
                    display->SetEmotion("sleep");
                    if (xiao) {
                        xiao->SetLed(24, 20, 80);
                        xiao->SetPose("right");
                    }
                } else if (scene == "coffee") {
                    display->SetEmotion("coffee");
                    if (xiao) {
                        xiao->SetLed(255, 150, 50);
                        xiao->SetPose("shake");
                    }
                } else if (scene == "curious") {
                    display->SetEmotion("curious");
                    if (xiao) {
                        xiao->SetLed(80, 190, 255);
                        xiao->SetPose("left");
                    }
                } else if (scene == "celebrate") {
                    display->SetEmotion("celebrate");
                    if (xiao) {
                        xiao->LedTest();
                        xiao->SetPose("dance");
                    }
                } else if (scene == "idle") {
                    display->SetEmotion("neutral");
                    if (xiao) {
                        xiao->SetDefaultLed();
                        xiao->SetPose("home");
                    }
                }
            });
            SendEvent("scene", "Scene started", "ready");
        }
    } else if (command_type == "audio.volume") {
        cJSON* percent = cJSON_GetObjectItem(payload, "percent");
        if (!cJSON_IsNumber(percent)) {
            SendEvent("error", "Volume is invalid", "invalid_volume");
        } else {
            int volume = std::clamp(percent->valueint, 0, 100);
            Application::GetInstance().Schedule([volume]() {
                Board::GetInstance().GetAudioCodec()->SetOutputVolume(volume);
            });
            SendEvent("audio", "Volume updated", "ready");
        }
    } else if (command_type == "display.brightness") {
        cJSON* percent = cJSON_GetObjectItem(payload, "percent");
        if (!cJSON_IsNumber(percent)) {
            SendEvent("error", "Brightness is invalid", "invalid_brightness");
        } else {
            const int brightness = std::clamp(percent->valueint, 5, 100);
            Application::GetInstance().Schedule([brightness]() {
                if (auto* backlight = Board::GetInstance().GetBacklight()) {
                    backlight->SetBrightness(brightness, true);
                }
            });
            SendEvent("display", "Brightness updated", "ready");
        }
    } else if (command_type == "speech.read") {
        cJSON* text = cJSON_GetObjectItem(payload, "text");
        if (!cJSON_IsString(text) || std::strlen(text->valuestring) == 0 ||
            std::strlen(text->valuestring) > 300) {
            SendEvent("error", "Speech text is invalid or too long", "invalid_speech");
        } else {
            std::string message = text->valuestring;
            Application::GetInstance().Schedule([message]() {
                Application::GetInstance().SpeakText(message);
            });
            SendEvent("speech", "Text sent for reading", "ready");
        }
    } else if (command_type == "dialog.send") {
        cJSON* text = cJSON_GetObjectItem(payload, "text");
        cJSON* language = cJSON_GetObjectItem(payload, "language");
        if (!cJSON_IsString(text) || !cJSON_IsString(language) ||
            std::strlen(text->valuestring) == 0 || std::strlen(text->valuestring) > 300 ||
            std::strlen(language->valuestring) == 0 || std::strlen(language->valuestring) > 32) {
            SendEvent("dialog.error", "Dialog text or language is invalid", "invalid_dialog");
        } else {
            std::string message = text->valuestring;
            std::string reply_language = language->valuestring;
            Application::GetInstance().Schedule([message, reply_language]() {
                Application::GetInstance().ChatText(message, reply_language);
            });
            SendEvent("dialog", "Message sent to assistant", "pending");
        }
    } else if (command_type == "media.control") {
        cJSON* action = cJSON_GetObjectItem(payload, "action");
        if (cJSON_IsString(action) && std::strcmp(action->valuestring, "stop") == 0) {
            Application::GetInstance().Schedule([]() {
                Application::GetInstance().AbortSpeaking(kAbortReasonNone);
            });
            SendEvent("media", "Playback stopped", "ready");
        } else {
            SendEvent("media", "Only stop is supported by current audio pipeline", "unsupported");
        }
    } else if (command_type == "xiao.servo") {
        cJSON* yaw = cJSON_GetObjectItem(payload, "yaw");
        cJSON* pitch = cJSON_GetObjectItem(payload, "pitch");
        if (!xiao_controller_) {
            SendEvent("xiao.error", "XIAO bridge is unavailable", "unavailable");
        } else if (!cJSON_IsNumber(yaw) || !cJSON_IsNumber(pitch)) {
            SendEvent("xiao.error", "Servo angles are invalid", "invalid_command");
        } else {
            const int yaw_value = std::clamp(yaw->valueint, 40, 80);
            const int pitch_value = std::clamp(pitch->valueint, 40, 80);
            const bool ok = xiao_controller_->SetMultipleServos({{1, yaw_value}, {2, pitch_value}});
            SendEvent("xiao.servo", ok ? "Servo position sent" : "Servo command failed",
                      ok ? "ready" : "failed");
        }
    } else if (command_type == "xiao.pose") {
        cJSON* name = cJSON_GetObjectItem(payload, "name");
        if (!xiao_controller_) {
            SendEvent("xiao.error", "XIAO bridge is unavailable", "unavailable");
        } else if (!cJSON_IsString(name)) {
            SendEvent("xiao.error", "Pose is invalid", "invalid_command");
        } else {
            const bool ok = xiao_controller_->SetPose(name->valuestring);
            SendEvent("xiao.pose", ok ? "Pose sent" : "Pose command failed", ok ? "ready" : "failed");
        }
    } else if (command_type == "xiao.led") {
        cJSON* red = cJSON_GetObjectItem(payload, "red");
        cJSON* green = cJSON_GetObjectItem(payload, "green");
        cJSON* blue = cJSON_GetObjectItem(payload, "blue");
        if (!xiao_controller_) {
            SendEvent("xiao.error", "XIAO bridge is unavailable", "unavailable");
        } else if (!cJSON_IsNumber(red) || !cJSON_IsNumber(green) || !cJSON_IsNumber(blue)) {
            SendEvent("xiao.error", "LED color is invalid", "invalid_command");
        } else {
            const bool ok = xiao_controller_->SetLed(
                std::clamp(red->valueint, 0, 255),
                std::clamp(green->valueint, 0, 255),
                std::clamp(blue->valueint, 0, 255));
            SendEvent("xiao.led", ok ? "LED color sent" : "LED command failed", ok ? "ready" : "failed");
        }
    } else if (command_type == "xiao.led_action") {
        cJSON* action = cJSON_GetObjectItem(payload, "action");
        if (!xiao_controller_) {
            SendEvent("xiao.error", "XIAO bridge is unavailable", "unavailable");
        } else if (!cJSON_IsString(action)) {
            SendEvent("xiao.error", "LED action is invalid", "invalid_command");
        } else {
            bool ok = false;
            if (std::strcmp(action->valuestring, "default") == 0) {
                ok = xiao_controller_->SetDefaultLed();
            } else if (std::strcmp(action->valuestring, "off") == 0) {
                ok = xiao_controller_->LedOff();
            } else if (std::strcmp(action->valuestring, "test") == 0) {
                ok = xiao_controller_->LedTest();
            } else {
                SendEvent("xiao.error", "Unknown LED action", "invalid_command");
                cJSON_Delete(root);
                return;
            }
            SendEvent("xiao.led", ok ? "LED action sent" : "LED command failed", ok ? "ready" : "failed");
        }
    } else if (command_type == "xiao.status" || command_type == "xiao.distance" ||
               command_type == "xiao.radar") {
        if (!xiao_controller_) {
            SendEvent("xiao.error", "XIAO bridge is unavailable", "unavailable");
        } else {
            std::string response;
            bool ok = false;
            if (command_type == "xiao.status") {
                ok = xiao_controller_->RequestStatus(response);
            } else if (command_type == "xiao.distance") {
                ok = xiao_controller_->RequestDistance(response);
            } else {
                ok = xiao_controller_->RequestRadar(response);
            }
            SendEvent(command_type, ok ? response : "No reply from XIAO", ok ? "ready" : "timeout");
        }
    } else {
        SendEvent("error", "Unsupported command", "unsupported");
    }
    cJSON_Delete(root);
}
