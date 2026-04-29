#include "servo_controller.h"
#include <esp_log.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>
#include "config.h"

#define TAG "ServoController"

ServoController::ServoController() 
    : initialized_(false) {
}

ServoController::~ServoController() {
    if (initialized_) {
        ledc_stop(SERVO_PWM_SPEED_MODE, SERVO_PWM_YAW_CHANNEL, 0);
        ledc_stop(SERVO_PWM_SPEED_MODE, SERVO_PWM_PITCH_CHANNEL, 0);
        initialized_ = false;
    }
}

bool ServoController::Init() {
    if (initialized_) {
        ESP_LOGW(TAG, "ServoController уже инициализирован");
        return true;
    }

    ESP_LOGI(TAG, "Инициализация PWM сервоприводов головы");
    ESP_LOGI(TAG, "  Yaw S%d: GPIO%d", SERVO_HEAD_YAW_NUM, SERVO_HEAD_YAW_PIN);
    ESP_LOGI(TAG, "  Pitch S%d: GPIO%d", SERVO_HEAD_PITCH_NUM, SERVO_HEAD_PITCH_PIN);
    ESP_LOGI(TAG, "  PWM: %d Hz, pulse %d-%d us",
             SERVO_PWM_FREQ_HZ, SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);

    if (SERVO_HEAD_YAW_PIN == AUDIO_CODEC_I2C_SDA_PIN || SERVO_HEAD_YAW_PIN == AUDIO_CODEC_I2C_SCL_PIN) {
        ESP_LOGW(TAG, "SERVO_HEAD_YAW_PIN conflicts with codec I2C bus");
    }
    if (SERVO_HEAD_PITCH_PIN == AUDIO_CODEC_I2C_SDA_PIN || SERVO_HEAD_PITCH_PIN == AUDIO_CODEC_I2C_SCL_PIN) {
        ESP_LOGW(TAG, "SERVO_HEAD_PITCH_PIN conflicts with codec I2C bus");
    }

    ledc_timer_config_t timer_config = {
        .speed_mode = SERVO_PWM_SPEED_MODE,
        .duty_resolution = SERVO_PWM_RESOLUTION,
        .timer_num = SERVO_PWM_TIMER,
        .freq_hz = SERVO_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t ret = ledc_timer_config(&timer_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка настройки PWM таймера: %s", esp_err_to_name(ret));
        return false;
    }

    if (!ConfigureChannel(SERVO_HEAD_YAW_NUM, SERVO_HEAD_YAW_PIN, SERVO_PWM_YAW_CHANNEL)) {
        return false;
    }
    if (!ConfigureChannel(SERVO_HEAD_PITCH_NUM, SERVO_HEAD_PITCH_PIN, SERVO_PWM_PITCH_CHANNEL)) {
        return false;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "PWM сервоприводов инициализирован успешно");
    vTaskDelay(pdMS_TO_TICKS(100));
    SetPose("home");

    return true;
}

bool ServoController::ConfigureChannel(int servo_num, gpio_num_t pin, ledc_channel_t channel) {
    if (pin == GPIO_NUM_NC) {
        ESP_LOGW(TAG, "S%d отключён: GPIO_NUM_NC", servo_num);
        return true;
    }

    ledc_channel_config_t channel_config = {
        .gpio_num = pin,
        .speed_mode = SERVO_PWM_SPEED_MODE,
        .channel = channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = SERVO_PWM_TIMER,
        .duty = AngleToDuty(SERVO_HOME_ANGLE),
        .hpoint = 0,
        .flags = {
            .output_invert = 0,
        },
    };
    esp_err_t ret = ledc_channel_config(&channel_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка настройки PWM S%d GPIO%d: %s",
                 servo_num, pin, esp_err_to_name(ret));
        return false;
    }
    return true;
}

uint32_t ServoController::AngleToDuty(int angle) const {
    angle = std::max(SERVO_MIN_ANGLE, std::min(SERVO_MAX_ANGLE, angle));
    const int pulse_us = SERVO_MIN_PULSE_US +
        ((SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US) * (angle - SERVO_MIN_ANGLE)) /
        std::max(1, SERVO_MAX_ANGLE - SERVO_MIN_ANGLE);
    const uint32_t max_duty = (1UL << SERVO_PWM_RESOLUTION) - 1;
    const uint32_t period_us = 1000000UL / SERVO_PWM_FREQ_HZ;
    return (uint32_t)(((uint64_t)pulse_us * max_duty) / period_us);
}

bool ServoController::GetChannelForServo(int servo_num, ledc_channel_t& channel) const {
    if (servo_num == SERVO_HEAD_YAW_NUM) {
        channel = SERVO_PWM_YAW_CHANNEL;
        return true;
    }
    if (servo_num == SERVO_HEAD_PITCH_NUM) {
        channel = SERVO_PWM_PITCH_CHANNEL;
        return true;
    }
    ESP_LOGE(TAG, "Неверный номер сервопривода: %d (yaw=%d, pitch=%d)",
             servo_num, SERVO_HEAD_YAW_NUM, SERVO_HEAD_PITCH_NUM);
    return false;
}

bool ServoController::WriteAngle(ledc_channel_t channel, int angle) {
    const uint32_t duty = AngleToDuty(angle);
    esp_err_t ret = ledc_set_duty(SERVO_PWM_SPEED_MODE, channel, duty);
    if (ret == ESP_OK) {
        ret = ledc_update_duty(SERVO_PWM_SPEED_MODE, channel);
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка установки PWM channel %d: %s", channel, esp_err_to_name(ret));
        return false;
    }
    return true;
}

bool ServoController::SetServoAngle(int servo_num, int angle) {
    if (!initialized_) {
        ESP_LOGE(TAG, "ServoController не инициализирован");
        return false;
    }
    ledc_channel_t channel;
    if (!GetChannelForServo(servo_num, channel)) {
        return false;
    }
    angle = std::max(SERVO_MIN_ANGLE, std::min(SERVO_MAX_ANGLE, angle));
    std::lock_guard<std::mutex> lock(servo_mutex_);
    ESP_LOGI(TAG, "S%d -> %d°", servo_num, angle);
    return WriteAngle(channel, angle);
}

bool ServoController::SetMultipleServos(const std::vector<std::pair<int, int>>& commands) {
    bool all_success = true;
    for (const auto& cmd : commands) {
        if (!SetServoAngle(cmd.first, cmd.second)) {
            all_success = false;
        }
        // Небольшая задержка между командами
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return all_success;
}

bool ServoController::SetPose(const std::string& pose_name) {
    ESP_LOGI(TAG, "Установка позы: %s", pose_name.c_str());

    if (pose_name == "home" || pose_name == "reset") {
        std::vector<std::pair<int, int>> home_pose = {
            {SERVO_HEAD_YAW_NUM, SERVO_HOME_ANGLE},
            {SERVO_HEAD_PITCH_NUM, SERVO_HOME_ANGLE},
        };
        return SetMultipleServos(home_pose);
    }
    else if (pose_name == "left" || pose_name == "look_left") {
        return SetServoAngle(SERVO_HEAD_YAW_NUM, 45);
    }
    else if (pose_name == "right" || pose_name == "look_right") {
        return SetServoAngle(SERVO_HEAD_YAW_NUM, 120);
    }
    else if (pose_name == "up" || pose_name == "look_up") {
        return SetServoAngle(SERVO_HEAD_PITCH_NUM, 60);
    }
    else if (pose_name == "down" || pose_name == "look_down") {
        return SetServoAngle(SERVO_HEAD_PITCH_NUM, 120);
    }
    else if (pose_name == "nod" || pose_name == "nodding") {
        SetServoAngle(SERVO_HEAD_PITCH_NUM, 60);
        vTaskDelay(pdMS_TO_TICKS(200));
        SetServoAngle(SERVO_HEAD_PITCH_NUM, 120);
        vTaskDelay(pdMS_TO_TICKS(200));
        SetServoAngle(SERVO_HEAD_PITCH_NUM, 70);
        vTaskDelay(pdMS_TO_TICKS(200));
        SetServoAngle(SERVO_HEAD_PITCH_NUM, SERVO_HOME_ANGLE);
        return true;
    }
    else if (pose_name == "shake" || pose_name == "swing" || pose_name == "покачивание") {
        SetServoAngle(SERVO_HEAD_YAW_NUM, 55);
        vTaskDelay(pdMS_TO_TICKS(180));
        SetServoAngle(SERVO_HEAD_YAW_NUM, 120);
        vTaskDelay(pdMS_TO_TICKS(180));
        SetServoAngle(SERVO_HEAD_YAW_NUM, 65);
        vTaskDelay(pdMS_TO_TICKS(180));
        SetServoAngle(SERVO_HEAD_YAW_NUM, SERVO_HOME_ANGLE);
        return true;
    }
    else if (pose_name == "dance" || pose_name == "dancing") {
        SetServoAngle(SERVO_HEAD_YAW_NUM, 45);
        SetServoAngle(SERVO_HEAD_PITCH_NUM, 120);
        vTaskDelay(pdMS_TO_TICKS(300));
        SetServoAngle(SERVO_HEAD_YAW_NUM, 120);
        SetServoAngle(SERVO_HEAD_PITCH_NUM, 60);
        vTaskDelay(pdMS_TO_TICKS(300));
        SetPose("home");
        return true;
    }
    else if (pose_name == "greet" || pose_name == "greeting") {
        SetPose("nod");
        return true;
    }
    else if (pose_name == "sad" || pose_name == "sad_pose") {
        return SetServoAngle(SERVO_HEAD_PITCH_NUM, 120);
    }
    else if (pose_name == "happy" || pose_name == "happy_pose") {
        return SetServoAngle(SERVO_HEAD_PITCH_NUM, 70);
    }
    else {
        ESP_LOGW(TAG, "Неизвестная поза: %s", pose_name.c_str());
        return false;
    }
}
