#include "servo_controller.h"

#ifdef SERVO_CONTROLLER_CONFIG_HEADER
#include SERVO_CONTROLLER_CONFIG_HEADER
#else
#include "config.h"
#endif

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include <driver/gpio.h>
#include <driver/uart.h>
#include <esp_now.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "XiaoBridge"
#define XIAO_MAX_FRAME_BYTES (128 * 1024)
#ifndef XIAO_UART_ENABLED
#define XIAO_UART_ENABLED 0
#endif
#ifndef LOCAL_SERVO_ENABLED
#define LOCAL_SERVO_ENABLED 0
#endif

#if LOCAL_SERVO_ENABLED
#include <driver/ledc.h>
#endif

namespace {
constexpr uint32_t kEspNowMagic = 0x574e4248;  // HBNW in little endian.
constexpr uint8_t kEspNowVersion = 1;
constexpr uint8_t kEspNowCommand = 1;
constexpr uint8_t kEspNowResponse = 2;
constexpr size_t kEspNowPayloadSize = 192;
constexpr int64_t kEspNowProbeIntervalUs = 5 * 1000 * 1000;
constexpr uint8_t kBroadcastMac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

struct __attribute__((packed)) EspNowPacket {
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint16_t sequence;
    char payload[kEspNowPayloadSize];
};

#if LOCAL_SERVO_ENABLED
// LEDC_TIMER_1/CHANNEL_0 belong to GpioLed; keep servos on their own timer.
constexpr ledc_mode_t kServoSpeedMode = LEDC_LOW_SPEED_MODE;
constexpr ledc_timer_t kServoTimer = LEDC_TIMER_2;
constexpr ledc_timer_bit_t kServoResolution = LEDC_TIMER_14_BIT;
constexpr uint32_t kServoFrequencyHz = 50;
constexpr ledc_channel_t kServoYawChannel = LEDC_CHANNEL_6;
constexpr ledc_channel_t kServoPitchChannel = LEDC_CHANNEL_7;

uint32_t ServoAngleToDuty(int angle) {
    uint32_t pulse_us = LOCAL_SERVO_MIN_PULSE_US +
        (uint32_t)(LOCAL_SERVO_MAX_PULSE_US - LOCAL_SERVO_MIN_PULSE_US) * angle / 180;
    uint32_t period_us = 1000000 / kServoFrequencyHz;
    return (uint32_t)(((uint64_t)pulse_us << 14) / period_us);
}
#endif
}

ServoController* ServoController::espnow_instance_ = nullptr;

ServoController::ServoController()
    : uart_port_(XIAO_UART_PORT_NUM), initialized_(false) {
}

ServoController::~ServoController() {
#if LOCAL_SERVO_ENABLED
    if (local_servo_release_timer_) {
        ReleaseLocalServos(true);
        esp_timer_delete(local_servo_release_timer_);
        local_servo_release_timer_ = nullptr;
    }
#endif
    if (espnow_initialized_) {
        esp_now_unregister_recv_cb();
        esp_now_deinit();
        espnow_initialized_ = false;
    }
    if (espnow_response_sem_) {
        vSemaphoreDelete(espnow_response_sem_);
        espnow_response_sem_ = nullptr;
    }
    if (initialized_) {
        uart_driver_delete(uart_port_);
        initialized_ = false;
    }
}

bool ServoController::Init() {
    if (initialized_) {
        ESP_LOGW(TAG, "XIAO bridge already initialized");
        return true;
    }

#if LOCAL_SERVO_ENABLED
    InitLocalServos();
#endif

#if !XIAO_UART_ENABLED
    ESP_LOGI(TAG, "XIAO UART bridge disabled; using ESP-NOW only");
    return true;
#else
    ESP_LOGI(TAG, "Init XIAO UART bridge");
    ESP_LOGI(TAG, "  Port: UART_NUM_%d", uart_port_);
    ESP_LOGI(TAG, "  Baud: %d", XIAO_UART_BAUD_RATE);
    ESP_LOGI(TAG, "  TX Pin: GPIO%d", XIAO_UART_TX_PIN);
    ESP_LOGI(TAG, "  RX Pin: GPIO%d", XIAO_UART_RX_PIN);

    uart_config_t uart_config = {
        .baud_rate = XIAO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_param_config(uart_port_, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART param config failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = uart_set_pin(uart_port_, XIAO_UART_TX_PIN, XIAO_UART_RX_PIN,
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART pin config failed: %s", esp_err_to_name(ret));
        return false;
    }

    ret = uart_driver_install(uart_port_, 4096, 4096, 0, nullptr, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "UART driver install failed: %s", esp_err_to_name(ret));
        return false;
    }

    initialized_ = true;
    uart_flush_input(uart_port_);
    vTaskDelay(pdMS_TO_TICKS(100));

    SetDefaultLed();
    SetPose("home");
    ESP_LOGI(TAG, "XIAO bridge UART initialized");
    return true;
#endif
}

bool ServoController::InitEspNow() {
#if !XIAO_ESPNOW_ENABLED
    return false;
#else
    if (espnow_initialized_) {
        return true;
    }

    esp_err_t ret = esp_now_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW init failed: %s", esp_err_to_name(ret));
        return false;
    }

    espnow_response_sem_ = xSemaphoreCreateBinary();
    if (!espnow_response_sem_) {
        ESP_LOGE(TAG, "ESP-NOW response semaphore allocation failed");
        esp_now_deinit();
        return false;
    }

    espnow_instance_ = this;
    ret = esp_now_register_recv_cb(EspNowReceiveCallback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW RX callback registration failed: %s", esp_err_to_name(ret));
        vSemaphoreDelete(espnow_response_sem_);
        espnow_response_sem_ = nullptr;
        esp_now_deinit();
        return false;
    }

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, kBroadcastMac, ESP_NOW_ETH_ALEN);
    peer.ifidx = WIFI_IF_STA;
    peer.channel = 0;
    peer.encrypt = false;
    ret = esp_now_add_peer(&peer);
    if (ret != ESP_OK && ret != ESP_ERR_ESPNOW_EXIST) {
        ESP_LOGE(TAG, "ESP-NOW broadcast peer registration failed: %s", esp_err_to_name(ret));
        esp_now_unregister_recv_cb();
        esp_now_deinit();
        vSemaphoreDelete(espnow_response_sem_);
        espnow_response_sem_ = nullptr;
        return false;
    }

    uint8_t primary = 0;
    wifi_second_chan_t secondary = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&primary, &secondary);
    espnow_initialized_ = true;
    espnow_peer_online_ = false;
    espnow_peer_mac_known_ = false;
    espnow_failures_ = 0;
    last_espnow_probe_us_ = esp_timer_get_time();
    ESP_LOGI(TAG, "ESP-NOW XIAO bridge enabled on Wi-Fi channel %u", primary);

    std::string response;
    {
        std::lock_guard<std::mutex> lock(uart_mutex_);
        if (SendEspNowCommandLocked("PING", &response, 1000)) {
            espnow_peer_online_ = true;
            espnow_failures_ = 0;
            ESP_LOGI(TAG, "ESP-NOW XIAO peer online: %s", response.c_str());
            SendEspNowCommandLocked("LEDDEFAULT", nullptr, 0);
            char home_command[32];
            snprintf(home_command, sizeof(home_command), "SERVO %d %d",
                     SERVO_HOME_ANGLE, SERVO_HOME_ANGLE);
            SendEspNowCommandLocked(home_command, nullptr, 0);
        } else {
            espnow_peer_online_ = false;
            espnow_failures_++;
            last_espnow_probe_us_ = 0;
            ESP_LOGW(TAG, "ESP-NOW initialized; XIAO peer has not replied yet");
        }
    }
    return true;
#endif
}

void ServoController::EspNowReceiveCallback(
    const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (espnow_instance_) {
        espnow_instance_->HandleEspNowPacket(info, data, len);
    }
}

void ServoController::HandleEspNowPacket(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (len < (int)(sizeof(EspNowPacket) - kEspNowPayloadSize + 1)) {
        return;
    }
    const auto* packet = reinterpret_cast<const EspNowPacket*>(data);
    if (packet->magic != kEspNowMagic || packet->version != kEspNowVersion ||
        packet->type != kEspNowResponse || packet->sequence != espnow_waiting_sequence_) {
        return;
    }
    size_t available = (size_t)len - (sizeof(EspNowPacket) - kEspNowPayloadSize);
    size_t length = strnlen(packet->payload, std::min(available, kEspNowPayloadSize));
    espnow_response_.assign(packet->payload, length);
    if (info) {
        memcpy(espnow_peer_mac_, info->src_addr, ESP_NOW_ETH_ALEN);
        espnow_peer_mac_known_ = true;
    }
    xSemaphoreGive(espnow_response_sem_);
}

bool ServoController::EnsureEspNowPeerLocked(const uint8_t* mac) {
    if (!espnow_initialized_ || mac == nullptr) {
        return false;
    }
    if (esp_now_is_peer_exist(mac)) {
        return true;
    }

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, mac, ESP_NOW_ETH_ALEN);
    peer.ifidx = WIFI_IF_STA;
    peer.channel = 0;
    peer.encrypt = false;
    esp_err_t ret = esp_now_add_peer(&peer);
    if (ret != ESP_OK && ret != ESP_ERR_ESPNOW_EXIST) {
        ESP_LOGW(TAG, "ESP-NOW peer registration failed: %s", esp_err_to_name(ret));
        return false;
    }
    return true;
}

bool ServoController::SendEspNowCommandLocked(
    const std::string& command, std::string* response, int timeout_ms) {
    if (!espnow_initialized_ || !espnow_response_sem_ || command.size() >= kEspNowPayloadSize) {
        return false;
    }
    EspNowPacket packet = {};
    packet.magic = kEspNowMagic;
    packet.version = kEspNowVersion;
    packet.type = kEspNowCommand;
    packet.sequence = ++espnow_sequence_;
    memcpy(packet.payload, command.c_str(), command.size() + 1);

    while (xSemaphoreTake(espnow_response_sem_, 0) == pdTRUE) {}
    espnow_waiting_sequence_ = packet.sequence;
    espnow_response_.clear();
    size_t length = sizeof(EspNowPacket) - kEspNowPayloadSize + command.size() + 1;
    const uint8_t* destination = espnow_peer_mac_known_ ? espnow_peer_mac_ : kBroadcastMac;
    if (espnow_peer_mac_known_ && !EnsureEspNowPeerLocked(destination)) {
        destination = kBroadcastMac;
    }
    esp_err_t ret = esp_now_send(destination, reinterpret_cast<const uint8_t*>(&packet), length);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "ESP-NOW send failed for %s: %s", command.c_str(), esp_err_to_name(ret));
        return false;
    }
    if (!response) {
        return true;
    }
    if (xSemaphoreTake(espnow_response_sem_, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        ESP_LOGW(TAG, "ESP-NOW timeout for command: %s", command.c_str());
        espnow_waiting_sequence_ = 0;
        return false;
    }
    if (response) {
        *response = espnow_response_;
    }
    espnow_waiting_sequence_ = 0;
    ESP_LOGD(TAG, "ESP-NOW RX: %s", espnow_response_.c_str());
    return true;
}

bool ServoController::RefreshEspNowPeerLocked() {
    if (!espnow_initialized_) {
        return false;
    }
    if (espnow_peer_online_) {
        return true;
    }

    int64_t now = esp_timer_get_time();
    if (now - last_espnow_probe_us_ < kEspNowProbeIntervalUs) {
        return false;
    }
    last_espnow_probe_us_ = now;

    std::string response;
    if (SendEspNowCommandLocked("PING", &response, 300)) {
        espnow_peer_online_ = true;
        espnow_failures_ = 0;
        ESP_LOGI(TAG, "ESP-NOW XIAO peer recovered: %s", response.c_str());
        return true;
    }

    espnow_failures_++;
    ESP_LOGW(TAG, "ESP-NOW XIAO peer still offline, failures=%d", espnow_failures_);
    return false;
}

int ServoController::ClampAngle(int angle) const {
    return std::max(SERVO_MIN_ANGLE, std::min(SERVO_MAX_ANGLE, angle));
}

#if LOCAL_SERVO_ENABLED
bool ServoController::InitLocalServos() {
    // The GPIOs stay untouched here; PWM attaches on the first servo command
    // and detaches LOCAL_SERVO_HOLD_MS after the last one.
    const esp_timer_create_args_t timer_args = {
        .callback = LocalServoReleaseCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "servo_release",
        .skip_unhandled_events = true,
    };
    esp_err_t ret = esp_timer_create(&timer_args, &local_servo_release_timer_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Servo release timer create failed: %s", esp_err_to_name(ret));
        return false;
    }

    local_servos_ready_ = true;
    local_yaw_angle_ = SERVO_HOME_ANGLE;
    local_pitch_angle_ = SERVO_HOME_ANGLE;
    ESP_LOGI(TAG, "Local head servos on demand: yaw=GPIO%d pitch=GPIO%d hold=%dms",
             LOCAL_SERVO_YAW_GPIO, LOCAL_SERVO_PITCH_GPIO, LOCAL_SERVO_HOLD_MS);
    return true;
}

bool ServoController::AttachLocalServosLocked() {
    if (local_servos_attached_) {
        return true;
    }

    ledc_timer_config_t timer_config = {
        .speed_mode = kServoSpeedMode,
        .duty_resolution = kServoResolution,
        .timer_num = kServoTimer,
        .freq_hz = kServoFrequencyHz,
        .clk_cfg = LEDC_AUTO_CLK,
        .deconfigure = false,
    };
    esp_err_t ret = ledc_timer_config(&timer_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Servo LEDC timer config failed: %s", esp_err_to_name(ret));
        return false;
    }

    const struct {
        int gpio;
        ledc_channel_t channel;
        int angle;
        const char* name;
    } servos[] = {
        {LOCAL_SERVO_YAW_GPIO, kServoYawChannel, local_yaw_angle_, "yaw"},
        {LOCAL_SERVO_PITCH_GPIO, kServoPitchChannel, local_pitch_angle_, "pitch"},
    };
    for (const auto& servo : servos) {
        ledc_channel_config_t channel_config = {
            .gpio_num = servo.gpio,
            .speed_mode = kServoSpeedMode,
            .channel = servo.channel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = kServoTimer,
            .duty = ServoAngleToDuty(servo.angle),
            .hpoint = 0,
            .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
            .flags = {},
        };
        ret = ledc_channel_config(&channel_config);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Servo %s channel config failed on GPIO%d: %s",
                     servo.name, servo.gpio, esp_err_to_name(ret));
            return false;
        }
    }

    local_servos_attached_ = true;
    ESP_LOGD(TAG, "Local servos attached");
    return true;
}

void ServoController::ReleaseLocalServos(bool force) {
    std::lock_guard<std::mutex> lock(local_servo_mutex_);
    if (!local_servos_attached_) {
        return;
    }
    // A command that re-armed the timer while this call waited on the lock
    // means the servos are in use again — keep them attached.
    if (!force && esp_timer_is_active(local_servo_release_timer_)) {
        return;
    }
    esp_timer_stop(local_servo_release_timer_);
    ledc_stop(kServoSpeedMode, kServoYawChannel, 0);
    ledc_stop(kServoSpeedMode, kServoPitchChannel, 0);
    // Free the pads: high-Z inputs until the next servo command.
    gpio_config_t input_config = {
        .pin_bit_mask = (1ULL << LOCAL_SERVO_YAW_GPIO) | (1ULL << LOCAL_SERVO_PITCH_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&input_config);
    local_servos_attached_ = false;
    ESP_LOGD(TAG, "Local servos released");
}

void ServoController::LocalServoReleaseCallback(void* arg) {
    static_cast<ServoController*>(arg)->ReleaseLocalServos(false);
}

bool ServoController::SetLocalServoAngle(int servo_num, int angle) {
    ledc_channel_t channel;
    if (servo_num == SERVO_HEAD_YAW_NUM) {
        channel = kServoYawChannel;
    } else if (servo_num == SERVO_HEAD_PITCH_NUM) {
        channel = kServoPitchChannel;
    } else {
        ESP_LOGE(TAG, "Invalid servo number: %d (yaw=%d, pitch=%d)",
                 servo_num, SERVO_HEAD_YAW_NUM, SERVO_HEAD_PITCH_NUM);
        return false;
    }

    std::lock_guard<std::mutex> lock(local_servo_mutex_);
    if (!AttachLocalServosLocked()) {
        return false;
    }

    uint32_t duty = ServoAngleToDuty(angle);
    esp_err_t ret = ledc_set_duty(kServoSpeedMode, channel, duty);
    if (ret == ESP_OK) {
        ret = ledc_update_duty(kServoSpeedMode, channel);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Servo S%d duty update failed: %s", servo_num, esp_err_to_name(ret));
        return false;
    }

    if (servo_num == SERVO_HEAD_YAW_NUM) {
        local_yaw_angle_ = angle;
    } else {
        local_pitch_angle_ = angle;
    }

    esp_timer_stop(local_servo_release_timer_);
    esp_timer_start_once(local_servo_release_timer_, (uint64_t)LOCAL_SERVO_HOLD_MS * 1000);
    ESP_LOGD(TAG, "Local servo S%d -> %d deg (duty %lu)", servo_num, angle, (unsigned long)duty);
    return true;
}
#endif

bool ServoController::SendCommandLocked(const std::string& command) {
    if (!initialized_) {
        ESP_LOGE(TAG, "XIAO bridge is not initialized");
        return false;
    }

    std::string line = command + "\n";
    int written = uart_write_bytes(uart_port_, line.c_str(), line.size());
    if (written != (int)line.size()) {
        ESP_LOGE(TAG, "UART write failed for command: %s", command.c_str());
        return false;
    }
    ESP_LOGD(TAG, "TX: %s", command.c_str());
    return true;
}

bool ServoController::SendCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(uart_mutex_);
    if (espnow_initialized_ && (espnow_peer_online_ || RefreshEspNowPeerLocked())) {
        if (SendEspNowCommandLocked(command, nullptr, 0)) {
            return true;
        }
        espnow_peer_online_ = false;
        espnow_failures_++;
        ESP_LOGW(TAG, "ESP-NOW send failed for: %s", command.c_str());
    }
#if XIAO_UART_ENABLED
    return SendCommandLocked(command);
#else
    ESP_LOGW(TAG, "XIAO ESP-NOW peer is offline, command not sent: %s", command.c_str());
    return false;
#endif
}

bool ServoController::ReadLineLocked(std::string& response, int timeout_ms) {
    response.clear();
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    uint8_t c = 0;

    while (esp_timer_get_time() < deadline) {
        int len = uart_read_bytes(uart_port_, &c, 1, pdMS_TO_TICKS(20));
        if (len <= 0) {
            continue;
        }
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            if (!response.empty()) {
                ESP_LOGD(TAG, "RX: %s", response.c_str());
                return true;
            }
            continue;
        }
        if (response.size() < 256) {
            response.push_back((char)c);
        }
    }
    return false;
}

bool ServoController::ReadBytesLocked(uint8_t* data, size_t len, int timeout_ms) {
    if (len == 0) {
        return true;
    }

    size_t total = 0;
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    while (total < len && esp_timer_get_time() < deadline) {
        int remaining_ms = (int)((deadline - esp_timer_get_time()) / 1000);
        if (remaining_ms <= 0) {
            break;
        }
        int chunk = uart_read_bytes(
            uart_port_,
            data + total,
            len - total,
            pdMS_TO_TICKS(std::min(remaining_ms, 100)));
        if (chunk > 0) {
            total += chunk;
        }
    }
    return total == len;
}

bool ServoController::SendCommandAndReadLine(const std::string& command, std::string& response, int timeout_ms) {
    std::lock_guard<std::mutex> lock(uart_mutex_);
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!espnow_initialized_ || (!espnow_peer_online_ && !RefreshEspNowPeerLocked())) {
            break;
        }
        if (SendEspNowCommandLocked(command, &response, timeout_ms)) {
            espnow_peer_online_ = true;
            espnow_failures_ = 0;
            return true;
        }
        espnow_peer_online_ = false;
        espnow_failures_++;
        ESP_LOGW(TAG, "ESP-NOW request attempt %d failed for: %s", attempt + 1, command.c_str());
    }
#if XIAO_UART_ENABLED
    if (!initialized_) {
        ESP_LOGE(TAG, "XIAO bridge is not initialized");
        return false;
    }
    uart_flush_input(uart_port_);
    if (!SendCommandLocked(command)) {
        return false;
    }
    return ReadLineLocked(response, timeout_ms);
#else
    ESP_LOGW(TAG, "No ESP-NOW reply from XIAO for: %s", command.c_str());
    return false;
#endif
}

bool ServoController::SetServoAngle(int servo_num, int angle) {
    angle = ClampAngle(angle);
#if LOCAL_SERVO_ENABLED
    if (local_servos_ready_) {
        return SetLocalServoAngle(servo_num, angle);
    }
#endif
    if (servo_num == SERVO_HEAD_YAW_NUM) {
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "YAW %d", angle);
        return SendCommand(cmd);
    }
    if (servo_num == SERVO_HEAD_PITCH_NUM) {
        char cmd[32];
        snprintf(cmd, sizeof(cmd), "PITCH %d", angle);
        return SendCommand(cmd);
    }

    ESP_LOGE(TAG, "Invalid servo number: %d (yaw=%d, pitch=%d)",
             servo_num, SERVO_HEAD_YAW_NUM, SERVO_HEAD_PITCH_NUM);
    return false;
}

bool ServoController::SetMultipleServos(const std::vector<std::pair<int, int>>& commands) {
    int yaw = -1;
    int pitch = -1;

    for (const auto& command : commands) {
        int angle = ClampAngle(command.second);
        if (command.first == SERVO_HEAD_YAW_NUM) {
            yaw = angle;
        } else if (command.first == SERVO_HEAD_PITCH_NUM) {
            pitch = angle;
        } else {
            ESP_LOGW(TAG, "Ignoring unsupported servo S%d", command.first);
        }
    }

    if (yaw >= 0 && pitch >= 0) {
#if LOCAL_SERVO_ENABLED
        if (local_servos_ready_) {
            bool ok = SetLocalServoAngle(SERVO_HEAD_YAW_NUM, yaw);
            return SetLocalServoAngle(SERVO_HEAD_PITCH_NUM, pitch) && ok;
        }
#endif
        char cmd[48];
        snprintf(cmd, sizeof(cmd), "SERVO %d %d", yaw, pitch);
        return SendCommand(cmd);
    }

    bool ok = true;
    if (yaw >= 0) {
        ok = SetServoAngle(SERVO_HEAD_YAW_NUM, yaw) && ok;
    }
    if (pitch >= 0) {
        ok = SetServoAngle(SERVO_HEAD_PITCH_NUM, pitch) && ok;
    }
    return ok;
}

bool ServoController::SetPose(const std::string& pose_name) {
    ESP_LOGI(TAG, "Set pose: %s", pose_name.c_str());

    if (pose_name == "home" || pose_name == "reset") {
        return SetMultipleServos({
            {SERVO_HEAD_YAW_NUM, SERVO_HOME_ANGLE},
            {SERVO_HEAD_PITCH_NUM, SERVO_HOME_ANGLE},
        });
    } else if (pose_name == "left" || pose_name == "look_left") {
        return SetServoAngle(SERVO_HEAD_YAW_NUM, SERVO_MIN_ANGLE);
    } else if (pose_name == "right" || pose_name == "look_right") {
        return SetServoAngle(SERVO_HEAD_YAW_NUM, SERVO_MAX_ANGLE);
    } else if (pose_name == "up" || pose_name == "look_up") {
        return SetServoAngle(SERVO_HEAD_PITCH_NUM, SERVO_MIN_ANGLE);
    } else if (pose_name == "down" || pose_name == "look_down") {
        return SetServoAngle(SERVO_HEAD_PITCH_NUM, SERVO_MAX_ANGLE);
    } else if (pose_name == "nod" || pose_name == "nodding") {
        SetServoAngle(SERVO_HEAD_PITCH_NUM, SERVO_MIN_ANGLE);
        vTaskDelay(pdMS_TO_TICKS(200));
        SetServoAngle(SERVO_HEAD_PITCH_NUM, SERVO_MAX_ANGLE);
        vTaskDelay(pdMS_TO_TICKS(200));
        return SetServoAngle(SERVO_HEAD_PITCH_NUM, SERVO_HOME_ANGLE);
    } else if (pose_name == "shake" || pose_name == "swing" || pose_name == "покачивание") {
        SetServoAngle(SERVO_HEAD_YAW_NUM, SERVO_MIN_ANGLE);
        vTaskDelay(pdMS_TO_TICKS(180));
        SetServoAngle(SERVO_HEAD_YAW_NUM, SERVO_MAX_ANGLE);
        vTaskDelay(pdMS_TO_TICKS(180));
        return SetServoAngle(SERVO_HEAD_YAW_NUM, SERVO_HOME_ANGLE);
    } else if (pose_name == "dance" || pose_name == "dancing") {
        SetMultipleServos({{SERVO_HEAD_YAW_NUM, SERVO_MIN_ANGLE}, {SERVO_HEAD_PITCH_NUM, SERVO_MAX_ANGLE}});
        vTaskDelay(pdMS_TO_TICKS(300));
        SetMultipleServos({{SERVO_HEAD_YAW_NUM, SERVO_MAX_ANGLE}, {SERVO_HEAD_PITCH_NUM, SERVO_MIN_ANGLE}});
        vTaskDelay(pdMS_TO_TICKS(300));
        return SetPose("home");
    } else if (pose_name == "dizzy" || pose_name == "spinning") {
        SetMultipleServos({{SERVO_HEAD_YAW_NUM, SERVO_MIN_ANGLE}, {SERVO_HEAD_PITCH_NUM, SERVO_MIN_ANGLE}});
        vTaskDelay(pdMS_TO_TICKS(150));
        SetMultipleServos({{SERVO_HEAD_YAW_NUM, SERVO_MAX_ANGLE}, {SERVO_HEAD_PITCH_NUM, SERVO_MIN_ANGLE}});
        vTaskDelay(pdMS_TO_TICKS(150));
        SetMultipleServos({{SERVO_HEAD_YAW_NUM, SERVO_MAX_ANGLE}, {SERVO_HEAD_PITCH_NUM, SERVO_MAX_ANGLE}});
        vTaskDelay(pdMS_TO_TICKS(150));
        SetMultipleServos({{SERVO_HEAD_YAW_NUM, SERVO_MIN_ANGLE}, {SERVO_HEAD_PITCH_NUM, SERVO_MAX_ANGLE}});
        vTaskDelay(pdMS_TO_TICKS(150));
        return SetPose("home");
    } else if (pose_name == "greet" || pose_name == "greeting") {
        return SetPose("nod");
    } else if (pose_name == "sad" || pose_name == "sad_pose") {
        return SetServoAngle(SERVO_HEAD_PITCH_NUM, SERVO_MAX_ANGLE);
    } else if (pose_name == "happy" || pose_name == "happy_pose") {
        return SetServoAngle(SERVO_HEAD_PITCH_NUM, SERVO_MIN_ANGLE);
    }

    ESP_LOGW(TAG, "Unknown pose: %s", pose_name.c_str());
    return false;
}

bool ServoController::SetLed(int r, int g, int b) {
    r = std::max(0, std::min(255, r));
    g = std::max(0, std::min(255, g));
    b = std::max(0, std::min(255, b));
    char cmd[48];
    snprintf(cmd, sizeof(cmd), "LED %d %d %d", r, g, b);
    return SendCommand(cmd);
}

bool ServoController::SetDefaultLed() {
    return SetLed(XIAO_LED_DEFAULT_R, XIAO_LED_DEFAULT_G, XIAO_LED_DEFAULT_B);
}

bool ServoController::LedTest() {
    return SendCommand("LEDTEST");
}

bool ServoController::LedOff() {
    return SendCommand("LEDOFF");
}

bool ServoController::SetPowerSaveMode(bool sleeping) {
#if LOCAL_SERVO_ENABLED
    if (local_servos_ready_ && sleeping) {
        // Wake needs no action: PWM re-attaches on the next servo command.
        ReleaseLocalServos(true);
    }
#endif
    return SendCommand(sleeping ? "POWER SLEEP" : "POWER WAKE");
}

bool ServoController::RequestDistance(std::string& response) {
    return SendCommandAndReadLine("DIST?", response, 700);
}

bool ServoController::RequestRadar(std::string& response) {
    return SendCommandAndReadLine("RADAR?", response, 700);
}

bool ServoController::RequestStatus(std::string& response) {
    return SendCommandAndReadLine("STATUS?", response, 700);
}

bool ServoController::RequestFrame(std::string& jpeg_data, uint32_t& frame_id, std::string& error) {
    jpeg_data.clear();
    frame_id = 0;
    error.clear();

#if !XIAO_UART_ENABLED
    error = "FRAME over ESP-NOW is unsupported; use the XIAO HTTP /capture endpoint";
    return false;
#else
    std::lock_guard<std::mutex> lock(uart_mutex_);
    if (!initialized_) {
        error = "XIAO bridge is not initialized";
        return false;
    }

    uart_flush_input(uart_port_);
    if (!SendCommandLocked("FRAME")) {
        error = "UART write failed";
        return false;
    }

    std::string header;
    if (!ReadLineLocked(header, 2500)) {
        error = "FRAME header timeout";
        return false;
    }
    if (header.rfind("ERR ", 0) == 0) {
        error = header;
        return false;
    }

    unsigned long id = 0;
    unsigned int len = 0;
    if (sscanf(header.c_str(), "@FRAME %lu %u", &id, &len) != 2) {
        error = "Unexpected FRAME header: " + header;
        return false;
    }
    if (len == 0 || len > XIAO_MAX_FRAME_BYTES) {
        error = "Invalid FRAME size: " + std::to_string(len);
        return false;
    }

    jpeg_data.resize(len);
    if (!ReadBytesLocked(reinterpret_cast<uint8_t*>(&jpeg_data[0]), len, 5000)) {
        jpeg_data.clear();
        error = "FRAME data timeout";
        return false;
    }

    std::string end_line;
    if (!ReadLineLocked(end_line, 1000)) {
        jpeg_data.clear();
        error = "FRAME end marker timeout";
        return false;
    }

    unsigned long end_id = 0;
    if (sscanf(end_line.c_str(), "@END %lu", &end_id) != 1 || end_id != id) {
        jpeg_data.clear();
        error = "Unexpected FRAME end marker: " + end_line;
        return false;
    }

    frame_id = (uint32_t)id;
    return true;
#endif
}

bool ServoController::CameraStreamOff() {
    std::string response;
    return SendCommandAndReadLine("STREAM OFF", response, 700) &&
           response.rfind("OK", 0) == 0;
}
