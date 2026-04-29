#include "cat_display.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <wifi_station.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <cstring>

#define TAG "CatDisplay"

static constexpr int kBaseW = 240;
static constexpr int kBaseH = 240;
static constexpr int kBaseCx = 120;
static constexpr int kBaseCy = 115;

static const uint16_t C_BG = CatDisplay::Color565(5, 5, 8);
static const uint16_t C_PANEL = CatDisplay::Color565(9, 13, 22);
static const uint16_t C_TEXT = CatDisplay::Color565(180, 225, 255);
static const uint16_t C_GLOW = CatDisplay::Color565(100, 200, 255);
static const uint16_t C_IRIS = CatDisplay::Color565(24, 70, 110);
static const uint16_t C_PINK = CatDisplay::Color565(255, 120, 180);
static const uint16_t C_RED = CatDisplay::Color565(255, 90, 90);
static const uint16_t C_GOLD = CatDisplay::Color565(255, 210, 90);
static const uint16_t C_LAVENDER = CatDisplay::Color565(210, 160, 255);
static const uint16_t C_SOFT_BLUE = CatDisplay::Color565(140, 190, 255);
static const uint16_t C_GREEN = CatDisplay::Color565(170, 255, 140);

static const CatDisplay::EmotionProfile kHappyProfile = {
    C_GLOW, C_IRIS, 0.94f, -5, 0, 0, 0.12f, false, false, false,
    CatDisplay::EmotionProfile::Mouth::Smile,
};
static const CatDisplay::EmotionProfile kSpeakingProfile = {
    CatDisplay::Color565(110, 220, 255), CatDisplay::Color565(40, 110, 150),
    0.88f, -4, -1, 1, 0.20f, false, false, false,
    CatDisplay::EmotionProfile::Mouth::Grin,
};
static const CatDisplay::EmotionProfile kListeningProfile = {
    C_SOFT_BLUE, CatDisplay::Color565(50, 90, 140), 0.86f, -2, 0, 1, 0.04f,
    false, false, false, CatDisplay::EmotionProfile::Mouth::Ponder,
};
static const CatDisplay::EmotionProfile kSadProfile = {
    C_SOFT_BLUE, CatDisplay::Color565(50, 90, 140), 0.68f, 4, 3, -3, -0.08f,
    false, false, false, CatDisplay::EmotionProfile::Mouth::Frown,
};
static const CatDisplay::EmotionProfile kAngryProfile = {
    C_RED, CatDisplay::Color565(120, 30, 30), 0.58f, -1, 6, -6, -0.12f,
    false, false, false, CatDisplay::EmotionProfile::Mouth::Flat,
};
static const CatDisplay::EmotionProfile kShyProfile = {
    C_PINK, CatDisplay::Color565(150, 90, 120), 0.63f, 0, 0, 0, -0.02f,
    true, false, false, CatDisplay::EmotionProfile::Mouth::Shy,
};
static const CatDisplay::EmotionProfile kLoveProfile = {
    C_PINK, C_PINK, 0.86f, -1, -1, 1, 0.18f,
    true, false, true, CatDisplay::EmotionProfile::Mouth::Smile,
};
static const CatDisplay::EmotionProfile kConnectProfile = {
    C_GOLD, CatDisplay::Color565(255, 150, 60), 0.88f, -4, -2, 2, 0.22f,
    false, true, false, CatDisplay::EmotionProfile::Mouth::Grin,
};
static const CatDisplay::EmotionProfile kSleepProfile = {
    C_LAVENDER, CatDisplay::Color565(70, 80, 140), 0.62f, 1, 0, 0, 0.0f,
    false, false, false, CatDisplay::EmotionProfile::Mouth::Smile,
};

CatDisplay::CatDisplay(int width, int height, lv_obj_t* canvas)
    : width_(width),
      height_(height),
      status_bar_height_(std::max(24, height / 13)),
      canvas_(canvas),
      state_(State::IDLE),
      blink_t_(0.0f),
      blink_closing_(false),
      needs_redraw_(true),
      mouth_phase_(0.0f),
      mouth_open_(0.0f),
      pulse_phase_(0.0f),
      breath_phase_(0.0f),
      whisker_spread_(0.15f),
      ear_perk_(0.0f) {
    const int draw_h = std::max(1, height_ - status_bar_height_);
    scale_ = std::min(width_ / (float)kBaseW, draw_h / (float)kBaseH) * 1.5f;
    origin_x_ = (int)((width_  - kBaseW * scale_) * 0.5f);
    origin_y_ = status_bar_height_ + (int)((draw_h - kBaseH * scale_) * 0.5f);

    state_enter_us_ = esp_timer_get_time();
    last_update_us_ = state_enter_us_;
    next_blink_us_ = state_enter_us_ + 3500000LL;
}

CatDisplay::~CatDisplay() {
    if (animation_timer_) {
        esp_timer_stop(animation_timer_);
        esp_timer_delete(animation_timer_);
        animation_timer_ = nullptr;
    }
}

bool CatDisplay::Init() {
    if (!canvas_) {
        ESP_LOGE(TAG, "No canvas");
        return false;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = &CatDisplay::AnimationTimerCallback,
        .arg = this,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "cat_anim",
        .skip_unhandled_events = true,
    };
    if (esp_timer_create(&timer_args, &animation_timer_) == ESP_OK) {
        esp_timer_start_periodic(animation_timer_, 110 * 1000);
    }

    ESP_LOGI(TAG, "CatDisplay ready (LVGL canvas): %dx%d scale=%.2f", width_, height_, scale_);
    Redraw();
    return true;
}

void CatDisplay::AnimationTimerCallback(void* arg) {
    static_cast<CatDisplay*>(arg)->Update();
}

uint16_t CatDisplay::Color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint16_t)(r & 0xF8) << 8) |
           ((uint16_t)(g & 0xFC) << 3) |
           (uint16_t)(b >> 3);
}

int CatDisplay::S(float value) const {
    return std::max(1, (int)lroundf(value * scale_));
}

int CatDisplay::X(float value) const {
    return origin_x_ + (int)lroundf(value * scale_);
}

int CatDisplay::Y(float value) const {
    return origin_y_ + (int)lroundf(value * scale_);
}

void CatDisplay::SetState(State s) {
    if (s == state_) return;
    state_ = s;
    state_enter_us_ = esp_timer_get_time();
    mouth_phase_ = 0.0f;
    mouth_open_ = 0.0f;
    needs_redraw_ = true;
    if (s == State::LISTENING || s == State::SPEAKING) {
        blink_t_ = 0.0f;
        blink_closing_ = false;
    }
}

void CatDisplay::SetStateFromStatus(const char* status) {
    if (!status) return;
    auto ci = [&](const char* needle) -> bool {
        for (const char* h = status; *h; h++) {
            const char* hh = h;
            const char* nn = needle;
            while (*hh && *nn && ((*hh | 0x20) == (*nn | 0x20))) { hh++; nn++; }
            if (!*nn) return true;
        }
        return false;
    };
    if (ci("listen")) SetState(State::LISTENING);
    else if (ci("speak")) SetState(State::SPEAKING);
    else if (ci("connect")) SetState(State::CONNECTING);
    else if (ci("sleep")) SetState(State::SLEEPING);
    else SetState(State::IDLE);
}

void CatDisplay::SetStateFromEmotion(const char* emotion) {
    if (!emotion) return;
    if (strstr(emotion, "happy") || strstr(emotion, "joy") || strstr(emotion, "рад")) {
        SetState(State::HAPPY);
    } else if (strstr(emotion, "sad") || strstr(emotion, "cry") || strstr(emotion, "груст")) {
        SetState(State::SAD);
    } else if (strstr(emotion, "angry") || strstr(emotion, "злой")) {
        SetState(State::ANGRY);
    } else if (strstr(emotion, "love") || strstr(emotion, "влюб")) {
        SetState(State::LOVE);
    } else if (strstr(emotion, "shy") || strstr(emotion, "embarrass")) {
        SetState(State::SHY);
    } else if (strstr(emotion, "sleep")) {
        SetState(State::SLEEPING);
    } else {
        SetState(State::IDLE);
    }
}

void CatDisplay::SetBatteryStatus(int level, bool charging) {
    battery_level_ = std::max(0, std::min(100, level));
    battery_charging_ = charging;
    needs_redraw_ = true;
}

void CatDisplay::Update() {
    const int64_t now = esp_timer_get_time();
    float dt = (float)(now - last_update_us_) * 1e-6f;
    if (dt < 0.03f) return;
    if (dt > 0.2f) dt = 0.2f;
    last_update_us_ = now;

    bool dirty = true;
    const bool active = state_ == State::SPEAKING || state_ == State::LISTENING || state_ == State::CONNECTING;

    if (!active && !blink_closing_ && now >= next_blink_us_) {
        blink_closing_ = true;
    }
    if (blink_closing_) {
        blink_t_ += dt * 5.0f;
        if (blink_t_ >= 1.0f) {
            blink_t_ = 1.0f;
            blink_closing_ = false;
            uint32_t extra = (uint32_t)(now & 0x1FFFFF) % 2500000u;
            next_blink_us_ = now + 3500000LL + extra;
        }
    } else if (blink_t_ > 0.0f) {
        blink_t_ -= dt * 5.0f;
        if (blink_t_ < 0.0f) blink_t_ = 0.0f;
    }

    breath_phase_ += active ? dt * 2.4f : dt * 1.1f;
    ear_perk_ *= 0.82f;

    switch (state_) {
    case State::SPEAKING:
        mouth_phase_ += dt * 9.0f;
        mouth_open_ = 0.20f + 0.45f * (0.5f + 0.5f * sinf(mouth_phase_ * 1.7f));
        mouth_open_ += 0.10f * (0.5f + 0.5f * sinf(mouth_phase_ * 0.63f));
        whisker_spread_ = 0.45f + 0.35f * (0.5f + 0.5f * sinf(mouth_phase_ * 1.1f));
        break;
    case State::CONNECTING:
        pulse_phase_ += dt * 2.5f;
        mouth_open_ *= 0.72f;
        whisker_spread_ = 0.24f;
        break;
    case State::LISTENING:
        mouth_open_ = 0.05f;
        whisker_spread_ = 0.25f;
        break;
    default:
        mouth_open_ *= 0.72f;
        if (mouth_open_ < 0.02f) mouth_open_ = 0.0f;
        whisker_spread_ = 0.12f + 0.12f * (0.5f + 0.5f * sinf(breath_phase_ * 1.7f));
        break;
    }

    if (dirty || needs_redraw_) {
        needs_redraw_ = false;
        Redraw();
    }
}

const CatDisplay::EmotionProfile& CatDisplay::CurrentProfile() const {
    switch (state_) {
    case State::CONNECTING: return kConnectProfile;
    case State::LISTENING:  return kListeningProfile;
    case State::SPEAKING:   return kSpeakingProfile;
    case State::HAPPY:      return kHappyProfile;
    case State::SAD:        return kSadProfile;
    case State::ANGRY:      return kAngryProfile;
    case State::SHY:        return kShyProfile;
    case State::LOVE:       return kLoveProfile;
    case State::SLEEPING:   return kSleepProfile;
    case State::IDLE:
    default: return kHappyProfile;
    }
}

void CatDisplay::Redraw() {
    const EmotionProfile& profile = CurrentProfile();
    const float breath = 0.5f + 0.5f * sinf(breath_phase_);
    const int face_bob = (int)lroundf((breath - 0.5f) * 3.0f);
    float blink_ratio = 1.0f - blink_t_;
    if (state_ == State::SLEEPING) blink_ratio = 0.05f;
    if (state_ == State::CONNECTING) blink_ratio = 0.75f + 0.18f * sinf(pulse_phase_);

    DrawBackground();
    DrawStatusBar();

    const int eye_l = X(kBaseCx - 38);
    const int eye_r = X(kBaseCx + 38);
    const int eye_y = Y(kBaseCy - 15 + face_bob);
    const int brow_y = Y(kBaseCy - 46 + face_bob + profile.brow_lift);

    DrawEyebrow(eye_l - S(8), brow_y, S(10), S(profile.left_tilt), profile.accent);
    DrawEyebrow(eye_r + S(8), brow_y - S(ear_perk_ * 2), S(10), S(profile.right_tilt), profile.accent);
    DrawEyeRing(eye_l, eye_y, S(28), S(6), blink_ratio, profile.accent, profile.eye_scale, profile.heart_eyes);
    DrawEyeRing(eye_r, eye_y, S(28), S(6), blink_ratio, profile.accent, profile.eye_scale, profile.heart_eyes);
    DrawNose(X(kBaseCx), Y(kBaseCy + 24 + face_bob), S(6), profile.accent);
    DrawMouth(X(kBaseCx), Y(kBaseCy + 40 + face_bob), mouth_open_, profile);
    DrawWhiskers(X(kBaseCx), Y(kBaseCy + 40 + face_bob), profile.accent, whisker_spread_, profile.whisker_bias);
    if (profile.blush) DrawBlush(X(kBaseCx), Y(kBaseCy + face_bob), C_PINK);
    if (profile.sparkle) {
        DrawSparkle(X(kBaseCx - 58), Y(kBaseCy - 38 + face_bob), S(3), profile.accent);
        DrawSparkle(X(kBaseCx + 58), Y(kBaseCy - 44 + face_bob), S(3), profile.accent);
    }

    FlushCanvas();
}

void CatDisplay::SetPixel(int x, int y, uint16_t color) {
    if ((unsigned)x < (unsigned)width_ && (unsigned)y < (unsigned)height_) {
        uint16_t* buf = (uint16_t*)lv_canvas_get_buf(canvas_);
        buf[y * width_ + x] = color;
    }
}

void CatDisplay::FillRect(int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    const int x2 = std::min(x + w, width_);
    const int y2 = std::min(y + h, height_);
    x = std::max(x, 0);
    y = std::max(y, 0);
    if (x >= x2 || y >= y2) return;
    for (int row = y; row < y2; row++) {
        for (int col = x; col < x2; col++) {
            SetPixel(col, row, color);
        }
    }
}

void CatDisplay::FillCircle(int cx, int cy, int radius, uint16_t color) {
    FillEllipse(cx, cy, radius, radius, color);
}

void CatDisplay::FillEllipse(int cx, int cy, int rx, int ry, uint16_t color) {
    if (rx <= 0 || ry <= 0) return;
    const int y1 = std::max(cy - ry, 0);
    const int y2 = std::min(cy + ry, height_ - 1);
    for (int y = y1; y <= y2; y++) {
        const float dy = (float)(y - cy) / (float)ry;
        const float dx = sqrtf(std::max(0.0f, 1.0f - dy * dy)) * (float)rx;
        int xa = std::max((int)(cx - dx + 0.5f), 0);
        int xb = std::min((int)(cx + dx - 0.5f), width_ - 1);
        if (xa > xb) continue;
        for (int x = xa; x <= xb; x++) SetPixel(x, y, color);
    }
}

void CatDisplay::FillTriangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
    if (y0 > y1) { std::swap(y0, y1); std::swap(x0, x1); }
    if (y0 > y2) { std::swap(y0, y2); std::swap(x0, x2); }
    if (y1 > y2) { std::swap(y1, y2); std::swap(x1, x2); }
    auto interp = [](int ya, int xa, int yb, int xb, int y) {
        return (yb == ya) ? xa : xa + (xb - xa) * (y - ya) / (yb - ya);
    };
    for (int y = std::max(y0, 0); y <= std::min(y2, height_ - 1); y++) {
        int xa = interp(y0, x0, y2, x2, y);
        int xb = (y < y1) ? interp(y0, x0, y1, x1, y) : interp(y1, x1, y2, x2, y);
        if (xa > xb) std::swap(xa, xb);
        FillRect(xa, y, xb - xa + 1, 1, color);
    }
}

void CatDisplay::DrawStroke(int x0, int y0, int x1, int y1, int thickness, uint16_t color) {
    const int steps = std::max(std::max(abs(x1 - x0), abs(y1 - y0)), 1);
    for (int i = 0; i <= steps; i++) {
        const float t = (float)i / (float)steps;
        const int x = (int)lroundf(x0 + (x1 - x0) * t);
        const int y = (int)lroundf(y0 + (y1 - y0) * t);
        FillCircle(x, y, thickness, color);
    }
}

void CatDisplay::DrawBackground() {
    uint16_t* buf = (uint16_t*)lv_canvas_get_buf(canvas_);
    std::fill(buf, buf + (size_t)width_ * height_, C_BG);
    uint32_t seed = 0x5EED1234;
    for (int i = 0; i < 60; i++) {
        seed = seed * 1664525u + 1013904223u;
        int sx = (seed >> 16) % width_;
        seed = seed * 1664525u + 1013904223u;
        int sy = status_bar_height_ + ((seed >> 16) % std::max(1, height_ - status_bar_height_));
        uint16_t c = (i % 3 == 0) ? C_TEXT : CatDisplay::Color565(70, 110, 150);
        SetPixel(sx, sy, c);
        if (seed & 0x8) SetPixel(sx + 1, sy, c);
    }
}

void CatDisplay::DrawStatusBar() {
    FillRect(0, 0, width_, status_bar_height_, C_PANEL);
    DrawSignalIcon(14, 7, C_TEXT);
    DrawClock(width_ / 2, 7, C_TEXT);
    DrawBatteryIcon(width_ - 54, 8, C_TEXT);
}

void CatDisplay::DrawSignalIcon(int x, int y, uint16_t color) {
    int bars = 0;
    auto& wifi = WifiStation::GetInstance();
    if (wifi.IsConnected()) {
        int rssi = wifi.GetRssi();
        bars = (rssi >= -60) ? 4 : (rssi >= -70) ? 3 : (rssi >= -80) ? 2 : 1;
    }
    for (int i = 0; i < 4; i++) {
        int h = 5 + i * 4;
        uint16_t c = (i < bars) ? color : CatDisplay::Color565(35, 55, 75);
        FillRect(x + i * 7, y + 17 - h, 4, h, c);
    }
}

void CatDisplay::DrawBatteryIcon(int x, int y, uint16_t color) {
    const int w = 34, h = 16;
    DrawStroke(x, y, x + w, y, 1, color);
    DrawStroke(x, y + h, x + w, y + h, 1, color);
    DrawStroke(x, y, x, y + h, 1, color);
    DrawStroke(x + w, y, x + w, y + h, 1, color);
    FillRect(x + w + 2, y + 5, 3, 7, color);
    int level = battery_level_ < 0 ? 60 : battery_level_;
    uint16_t fill = battery_charging_ ? C_GREEN : (level <= 20 ? C_RED : color);
    FillRect(x + 3, y + 3, ((w - 5) * level) / 100, h - 5, fill);
    if (battery_charging_) {
        DrawStroke(x + 14, y + 3, x + 9, y + 9, 1, C_BG);
        DrawStroke(x + 9, y + 9, x + 17, y + 9, 1, C_BG);
        DrawStroke(x + 17, y + 9, x + 12, y + 14, 1, C_BG);
    }
}

void CatDisplay::DrawTinyDigit(int x, int y, int digit, uint16_t color) {
    static const uint8_t segs[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
    uint8_t s = segs[digit % 10];
    if (s & 0x01) FillRect(x + 1, y, 7, 2, color);
    if (s & 0x02) FillRect(x + 8, y + 1, 2, 6, color);
    if (s & 0x04) FillRect(x + 8, y + 8, 2, 6, color);
    if (s & 0x08) FillRect(x + 1, y + 14, 7, 2, color);
    if (s & 0x10) FillRect(x, y + 8, 2, 6, color);
    if (s & 0x20) FillRect(x, y + 1, 2, 6, color);
    if (s & 0x40) FillRect(x + 1, y + 7, 7, 2, color);
}

void CatDisplay::DrawTinyColon(int x, int y, uint16_t color) {
    FillRect(x, y + 4, 2, 2, color);
    FillRect(x, y + 10, 2, 2, color);
}

void CatDisplay::DrawClock(int cx, int y, uint16_t color) {
    char text[6] = "--:--";
    time_t now = time(nullptr);
    struct tm tm_now = {};
    if (now > 1700000000 && localtime_r(&now, &tm_now)) {
        snprintf(text, sizeof(text), "%02d:%02d", tm_now.tm_hour, tm_now.tm_min);
    }
    const int total_w = 54;
    int x = cx - total_w / 2;
    for (int i = 0; i < 5; i++) {
        if (text[i] == ':') { DrawTinyColon(x, y, color); x += 6; }
        else if (text[i] >= '0' && text[i] <= '9') { DrawTinyDigit(x, y, text[i] - '0', color); x += 12; }
        else { FillRect(x + 1, y + 7, 8, 2, color); x += 12; }
    }
}

void CatDisplay::DrawEyeRing(int cx, int cy, int radius, int thickness, float blink_ratio,
                             uint16_t accent, float eye_scale, bool heart_eyes) {
    blink_ratio = std::max(0.0f, std::min(1.0f, blink_ratio * eye_scale));
    if (blink_ratio < 0.22f) {
        int slit_h = std::max(1, (int)(radius * 0.12f * (1.0f + blink_ratio * 2.0f)));
        int w = std::max(3, (int)(radius * 0.68f));
        for (int yy = cy - slit_h; yy <= cy + slit_h; yy++) {
            float edge = 1.0f - fabsf((float)(yy - cy)) / std::max(1, slit_h + 1);
            int half = std::max(3, (int)(w * (0.82f + edge * 0.18f)));
            FillRect(cx - half, yy, half * 2 + 1, 1, accent);
        }
        return;
    }
    if (heart_eyes) {
        int outer = std::max(8, (int)(radius * 0.62f));
        int inner = std::max(4, outer - thickness);
        DrawHeart(cx, cy - S(2), outer, accent);
        DrawHeart(cx, cy - S(2), inner, C_BG);
        return;
    }
    int r_outer = std::max(1, (int)(radius * (0.50f + 0.50f * blink_ratio)));
    int r_inner = std::max(0, r_outer - std::max(1, (int)(thickness * (0.65f + 0.35f * blink_ratio))));
    FillEllipse(cx, cy, r_outer, std::max(3, (int)(r_outer * (0.78f + blink_ratio * 0.18f))), accent);
    if (r_inner > 0) {
        FillEllipse(cx, cy, r_inner, std::max(2, (int)(r_inner * (0.74f + blink_ratio * 0.16f))), C_BG);
    }
}

void CatDisplay::DrawHeart(int cx, int cy, int size, uint16_t color) {
    FillCircle(cx - size / 2, cy - size / 4, std::max(1, size / 2), color);
    FillCircle(cx + size / 2, cy - size / 4, std::max(1, size / 2), color);
    for (int i = 0; i <= size; i++) {
        int half = std::max(1, size - i);
        FillRect(cx - half, cy + i, half * 2 + 1, 1, color);
    }
}

void CatDisplay::DrawEyebrow(int cx, int cy, int half_width, int tilt, uint16_t color) {
    DrawStroke(cx - half_width, cy + tilt, cx + half_width, cy - tilt, std::max(1, S(1.2f)), color);
}

void CatDisplay::DrawNose(int cx, int cy, int size, uint16_t color) {
    for (int i = 0; i < size; i++) {
        int w = size - i;
        FillRect(cx - w, cy + i, w * 2 + 1, 1, color);
    }
}

void CatDisplay::DrawMouth(int cx, int cy, float open_ratio, const EmotionProfile& profile) {
    open_ratio = std::max(0.0f, std::min(1.0f, open_ratio));
    uint16_t accent = profile.accent;
    using Mouth = EmotionProfile::Mouth;
    if (profile.mouth == Mouth::Flat) {
        FillRect(cx - S(10), cy + S(6), S(21), S(2), accent);
        return;
    }
    if (profile.mouth == Mouth::Ponder) {
        FillRect(cx - S(8), cy + S(5), S(16), S(2), accent);
        FillRect(cx + S(8), cy + S(5), S(4), 1, accent);
        return;
    }
    int stem_h = S(3 + 5 * open_ratio);
    if (profile.mouth != Mouth::Frown) {
        FillRect(cx, cy - 1, std::max(1, S(1)), stem_h, accent);
    }
    int arc_rx = S(7 + 6 * open_ratio + (profile.mouth == Mouth::Grin ? 2 : 0));
    int arc_ry = S(4 + 4 * open_ratio + (profile.mouth == Mouth::Grin ? 1 : 0));
    if (profile.mouth == Mouth::Shy) {
        arc_rx = std::max(S(5), arc_rx - S(2));
        arc_ry = std::max(S(3), arc_ry - S(1));
    }
    int arc_cy = cy + stem_h + S(1);
    for (int side : {-1, 1}) {
        int arc_cx = cx + side * S(9);
        for (int dy = -arc_ry; dy <= arc_ry; dy++) {
            int yy = (profile.mouth == Mouth::Frown) ? (cy + S(7) - dy) : (arc_cy + dy);
            float t = (float)dy / std::max(1, arc_ry);
            int half = (int)(arc_rx * sqrtf(std::max(0.0f, 1.0f - t * t)));
            if (half <= 0) continue;
            if (side < 0) FillRect(arc_cx - half, yy, half + 1, 1, accent);
            else FillRect(arc_cx, yy, half + 1, 1, accent);
        }
    }
    if ((profile.mouth == Mouth::Smile || profile.mouth == Mouth::Grin) && open_ratio > 0.18f) {
        FillEllipse(cx, cy + stem_h + arc_ry + S(3), S(5 + 4 * open_ratio), S(2 + 3 * open_ratio), profile.iris);
    }
}

void CatDisplay::DrawWhiskers(int cx, int cy, uint16_t color, float spread, float bias) {
    spread = std::max(0.0f, std::min(1.0f, spread));
    for (int side : {-1, 1}) {
        int length = S(12 + 5 * spread);
        int idx = 0;
        for (int base_dy : {-5, 0, 5}) {
            int y = cy + S(base_dy + (idx - 1) * spread * 2 + bias * 6);
            int x0 = cx + side * S(20);
            int x1 = cx + side * (S(20) + length);
            DrawStroke(x0, y, x1, y, std::max(1, S(0.65f)), color);
            idx++;
        }
    }
}

void CatDisplay::DrawBlush(int cx, int cy, uint16_t color) {
    FillEllipse(cx - S(38), cy + S(20), S(8), S(4), color);
    FillEllipse(cx + S(38), cy + S(20), S(8), S(4), color);
}

void CatDisplay::DrawSparkle(int cx, int cy, int size, uint16_t color) {
    FillRect(cx - size, cy, size * 2 + 1, 1, color);
    FillRect(cx, cy - size, 1, size * 2 + 1, color);
}

void CatDisplay::FlushCanvas() {
    if (!canvas_) return;
    lv_obj_invalidate(canvas_);
}

void CatDisplay::FillScreen(uint16_t color) {
    uint16_t* buf = (uint16_t*)lv_canvas_get_buf(canvas_);
    std::fill(buf, buf + (size_t)width_ * height_, color);
    FlushCanvas();
}
