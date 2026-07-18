#pragma once

#include <lvgl.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdint.h>
#include <mutex>

class CatDisplay {
public:
    enum class State {
        IDLE,
        CONNECTING,
        LISTENING,
        SPEAKING,
        HAPPY,
        SAD,
        ANGRY,
        SHY,
        LOVE,
        COFFEE,
        DANCE,
        CURIOUS,
        CELEBRATE,
        DIZZY,
        SLEEPING,
    };

    CatDisplay(int width, int height, lv_obj_t* canvas, float presentation_scale = 1.0f);
    ~CatDisplay();

    bool Init();

    void SetState(State state);
    State GetState() const;
    void PlayDizzy(uint32_t duration_ms = 3600);

    void SetStateFromStatus(const char* status);
    void SetStateFromEmotion(const char* emotion);
    void SpawnTouchBubbles(int raw_x, int raw_y);
    void AddTrailPoint(int raw_x, int raw_y);
    void SetBatteryStatus(int level, bool charging);

    void Update();
    void FillScreen(uint16_t color);
    void Redraw();

    static uint16_t Color565(uint8_t r, uint8_t g, uint8_t b);

    struct EmotionProfile {
        uint16_t accent;
        uint16_t iris;
        float eye_scale;
        int brow_lift;
        int left_tilt;
        int right_tilt;
        float whisker_bias;
        bool blush;
        bool sparkle;
        bool heart_eyes;
        enum class Mouth {
            Smile,
            Grin,
            Flat,
            Frown,
            Ponder,
            Shy,
        } mouth;
    };

private:
    static void AnimationTimerCallback(void* arg);
    static void AnimationTask(void* arg);

    int S(float value) const;
    int U(float value) const;
    int X(float value) const;
    int Y(float value) const;

    void SetPixel(int x, int y, uint16_t color);
    void FillRect(int x, int y, int w, int h, uint16_t color);
    void FillCircle(int cx, int cy, int radius, uint16_t color);
    void FillEllipse(int cx, int cy, int rx, int ry, uint16_t color);
    void FillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color);
    void DrawStroke(int x0, int y0, int x1, int y1, int thickness, uint16_t color);

    const EmotionProfile& CurrentProfile() const;
    void DrawStatusBar();
    void DrawSignalIcon(int x, int y, uint16_t color);
    void DrawBatteryIcon(int x, int y, uint16_t color);
    void DrawTinyDigit(int x, int y, int digit, uint16_t color);
    void DrawTinyColon(int x, int y, uint16_t color);
    void DrawClock(int cx, int y, uint16_t color);
    void DrawBackground();
    void DrawEyeRing(int cx, int cy, int radius, int thickness, float blink_ratio,
                     uint16_t accent, float eye_scale, bool heart_eyes);
    void DrawHeart(int cx, int cy, int size, uint16_t color);
    void DrawEyebrow(int cx, int cy, int half_width, int tilt, uint16_t color);
    void DrawNose(int cx, int cy, int size, uint16_t color);
    void DrawMouth(int cx, int cy, float open_ratio, const EmotionProfile& profile);
    void DrawWhiskers(int cx, int cy, uint16_t color, float spread, float bias);
    void DrawBlush(int cx, int cy, uint16_t color);
    void DrawSparkle(int cx, int cy, int size, uint16_t color);
    void DrawCoffeeCup(int cx, int cy, uint16_t color);
    void DrawDizzyOrbit(int face_bob, uint16_t color);
    void DrawCatEars(int face_bob, const EmotionProfile& profile);
    void DrawZzz(int cx, int cy, uint16_t color);
    void DrawBubble(int cx, int cy, int radius, uint16_t color);
    void FlushCanvas();

    int physical_width_;
    int physical_height_;
    int width_;
    int height_;
    bool rotate_cw_;
    float presentation_scale_;
    int status_bar_height_;
    float scale_;
    int origin_x_;
    int origin_y_;

    lv_obj_t* canvas_ = nullptr;
    uint16_t* canvas_buf_ = nullptr;  // direct pointer to pixel data (draw_buf->data)
    esp_timer_handle_t animation_timer_ = nullptr;
    TaskHandle_t animation_task_ = nullptr;
    bool animation_task_stop_ = false;
    mutable std::recursive_mutex state_mutex_;

    State state_;
    int64_t state_enter_us_;
    int64_t dizzy_until_us_ = 0;
    int64_t last_update_us_;

    float blink_t_;
    bool blink_closing_;
    int64_t next_blink_us_;
    bool needs_redraw_;

    float mouth_phase_;
    float mouth_open_;
    float pulse_phase_;
    float breath_phase_;
    float whisker_spread_;
    float ear_perk_;

    int battery_level_ = -1;
    bool battery_charging_ = false;

    struct Bubble {
        float x, y;
        float vx, vy;
        float radius;
        float alpha;
        bool active = false;
    };
    static constexpr int kMaxBubbles = 6;
    Bubble bubbles_[kMaxBubbles] = {};
    uint32_t bubble_seed_ = 0x5EED1234;

    struct TrailPoint {
        float x, y;
        float alpha;
        uint16_t color;
        bool active = false;
    };
    static constexpr int kMaxTrail = 64;
    TrailPoint trail_[kMaxTrail] = {};
    uint8_t trail_hue_ = 0;
};
