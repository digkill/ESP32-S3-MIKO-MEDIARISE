/**
 * @file simple_display.cc
 * @brief Реализация простого управления дисплеем
 */

#include "simple_display.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <string.h>
#include <stdlib.h>

#define TAG "SimpleDisplay"

SimpleDisplay::SimpleDisplay(esp_lcd_panel_handle_t panel, int width, int height)
    : panel_(panel), width_(width), height_(height), buffer_(nullptr),
      eye_color_index_(0), last_eye_change_us_(0) {
    
    // Инициализация цветов глаз (RGB565)
    eye_colors_[0] = SimpleDisplay::Color565(255, 240, 90);   // Желтый
    eye_colors_[1] = SimpleDisplay::Color565(140, 255, 140);  // Зеленый
    eye_colors_[2] = SimpleDisplay::Color565(255, 170, 70);  // Оранжевый
}

SimpleDisplay::~SimpleDisplay() {
    if (buffer_) {
        free(buffer_);
        buffer_ = nullptr;
    }
}

bool SimpleDisplay::Init() {
    ESP_LOGI(TAG, "Инициализация SimpleDisplay %dx%d", width_, height_);
    
    // Выделяем буфер для одной строки
    buffer_ = (uint16_t*)malloc(width_ * sizeof(uint16_t));
    if (!buffer_) {
        ESP_LOGE(TAG, "Ошибка выделения памяти для буфера");
        return false;
    }
    
    ESP_LOGI(TAG, "SimpleDisplay инициализирован");
    return true;
}

uint16_t SimpleDisplay::Color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void SimpleDisplay::FillScreen(uint16_t color) {
    // Заполняем буфер цветом
    for (int i = 0; i < width_; i++) {
        buffer_[i] = color;
    }
    
    // Отправляем на экран построчно
    for (int y = 0; y < height_; y++) {
        esp_lcd_panel_draw_bitmap(panel_, 0, y, width_, y + 1, buffer_);
    }
}

void SimpleDisplay::FillRoundRect(int x, int y, int w, int h, int r, uint16_t color) {
    // Упрощенная версия: рисуем обычный прямоугольник
    // Для полной поддержки закругленных углов нужна более сложная логика
    
    // Проверка границ
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > width_) { w = width_ - x; }
    if (y + h > height_) { h = height_ - y; }
    
    if (w <= 0 || h <= 0) return;
    
    // Заполняем буфер цветом
    for (int i = 0; i < w; i++) {
        buffer_[i] = color;
    }
    
    // Отправляем на экран построчно
    for (int row = 0; row < h; row++) {
        int current_y = y + row;
        if (current_y >= 0 && current_y < height_) {
            esp_lcd_panel_draw_bitmap(panel_, x, current_y, x + w, current_y + 1, buffer_);
        }
    }
}

void SimpleDisplay::DrawRobotBase() {
    ESP_LOGI(TAG, "Рисование базовой структуры робота");
    
    // Очищаем экран (темно-синий фон)
    uint16_t bg_color = Color565(10, 10, 18);
    FillScreen(bg_color);
    
    // Голова (тёмно-серый корпус) - закомментировано как в оригинале
    // int headX = ROBOT_CX - HEAD_W / 2;
    // int headY = ROBOT_CY - HEAD_H / 2;
    // uint16_t body_color = Color565(40, 48, 64);
    // FillRoundRect(headX, headY, HEAD_W, HEAD_H, HEAD_R, body_color);
    
    // "Экран" лица (чёрный)
    int faceX = ROBOT_CX - FACE_W / 2;
    int faceY = ROBOT_CY - FACE_H / 2;
    FillRoundRect(faceX, faceY, FACE_W, FACE_H, FACE_R, COLOR_SCREEN);
    
    // Подбородок/утолщение снизу
    int headX = ROBOT_CX - HEAD_W / 2;
    int headY = ROBOT_CY - HEAD_H / 2;
    uint16_t body_color = Color565(40, 48, 64);
    FillRoundRect(headX + 20,
                  headY + HEAD_H - 30,
                  HEAD_W - 40,
                  24,
                  10,
                  body_color);
}

void SimpleDisplay::DrawEyes(uint16_t eye_color) {
    ESP_LOGI(TAG, "Рисование глаз, цвет: 0x%04X", eye_color);
    
    int faceY = ROBOT_CY - FACE_H / 2;
    int eyesCenterY = faceY + FACE_H / 2;
    
    int leftEyeX  = ROBOT_CX - EYE_W - EYE_SPACING / 2;
    int rightEyeX = ROBOT_CX + EYE_SPACING / 2;
    int eyeY      = eyesCenterY - EYE_H / 2;
    
    // Стираем старые глаза цветом экрана
    FillRoundRect(leftEyeX,  eyeY, EYE_W, EYE_H, EYE_R, COLOR_SCREEN);
    FillRoundRect(rightEyeX, eyeY, EYE_W, EYE_H, EYE_R, COLOR_SCREEN);
    
    // "Свечение" вокруг глаз (немного больше)
    FillRoundRect(leftEyeX  - 3,
                  eyeY      - 3,
                  EYE_W     + 6,
                  EYE_H     + 6,
                  EYE_R     + 4,
                  eye_color);
    FillRoundRect(rightEyeX - 3,
                  eyeY      - 3,
                  EYE_W     + 6,
                  EYE_H     + 6,
                  EYE_R     + 4,
                  eye_color);
    
    // Основные прямоугольники глаз
    FillRoundRect(leftEyeX,  eyeY, EYE_W, EYE_H, EYE_R, eye_color);
    FillRoundRect(rightEyeX, eyeY, EYE_W, EYE_H, EYE_R, eye_color);
}

void SimpleDisplay::UpdateIdleEyes() {
    int64_t now_us = esp_timer_get_time();
    if (now_us - last_eye_change_us_ > 700000) { // 700ms в микросекундах
        last_eye_change_us_ = now_us;
        eye_color_index_ = (eye_color_index_ + 1) % 3;
        ESP_LOGI(TAG, "IDLE: смена цвета глаз, индекс: %d", eye_color_index_);
        DrawEyes(eye_colors_[eye_color_index_]);
    }
}

void SimpleDisplay::Update() {
    // Для esp_lcd отрисовка происходит сразу в DrawBitmap
    // Эта функция может использоваться для принудительного обновления если нужно
}

