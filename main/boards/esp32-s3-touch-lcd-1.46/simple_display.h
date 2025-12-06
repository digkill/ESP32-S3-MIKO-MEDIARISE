/**
 * @file simple_display.h
 * @brief Простое управление дисплеем с анимацией глаз робота
 */

#ifndef SIMPLE_DISPLAY_H
#define SIMPLE_DISPLAY_H

#include <esp_lcd_panel_ops.h>
#include <driver/gpio.h>
#include <stdint.h>
#include <string>

// Цвета (RGB565)
static const uint16_t COLOR_BG = 0x0000;      // Черный фон
static const uint16_t COLOR_SCREEN = 0x0000;  // Черный экран

// Геометрия робота (для 320x240 после поворота)
#define ROBOT_CX 160  // Центр X
#define ROBOT_CY 120  // Центр Y
#define HEAD_W 200
#define HEAD_H 180
#define HEAD_R 50
#define FACE_W 160
#define FACE_H 120
#define FACE_R 40
#define EYE_WIDTH 50   // Ширина квадратных глаз
#define EYE_HEIGHT 55   // Высота квадратных глаз
#define EYE_ROUND_RADIUS 15  // Радиус скругления углов
#define EYE_SPACING 110  // Расстояние между глазами (еще больше отдалены)
#define MOUTH_WIDTH 80   // Ширина рта
#define MOUTH_HEIGHT 20  // Высота рта
#define MOUTH_Y 200      // Позиция рта по Y (внизу экрана)
#define STATUS_BAR_HEIGHT 20  // Высота статус-бара
#define STATUS_BAR_Y 0        // Позиция статус-бара (вверху)

// Состояния моргания
enum class BlinkState {
    OPEN,
    CLOSING,
    CLOSED,
    OPENING
};

class SimpleDisplay {
private:
    esp_lcd_panel_handle_t panel_;
    int width_;
    int height_;
    uint16_t* buffer_;
    
    // Цвет глаз (бирюзовый неоновый)
    uint16_t eye_color_;
    
    // Моргание
    BlinkState blink_state_;
    int64_t last_blink_time_us_;
    int blink_interval_ms_;  // Интервал между морганиями (2-5 секунд)
    int blink_duration_ms_;  // Длительность моргания (100-200ms)
    int blink_progress_;     // Прогресс моргания (0-100)
    
    // Эмоции
    std::string current_emotion_;
    int64_t emotion_start_time_us_;
    
    // Анимация рта
    bool is_speaking_;
    int64_t mouth_animation_time_us_;
    int mouth_frame_;
    
    // Статус-бар
    int64_t last_status_update_us_;
    
    // Вспомогательные функции
    void DrawCircle(int cx, int cy, int radius, uint16_t color);
    void FillCircle(int cx, int cy, int radius, uint16_t color);
    void DrawEye(int x, int y, int w, int h, int r, uint16_t color);
    void DrawMouth();
    void DrawStatusBar();
    void DrawSignalBars(int x, int y, int rssi);
    void DrawBattery(int x, int y, int level, bool charging);
    void DrawTime(int x, int y);

public:
    SimpleDisplay(esp_lcd_panel_handle_t panel, int width, int height);
    ~SimpleDisplay();
    
    /**
     * @brief Инициализирует дисплей и буфер
     * @return true если успешно
     */
    bool Init();
    
    /**
     * @brief Очищает экран указанным цветом
     */
    void FillScreen(uint16_t color);
    
    /**
     * @brief Рисует закругленный прямоугольник
     */
    void FillRoundRect(int x, int y, int w, int h, int r, uint16_t color);
    
    /**
     * @brief Рисует базовую структуру робота
     */
    void DrawRobotBase();
    
    /**
     * @brief Рисует глаза с указанным цветом
     */
    void DrawEyes(uint16_t eye_color);
    
    /**
     * @brief Обновляет анимацию глаз в зависимости от эмоции
     */
    void UpdateEyes();
    
    /**
     * @brief Устанавливает эмоцию
     */
    void SetEmotion(const char* emotion);
    
    /**
     * @brief Устанавливает состояние "говорит"
     */
    void SetSpeaking(bool speaking);
    
    /**
     * @brief Обновляет дисплей (отправляет буфер на экран)
     */
    void Update();
    
    /**
     * @brief Обновляет статус-бар
     */
    void UpdateStatusBar();
    
    /**
     * @brief Конвертирует RGB в RGB565
     */
    static uint16_t Color565(uint8_t r, uint8_t g, uint8_t b);
};

#endif // SIMPLE_DISPLAY_H

