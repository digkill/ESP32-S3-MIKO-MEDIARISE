/**
 * @file simple_display.cc
 * @brief Реализация простого управления дисплеем с анимацией глаз
 */

#include "simple_display.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <stdlib.h>
#include <cmath>
#include <time.h>
#include <sys/time.h>
#include "board.h"
#include <wifi_station.h>

#define TAG "SimpleDisplay"

namespace {
struct StarFrame {
    int x;
    int y;
};

constexpr StarFrame kDizzyFrames[3][3] = {
    {{-30, -6}, {0, 0}, {30, -6}},
    {{-24, 6}, {0, -10}, {24, 6}},
    {{-18, 0}, {0, 8}, {18, 0}},
};
constexpr int64_t kDizzyDurationUs = 3 * 1000000;
}

SimpleDisplay::SimpleDisplay(esp_lcd_panel_handle_t panel, int width, int height)
    : panel_(panel), width_(width), height_(height), buffer_(nullptr),
      eye_color_(Color565(255, 140, 0)), last_drawn_eye_color_(0), eyes_dirty_(true),
      blink_state_(BlinkState::OPEN), last_blink_time_us_(0),
      blink_interval_ms_(3000), blink_duration_ms_(150), blink_progress_(0),
      current_emotion_("neutral"), emotion_start_time_us_(0),
      is_speaking_(false), mouth_animation_time_us_(0), mouth_frame_(0),
      last_status_update_us_(0) {}

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
    
    // Рисуем базовую структуру робота
    DrawRobotBase();
    
    // Рисуем начальные глаза
    eyes_dirty_ = true;
    UpdateEyes();
    
    // Статус-бар рисуем позже, после инициализации системы (через UpdateStatusBar)
    // Инициализируем время последнего обновления так, чтобы статус-бар нарисовался через 1 секунду
    emotion_start_time_us_ = esp_timer_get_time();
    last_blink_time_us_ = esp_timer_get_time();
    last_status_update_us_ = esp_timer_get_time() - 2000000;  // Чтобы нарисовался сразу
    
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
    // Проверка границ
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > width_) { w = width_ - x; }
    if (y + h > height_) { h = height_ - y; }
    
    if (w <= 0 || h <= 0) return;
    
    // Ограничиваем радиус скругления
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    if (r < 0) r = 0;
    
    // Если радиус 0, рисуем обычный прямоугольник
    if (r == 0) {
        for (int i = 0; i < w; i++) {
            buffer_[i] = color;
        }
        for (int row = 0; row < h; row++) {
            int current_y = y + row;
            if (current_y >= 0 && current_y < height_) {
                esp_lcd_panel_draw_bitmap(panel_, x, current_y, x + w, current_y + 1, buffer_);
            }
        }
        return;
    }
    
    // Рисуем скругленный прямоугольник
    // Центры углов (X координаты)
    int cx1 = x + r;      // Левый верхний и нижний
    int cx2 = x + w - r;  // Правый верхний и нижний
    
    // Рисуем построчно
    for (int row = 0; row < h; row++) {
        int current_y = y + row;
        if (current_y < 0 || current_y >= height_) continue;
        
        int x_start = x;
        int x_end = x + w - 1;
        
        // Проверяем, находимся ли мы в области скругленных углов
        if (row < r) {
            // Верхние углы
            int dy = row - r;
            int dx = (int)sqrt(r * r - dy * dy);
            if (dx > 0) {
                x_start = cx1 - dx;  // Левый верхний угол
                x_end = cx2 + dx;    // Правый верхний угол
            }
        } else if (row >= h - r) {
            // Нижние углы
            int dy = (h - 1 - row) - r;
            int dx = (int)sqrt(r * r - dy * dy);
            if (dx > 0) {
                x_start = cx1 - dx;  // Левый нижний угол
                x_end = cx2 + dx;    // Правый нижний угол
            }
        }
        
        // Ограничиваем границы
        if (x_start < x) x_start = x;
        if (x_end >= x + w) x_end = x + w - 1;
        
        int line_w = x_end - x_start + 1;
        if (line_w > 0) {
            for (int i = 0; i < line_w; i++) {
                buffer_[i] = color;
            }
            esp_lcd_panel_draw_bitmap(panel_, x_start, current_y, x_end + 1, current_y + 1, buffer_);
        }
    }
}

void SimpleDisplay::FillCircle(int cx, int cy, int radius, uint16_t color) {
    // Заполняем круг построчно
    for (int y = -radius; y <= radius; y++) {
        int dy = y;
        int dx = (int)sqrt(radius * radius - dy * dy);
        if (dx > 0) {
            int x_start = cx - dx;
            int x_end = cx + dx;
            if (x_start < 0) x_start = 0;
            if (x_end >= width_) x_end = width_ - 1;
            
            int line_w = x_end - x_start + 1;
            if (line_w > 0) {
                for (int i = 0; i < line_w; i++) {
                    buffer_[i] = color;
                }
                int py = cy + y;
                if (py >= 0 && py < height_) {
                    esp_lcd_panel_draw_bitmap(panel_, x_start, py, x_end + 1, py + 1, buffer_);
                }
            }
        }
    }
}


void SimpleDisplay::DrawCircle(int cx, int cy, int radius, uint16_t color) {
    // Рисуем контур круга (используем FillCircle с небольшим радиусом)
    FillCircle(cx, cy, radius, color);
    FillCircle(cx, cy, radius - 1, COLOR_SCREEN);
}

void SimpleDisplay::DrawEye(int x, int y, int w, int h, int r, uint16_t color) {
    // Стираем старый глаз черным (увеличиваем область стирания для полного удаления скруглений)
    // Увеличиваем область стирания больше, чтобы убрать все артефакты скругления
    int erase_margin = r + 3;  // Больше чем радиус скругления
    FillRoundRect(x - erase_margin, y - erase_margin, w + erase_margin * 2, h + erase_margin * 2, 0, COLOR_SCREEN);
    
    // Рисуем основной глаз (квадратный со скруглением, без моргания)
    FillRoundRect(x, y, w, h, r, color);
}

void SimpleDisplay::DrawRobotBase() {
    ESP_LOGI(TAG, "Рисование базовой структуры робота");
    
    // Очищаем экран черным фоном
    FillScreen(COLOR_BG);
    
    // "Экран" лица (чёрный)
    int faceX = ROBOT_CX - FACE_W / 2;
    int faceY = ROBOT_CY - FACE_H / 2;
    FillRoundRect(faceX, faceY, FACE_W, FACE_H, FACE_R, COLOR_SCREEN);
}

void SimpleDisplay::DrawEyes(uint16_t eye_color) {
    int faceY = ROBOT_CY - FACE_H / 2;
    int eyesCenterY = faceY + FACE_H / 2;
    
    int leftEyeX  = ROBOT_CX - EYE_SPACING / 2 - EYE_WIDTH / 2;
    int rightEyeX = ROBOT_CX + EYE_SPACING / 2 - EYE_WIDTH / 2;
    int eyeY = eyesCenterY - EYE_HEIGHT / 2;
    
    // Рисуем квадратные глаза со скруглением (без моргания)
    DrawEye(leftEyeX, eyeY, EYE_WIDTH, EYE_HEIGHT, EYE_ROUND_RADIUS, eye_color);
    DrawEye(rightEyeX, eyeY, EYE_WIDTH, EYE_HEIGHT, EYE_ROUND_RADIUS, eye_color);
}

void SimpleDisplay::SetEmotion(const char* emotion) {
    if (emotion != nullptr) {
        current_emotion_ = std::string(emotion);
        emotion_start_time_us_ = esp_timer_get_time();
        eyes_dirty_ = true;
        ESP_LOGI(TAG, "Установлена эмоция: %s", emotion);
    }
}

void SimpleDisplay::SetSpeaking(bool speaking) {
    if (is_speaking_ != speaking) {
        is_speaking_ = speaking;
        if (speaking) {
            mouth_animation_time_us_ = esp_timer_get_time();
            mouth_frame_ = 0;
        } else {
            // Стираем рот когда перестает говорить
            int mouthX = ROBOT_CX - MOUTH_WIDTH / 2;
            FillRoundRect(mouthX - 2, MOUTH_Y - 2, MOUTH_WIDTH + 4, MOUTH_HEIGHT + 4, 10, COLOR_SCREEN);
        }
        ESP_LOGI(TAG, "SetSpeaking: %s", speaking ? "true" : "false");
    }
}

void SimpleDisplay::DrawMouth() {
    if (!is_speaking_) {
        return;
    }
    
    int64_t now_us = esp_timer_get_time();
    int64_t elapsed_ms = (now_us - mouth_animation_time_us_) / 1000;
    
    // Анимация рта: меняем форму каждые 100ms для эффекта речи
    mouth_frame_ = (elapsed_ms / 100) % 4;
    
    int mouthX = ROBOT_CX - MOUTH_WIDTH / 2;
    uint16_t mouth_color = eye_color_;  // Используем тот же цвет что и глаза
    
    // Стираем старый рот
    FillRoundRect(mouthX - 2, MOUTH_Y - 2, MOUTH_WIDTH + 4, MOUTH_HEIGHT + 4, 10, COLOR_SCREEN);
    
    // Рисуем рот в зависимости от кадра анимации
    int mouth_h = MOUTH_HEIGHT;
    int mouth_w = MOUTH_WIDTH;
    int mouth_y = MOUTH_Y;
    
    switch (mouth_frame_) {
        case 0:  // Закрыт
            mouth_h = 4;
            mouth_y = MOUTH_Y + (MOUTH_HEIGHT - 4) / 2;
            break;
        case 1:  // Полуоткрыт
            mouth_h = MOUTH_HEIGHT / 2;
            mouth_y = MOUTH_Y + MOUTH_HEIGHT / 4;
            break;
        case 2:  // Открыт
            mouth_h = MOUTH_HEIGHT;
            mouth_y = MOUTH_Y;
            break;
        case 3:  // Полуоткрыт (другой вариант)
            mouth_h = MOUTH_HEIGHT * 3 / 4;
            mouth_y = MOUTH_Y + MOUTH_HEIGHT / 8;
            break;
    }
    
    // Рисуем рот со скруглением
    FillRoundRect(mouthX, mouth_y, mouth_w, mouth_h, 10, mouth_color);
}

void SimpleDisplay::UpdateEyes() {
    if (!eyes_dirty_ && last_drawn_eye_color_ == eye_color_) {
        return;  // Уже нарисованы и цвет не менялся
    }

    DrawEyes(eye_color_);
    last_drawn_eye_color_ = eye_color_;
    eyes_dirty_ = false;
}

void SimpleDisplay::DrawSignalBars(int x, int y, int rssi) {
    // Определяем количество полосок сигнала на основе RSSI
    int bars = 0;
    if (rssi >= -60) {
        bars = 4;  // Отличный сигнал
    } else if (rssi >= -70) {
        bars = 3;  // Хороший сигнал
    } else if (rssi >= -80) {
        bars = 2;  // Средний сигнал
    } else if (rssi >= -90) {
        bars = 1;  // Слабый сигнал
    } else {
        bars = 0;  // Нет сигнала
    }
    
    // Рисуем антенку (вертикальная линия)
    int antenna_x = x;
    int antenna_y = y;
    int antenna_h = 12;
    FillRoundRect(antenna_x, antenna_y, 2, antenna_h, 1, eye_color_);
    
    // Рисуем полоски сигнала справа от антенки
    int bar_x = x + 4;
    int bar_spacing = 3;
    int bar_w = 2;
    uint16_t bar_color = eye_color_;
    
    for (int i = 0; i < 4; i++) {
        int bar_h = 3 + i * 2;  // Высота увеличивается: 3, 5, 7, 9
        int bar_y = antenna_y + antenna_h - bar_h;
        if (i < bars) {
            FillRoundRect(bar_x + i * (bar_w + bar_spacing), bar_y, bar_w, bar_h, 1, bar_color);
        } else {
            // Стираем неактивные полоски
            FillRoundRect(bar_x + i * (bar_w + bar_spacing), bar_y, bar_w, bar_h, 1, COLOR_SCREEN);
        }
    }
}

void SimpleDisplay::DrawBattery(int x, int y, int level, bool charging) {
    // Рисуем контур батареи
    int battery_w = 20;
    int battery_h = 10;
    uint16_t battery_color = eye_color_;
    
    // Основной корпус батареи
    FillRoundRect(x, y, battery_w, battery_h, 2, COLOR_SCREEN);  // Стираем
    // Контур
    for (int i = 0; i < battery_w; i++) {
        for (int j = 0; j < battery_h; j++) {
            if (i == 0 || i == battery_w - 1 || j == 0 || j == battery_h - 1) {
                int px = x + i;
                int py = y + j;
                if (px >= 0 && px < width_ && py >= 0 && py < height_) {
                    buffer_[i] = battery_color;
                    esp_lcd_panel_draw_bitmap(panel_, px, py, px + 1, py + 1, &buffer_[i]);
                }
            }
        }
    }
    
    // Положительный контакт (справа)
    FillRoundRect(x + battery_w, y + 2, 2, 6, 1, battery_color);
    
    // Заливка батареи (ячейки)
    if (charging) {
        // При зарядке - полная заливка
        FillRoundRect(x + 1, y + 1, battery_w - 2, battery_h - 2, 1, battery_color);
    } else {
        // Обычная заливка в зависимости от уровня
        int fill_w = (battery_w - 2) * level / 100;
        if (fill_w > 0) {
            FillRoundRect(x + 1, y + 1, fill_w, battery_h - 2, 1, battery_color);
        }
    }
}

void SimpleDisplay::DrawTime(int x, int y) {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    
    // Форматируем время как HH:MM
    int hour = timeinfo.tm_hour;
    int minute = timeinfo.tm_min;
    
    // Рисуем время простыми сегментами (7-сегментный индикатор упрощенный)
    const int digit_scale = 3;  // Увеличиваем цифры в 3 раза
    const int digit_w = 5 * digit_scale;
    const int digit_h = 8 * digit_scale;
    const int spacing = 2 * digit_scale;
    const int horizontal_thickness = 2 * digit_scale;
    const int vertical_width = 2 * digit_scale;
    const int vertical_height = 3 * digit_scale;
    const int lower_vertical_offset = 5 * digit_scale;
    const int middle_offset = 3 * digit_scale;
    const int colon_size = 2 * digit_scale;
    const int colon_top_offset = 2 * digit_scale;
    const int colon_bottom_offset = 5 * digit_scale;
    const int colon_shift = digit_scale;
    const int clear_w = 5 * (digit_w + spacing);
    const int clear_h = digit_h + 2 * digit_scale;

    // Стираем область времени (увеличена для больших цифр)
    FillRoundRect(x, y, clear_w, clear_h, 0, COLOR_SCREEN);
    
    // Функция для рисования одной цифры (0-9)
    auto drawDigit = [this, x, y, digit_w, digit_h, spacing, horizontal_thickness,
                      vertical_width, vertical_height, lower_vertical_offset, middle_offset]
                     (int digit, int offset) {
        uint16_t color = eye_color_;
        int base_x = x + offset * (digit_w + spacing);
        int base_y = y + 1;
        
        // Упрощенный 7-сегментный дисплей
        // Сегменты: a(верх), b(правый верх), c(правый низ), d(низ), e(левый низ), f(левый верх), g(середина)
        bool segments[7] = {false};
        
        switch (digit) {
            case 0: segments[0]=1; segments[1]=1; segments[2]=1; segments[3]=1; segments[4]=1; segments[5]=1; break;
            case 1: segments[1]=1; segments[2]=1; break;
            case 2: segments[0]=1; segments[1]=1; segments[6]=1; segments[4]=1; segments[3]=1; break;
            case 3: segments[0]=1; segments[1]=1; segments[2]=1; segments[3]=1; segments[6]=1; break;
            case 4: segments[5]=1; segments[6]=1; segments[1]=1; segments[2]=1; break;
            case 5: segments[0]=1; segments[5]=1; segments[6]=1; segments[2]=1; segments[3]=1; break;
            case 6: segments[0]=1; segments[5]=1; segments[6]=1; segments[2]=1; segments[3]=1; segments[4]=1; break;
            case 7: segments[0]=1; segments[1]=1; segments[2]=1; break;
            case 8: for(int i=0; i<7; i++) segments[i]=1; break;
            case 9: segments[0]=1; segments[1]=1; segments[2]=1; segments[3]=1; segments[5]=1; segments[6]=1; break;
        }
        
        // Рисуем сегменты (увеличенные для больших цифр)
        if (segments[0]) FillRoundRect(base_x, base_y, digit_w, horizontal_thickness, 0, color); // a
        if (segments[1]) FillRoundRect(base_x + digit_w - vertical_width, base_y, vertical_width, vertical_height, 0, color); // b
        if (segments[2]) FillRoundRect(base_x + digit_w - vertical_width, base_y + lower_vertical_offset, vertical_width, vertical_height, 0, color); // c
        if (segments[3]) FillRoundRect(base_x, base_y + digit_h - horizontal_thickness, digit_w, horizontal_thickness, 0, color); // d
        if (segments[4]) FillRoundRect(base_x, base_y + lower_vertical_offset, vertical_width, vertical_height, 0, color); // e
        if (segments[5]) FillRoundRect(base_x, base_y, vertical_width, vertical_height, 0, color); // f
        if (segments[6]) FillRoundRect(base_x, base_y + middle_offset, digit_w, horizontal_thickness, 0, color); // g
    };
    
    // Рисуем часы и минуты
    drawDigit(hour / 10, 0);
    drawDigit(hour % 10, 1);
    // Двоеточие (увеличенное)
    int colon_x = x + 2 * (digit_w + spacing) + colon_shift;
    FillRoundRect(colon_x, y + colon_top_offset, colon_size, colon_size, colon_size / 2, eye_color_);
    FillRoundRect(colon_x, y + colon_bottom_offset, colon_size, colon_size, colon_size / 2, eye_color_);
    drawDigit(minute / 10, 3);
    drawDigit(minute % 10, 4);
}

void SimpleDisplay::DrawStatusBar() {
    // Очищаем только область статус-бара (не весь экран!)
    FillRoundRect(0, STATUS_BAR_Y, width_, STATUS_BAR_HEIGHT, 0, COLOR_SCREEN);
    
    // Получаем информацию безопасно
    int rssi = -100;  // По умолчанию нет сигнала
    int battery_level = 100;
    bool charging = false;
    bool discharging = false;
    
    // Пытаемся получить реальные данные, но не падаем если не получается
    // WiFi сигнал - может быть не готов на ранних этапах
    // Батарея
    auto& board = Board::GetInstance();
    board.GetBatteryLevel(battery_level, charging, discharging);
    
    // Рисуем элементы статус-бара
    DrawSignalBars(5, STATUS_BAR_Y + 4, rssi);
    const int clock_half_width = 53;  // Половина ширины часов после масштабирования
    DrawTime(width_ / 2 - clock_half_width, STATUS_BAR_Y + 3);
    DrawBattery(width_ - 25, STATUS_BAR_Y + 5, battery_level, charging);
}

void SimpleDisplay::UpdateStatusBar() {
    int64_t now_us = esp_timer_get_time();
    // Обновляем статус-бар каждые 2 секунды
    if (now_us - last_status_update_us_ > 2000000) {
        last_status_update_us_ = now_us;
        
        // Безопасно получаем данные
        int rssi = -100;
        int battery_level = 100;
        bool charging = false;
        bool discharging = false;
        
        // Пытаемся получить WiFi RSSI
        // (может быть не готов на ранних этапах, поэтому просто используем значение по умолчанию)
        
        // Пытаемся получить уровень батареи
        auto& board = Board::GetInstance();
        board.GetBatteryLevel(battery_level, charging, discharging);
        
        // Очищаем только область статус-бара
        FillRoundRect(0, STATUS_BAR_Y, width_, STATUS_BAR_HEIGHT, 0, COLOR_SCREEN);
        
        // Рисуем элементы
        DrawSignalBars(5, STATUS_BAR_Y + 4, rssi);
        const int clock_half_width = 53;
        DrawTime(width_ / 2 - clock_half_width, STATUS_BAR_Y + 3);
        DrawBattery(width_ - 25, STATUS_BAR_Y + 5, battery_level, charging);
    }
}

void SimpleDisplay::Update() {
    UpdateEyes();
    DrawMouth();  // Обновляем анимацию рта
    DrawDizzyEffect();
    // Статус-бар обновляется отдельно через UpdateStatusBar(), не здесь
}

void SimpleDisplay::TriggerDizzyEffect() {
    dizzy_active_ = true;
    dizzy_start_time_us_ = esp_timer_get_time();
    last_dizzy_draw_time_us_ = 0;
    dizzy_frame_ = 0;
    ClearDizzyArea();
}

void SimpleDisplay::ClearDizzyArea() {
    int faceY = ROBOT_CY - FACE_H / 2;
    int eyesCenterY = faceY + FACE_H / 2;
    int area_w = 100;
    int area_h = 50;
    int area_x = ROBOT_CX - area_w / 2;
    int area_y = eyesCenterY - area_h / 2;
    FillRoundRect(area_x, area_y, area_w, area_h, 10, COLOR_SCREEN);
}

void SimpleDisplay::DrawDizzyEffect() {
    if (!dizzy_active_) {
        return;
    }
    int64_t now = esp_timer_get_time();
    if (now - dizzy_start_time_us_ > kDizzyDurationUs) {
        ClearDizzyArea();
        dizzy_active_ = false;
        return;
    }
    if (now - last_dizzy_draw_time_us_ < 120000) {
        return;
    }
    last_dizzy_draw_time_us_ = now;
    ClearDizzyArea();

    int faceY = ROBOT_CY - FACE_H / 2;
    int eyesCenterY = faceY + FACE_H / 2;
    int baseX = ROBOT_CX;
    int frame = dizzy_frame_ % 3;
    for (const auto& star : kDizzyFrames[frame]) {
        int cx = baseX + star.x;
        int cy = eyesCenterY + star.y;
        FillRoundRect(cx - 2, cy - 10, 4, 20, 1, eye_color_);
        FillRoundRect(cx - 10, cy - 2, 20, 4, 1, eye_color_);
        FillCircle(cx, cy, 3, Color565(255, 255, 255));
    }
    dizzy_frame_++;
}
