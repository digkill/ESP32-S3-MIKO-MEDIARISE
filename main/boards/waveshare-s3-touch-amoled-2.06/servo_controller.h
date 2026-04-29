#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <string>
#include <mutex>
#include <utility>
#include <vector>

/**
 * @brief Управление двумя сервоприводами головы через LEDC PWM.
 */
class ServoController {
public:
    ServoController();
    ~ServoController();

    /**
     * @brief Инициализирует PWM-каналы сервоприводов
     * @return true если успешно
     */
    bool Init();

    /**
     * @brief Устанавливает угол для сервопривода
     * @param servo_num Номер сервопривода (1, 2, 3, ...)
     * @param angle Угол поворота (0-180)
     * @return true если команда отправлена успешно
     */
    bool SetServoAngle(int servo_num, int angle);

    /**
     * @brief Устанавливает несколько сервоприводов одновременно
     * @param commands Массив пар {servo_num, angle}
     * @return true если все команды отправлены успешно
     */
    bool SetMultipleServos(const std::vector<std::pair<int, int>>& commands);

    /**
     * @brief Выполняет предустановленную позу робота
     * @param pose_name Имя позы ("home", "wave", "dance", etc.)
     * @return true если поза выполнена
     */
    bool SetPose(const std::string& pose_name);

private:
    bool initialized_;
    std::mutex servo_mutex_;

    bool ConfigureChannel(int servo_num, gpio_num_t pin, ledc_channel_t channel);
    bool WriteAngle(ledc_channel_t channel, int angle);
    bool GetChannelForServo(int servo_num, ledc_channel_t& channel) const;
    uint32_t AngleToDuty(int angle) const;
};

#endif // SERVO_CONTROLLER_H


