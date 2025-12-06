/**
 * @file simple_display.h
 * @brief Простое управление дисплеем GC9A01 без LVGL
 * 
 * Модуль для прямой отрисовки на дисплее через esp_lcd API
 */

#ifndef SIMPLE_DISPLAY_H
#define SIMPLE_DISPLAY_H

#include <esp_lcd_panel_ops.h>
#include <driver/gpio.h>
#include <stdint.h>

// Цвета (RGB565) - определены как константы для использования
static const uint16_t COLOR_BG = 0x2104;          // Темно-синий (10, 10, 18)
static const uint16_t COLOR_BODY = 0x2108;        // Темно-серый (40, 48, 64)
static const uint16_t COLOR_SCREEN = 0x0000;      // Черный (0, 0, 0)

// Геометрия робота
#define ROBOT_CX 120  // Центр X
#define ROBOT_CY 120  // Центр Y
#define HEAD_W 170
#define HEAD_H 150
#define HEAD_R 40
#define FACE_W 130
#define FACE_H 100
#define FACE_R 30
#define EYE_W 48
#define EYE_H 56
#define EYE_R 12
#define EYE_SPACING 18

class SimpleDisplay {
private:
    esp_lcd_panel_handle_t panel_;
    int width_;
    int height_;
    uint16_t* buffer_;
    
    // Цвета глаз для анимации
    uint16_t eye_colors_[3];
    uint8_t eye_color_index_;
    int64_t last_eye_change_us_;

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
     * @brief Обновляет анимацию глаз в режиме IDLE
     */
    void UpdateIdleEyes();
    
    /**
     * @brief Обновляет дисплей (отправляет буфер на экран)
     */
    void Update();
    
    /**
     * @brief Конвертирует RGB в RGB565
     */
    static uint16_t Color565(uint8_t r, uint8_t g, uint8_t b);
};

#endif // SIMPLE_DISPLAY_H

