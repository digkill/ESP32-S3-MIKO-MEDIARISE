#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include <driver/uart.h>
#include <string>
#include <mutex>
#include <utility>
#include <vector>

/**
 * @brief Класс для управления сервоприводами через UART
 * 
 * Отправляет команды вида "S1:45" (сервопривод 1, угол 45) 
 * на мастер-контроллер через UART
 */
class ServoController {
public:
    ServoController();
    ~ServoController();

    /**
     * @brief Инициализирует UART для связи с мастер-контроллером
     * @return true если успешно
     */
    bool Init();

    /**
     * @brief Устанавливает угол для сервопривода
     * @param servo_num Номер сервопривода (1, 2, 3, ...)
     * @param angle Угол поворота; команда ограничивается диапазоном 70-110
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
    uart_port_t uart_port_;
    bool initialized_;
    std::mutex uart_mutex_;

    /**
     * @brief Отправляет команду через UART
     * @param command Строка команды (например, "S1:45")
     * @return true если отправлено успешно
     */
    bool SendCommand(const std::string& command);
};

#endif // SERVO_CONTROLLER_H


