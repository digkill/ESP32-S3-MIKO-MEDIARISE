#include "servo_controller.h"
#include <esp_log.h>
#include <driver/uart.h>
#include <driver/gpio.h>
#include <sstream>
#include <algorithm>
#include "config.h"

#define TAG "ServoController"

ServoController::ServoController() 
    : uart_port_(SERVO_UART_PORT_NUM), initialized_(false) {
}

ServoController::~ServoController() {
    if (initialized_) {
        uart_driver_delete(uart_port_);
        initialized_ = false;
    }
}

bool ServoController::Init() {
    if (initialized_) {
        ESP_LOGW(TAG, "ServoController уже инициализирован");
        return true;
    }

    ESP_LOGI(TAG, "Инициализация UART для управления сервоприводами");
    ESP_LOGI(TAG, "  Port: UART_NUM_%d", uart_port_);
    ESP_LOGI(TAG, "  Baud: %d", SERVO_UART_BAUD_RATE);
    ESP_LOGI(TAG, "  TX Pin: GPIO%d", SERVO_UART_TX_PIN);
    ESP_LOGI(TAG, "  RX Pin: GPIO%d", SERVO_UART_RX_PIN);

    // Конфигурация UART
    uart_config_t uart_config = {
        .baud_rate = SERVO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    // Установка параметров UART
    esp_err_t ret = uart_param_config(uart_port_, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка настройки параметров UART: %s", esp_err_to_name(ret));
        return false;
    }

    // Установка пинов UART
    ret = uart_set_pin(uart_port_, SERVO_UART_TX_PIN, SERVO_UART_RX_PIN, 
                       UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка настройки пинов UART: %s", esp_err_to_name(ret));
        return false;
    }

    // Установка драйвера UART (буфер только для отправки, приём не нужен)
    ret = uart_driver_install(uart_port_, 1024, 0, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Ошибка установки драйвера UART: %s", esp_err_to_name(ret));
        return false;
    }

    initialized_ = true;
    ESP_LOGI(TAG, "UART для сервоприводов инициализирован успешно");

    // Отправка тестовой команды для проверки связи
    vTaskDelay(pdMS_TO_TICKS(100));
    SetServoAngle(1, 90); // Установить сервопривод 1 в среднее положение

    return true;
}

bool ServoController::SendCommand(const std::string& command) {
    if (!initialized_) {
        ESP_LOGE(TAG, "ServoController не инициализирован");
        return false;
    }

    std::lock_guard<std::mutex> lock(uart_mutex_);

    // Добавляем символ новой строки для команды
    std::string cmd_with_newline = command + "\n";
    
    int len = uart_write_bytes(uart_port_, cmd_with_newline.c_str(), cmd_with_newline.length());
    if (len < 0) {
        ESP_LOGE(TAG, "Ошибка отправки команды: %s", command.c_str());
        return false;
    }

    ESP_LOGD(TAG, "Отправлена команда: %s", command.c_str());
    return true;
}

bool ServoController::SetServoAngle(int servo_num, int angle) {
    // Проверка параметров
    if (servo_num < 1 || servo_num > 10) {
        ESP_LOGE(TAG, "Неверный номер сервопривода: %d (допустимо 1-10)", servo_num);
        return false;
    }

    // Ограничение угла
    angle = std::max(0, std::min(180, angle));

    // Формирование команды: "S1:45"
    std::ostringstream cmd;
    cmd << "S" << servo_num << ":" << angle;

    return SendCommand(cmd.str());
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
        // Домашняя поза - все сервоприводы в среднее положение
        std::vector<std::pair<int, int>> home_pose = {
            {1, 90}, {2, 90}, {3, 90}, {4, 90}
        };
        return SetMultipleServos(home_pose);
    }
    else if (pose_name == "wave" || pose_name == "wave_hand") {
        // Поза "махать рукой" - поднять одну руку и махать
        SetServoAngle(1, 45);  // Левая рука вверх
        vTaskDelay(pdMS_TO_TICKS(200));
        SetServoAngle(1, 135); // Мах вправо
        vTaskDelay(pdMS_TO_TICKS(200));
        SetServoAngle(1, 45);  // Мах влево
        vTaskDelay(pdMS_TO_TICKS(200));
        SetServoAngle(1, 90);  // Вернуть в центр
        return true;
    }
    else if (pose_name == "dance" || pose_name == "dancing") {
        // Танец - последовательность движений
        SetServoAngle(1, 45);
        SetServoAngle(2, 135);
        vTaskDelay(pdMS_TO_TICKS(300));
        SetServoAngle(1, 135);
        SetServoAngle(2, 45);
        vTaskDelay(pdMS_TO_TICKS(300));
        SetServoAngle(1, 90);
        SetServoAngle(2, 90);
        return true;
    }
    else if (pose_name == "greet" || pose_name == "greeting") {
        // Приветствие - поднять обе руки
        SetServoAngle(1, 60);
        SetServoAngle(2, 120);
        vTaskDelay(pdMS_TO_TICKS(500));
        SetServoAngle(1, 90);
        SetServoAngle(2, 90);
        return true;
    }
    else if (pose_name == "sad" || pose_name == "sad_pose") {
        // Грустная поза - опустить руки
        SetServoAngle(1, 150);
        SetServoAngle(2, 30);
        return true;
    }
    else if (pose_name == "happy" || pose_name == "happy_pose") {
        // Радостная поза - поднять руки вверх
        SetServoAngle(1, 30);
        SetServoAngle(2, 150);
        return true;
    }
    else {
        ESP_LOGW(TAG, "Неизвестная поза: %s", pose_name.c_str());
        return false;
    }
}




