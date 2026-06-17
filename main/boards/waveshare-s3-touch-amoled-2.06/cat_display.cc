#include "cat_display.h"

#include <esp_log.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include <esp_cache.h>
#include <esp_lvgl_port.h>
#include <wifi_station.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <cstdlib>

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
static const uint16_t C_COFFEE = CatDisplay::Color565(191, 121, 67);
static const uint16_t C_CREAM = CatDisplay::Color565(255, 224, 170);

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
static const CatDisplay::EmotionProfile kCoffeeProfile = {
    C_CREAM, C_COFFEE, 0.82f, -2, 0, 0, 0.12f,
    true, false, false, CatDisplay::EmotionProfile::Mouth::Smile,
};
static const CatDisplay::EmotionProfile kDanceProfile = {
    C_GOLD, C_PINK, 0.92f, -5, -2, 2, 0.24f,
    true, true, false, CatDisplay::EmotionProfile::Mouth::Grin,
};
static const CatDisplay::EmotionProfile kCuriousProfile = {
    C_SOFT_BLUE, C_GLOW, 1.04f, -3, -2, 0, 0.16f,
    false, true, false, CatDisplay::EmotionProfile::Mouth::Ponder,
};
static const CatDisplay::EmotionProfile kCelebrateProfile = {
    C_GREEN, C_GOLD, 0.96f, -6, -2, 2, 0.30f,
    true, true, false, CatDisplay::EmotionProfile::Mouth::Grin,
};
static const CatDisplay::EmotionProfile kDizzyProfile = {
    C_GOLD, C_SOFT_BLUE, 0.66f, 3, 3, -3, 0.32f,
    false, true, false, CatDisplay::EmotionProfile::Mouth::Ponder,
};

CatDisplay::CatDisplay(int width, int height, lv_obj_t* canvas, float presentation_scale)
    : physical_width_(width),
      physical_height_(height),
      width_(height),
      height_(width),
      rotate_cw_(height > width),
      presentation_scale_(std::max(1.0f, presentation_scale)),
      status_bar_height_(std::max(U(45), height_ / 14)),
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
    // Fill display: use max scale so face covers the full width/height, clips minimally.
    scale_ = std::max(width_ / (float)kBaseW, draw_h / (float)kBaseH);
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

    // Yekaterinburg timezone (UTC+5). POSIX sign is inverted: UTC+5 → offset -5.
    setenv("TZ", "YEKT-5", 1);
    tzset();

    // Cache draw_buf->data directly — lv_canvas_get_buf returns unaligned_data which
    // may differ if LVGL aligns the buffer internally.
    lv_draw_buf_t* draw_buf = lv_canvas_get_draw_buf(canvas_);
    if (!draw_buf || !draw_buf->data) {
        ESP_LOGE(TAG, "Canvas draw_buf unavailable");
        return false;
    }
    canvas_buf_ = (uint16_t*)draw_buf->data;

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

    ESP_LOGI(TAG, "CatDisplay ready (LVGL canvas): physical=%dx%d logical=%dx%d rotate_cw=%d scale=%.2f",
             physical_width_, physical_height_, width_, height_, rotate_cw_, scale_);
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

int CatDisplay::U(float value) const {
    return std::max(1, (int)lroundf(value / presentation_scale_));
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
        ear_perk_ = 1.0f;
    }
}

void CatDisplay::PlayDizzy(uint32_t duration_ms) {
    SetState(State::DIZZY);
    dizzy_until_us_ = esp_timer_get_time() + (int64_t)duration_ms * 1000;
    needs_redraw_ = true;
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
    } else if (strstr(emotion, "coffee") || strstr(emotion, "cafe") || strstr(emotion, "коф")) {
        SetState(State::COFFEE);
    } else if (strstr(emotion, "dance") || strstr(emotion, "dancing")) {
        SetState(State::DANCE);
    } else if (strstr(emotion, "curious") || strstr(emotion, "question")) {
        SetState(State::CURIOUS);
    } else if (strstr(emotion, "celebrate") || strstr(emotion, "party")) {
        SetState(State::CELEBRATE);
    } else if (strstr(emotion, "dizzy") || strstr(emotion, "shake")) {
        PlayDizzy();
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
    if (state_ == State::DIZZY && now >= dizzy_until_us_) {
        SetState(State::IDLE);
    }

    bool dirty = true;
    const bool active = state_ == State::SPEAKING || state_ == State::LISTENING ||
                        state_ == State::CONNECTING || state_ == State::DANCE ||
                        state_ == State::CELEBRATE || state_ == State::DIZZY;

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
    case State::COFFEE:
        pulse_phase_ += dt * 2.2f;
        mouth_open_ *= 0.72f;
        whisker_spread_ = 0.18f;
        break;
    case State::DANCE:
        pulse_phase_ += dt * 7.5f;
        mouth_open_ = 0.25f + 0.20f * (0.5f + 0.5f * sinf(pulse_phase_));
        whisker_spread_ = 0.42f + 0.25f * (0.5f + 0.5f * sinf(pulse_phase_));
        break;
    case State::CELEBRATE:
        pulse_phase_ += dt * 6.0f;
        mouth_open_ = 0.32f + 0.24f * (0.5f + 0.5f * sinf(pulse_phase_));
        whisker_spread_ = 0.55f;
        break;
    case State::CURIOUS:
        pulse_phase_ += dt * 2.0f;
        mouth_open_ = 0.04f;
        whisker_spread_ = 0.22f;
        break;
    case State::DIZZY:
        pulse_phase_ += dt * 6.5f;
        mouth_open_ = 0.03f;
        whisker_spread_ = 0.42f;
        break;
    case State::SLEEPING:
        pulse_phase_ += dt * 0.7f;
        mouth_open_ *= 0.92f;
        if (mouth_open_ < 0.01f) mouth_open_ = 0.0f;
        whisker_spread_ = 0.10f + 0.04f * (0.5f + 0.5f * sinf(breath_phase_ * 0.5f));
        break;
    default:
        mouth_open_ *= 0.72f;
        if (mouth_open_ < 0.02f) mouth_open_ = 0.0f;
        whisker_spread_ = 0.12f + 0.12f * (0.5f + 0.5f * sinf(breath_phase_ * 1.7f));
        break;
    }

    for (auto& b : bubbles_) {
        if (!b.active) continue;
        b.x += b.vx * dt;
        b.y += b.vy * dt;
        b.alpha -= dt * 1.1f;
        if (b.alpha <= 0.0f) {
            b.active = false;
        } else {
            dirty = true;
        }
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
    case State::COFFEE:     return kCoffeeProfile;
    case State::DANCE:      return kDanceProfile;
    case State::CURIOUS:    return kCuriousProfile;
    case State::CELEBRATE:  return kCelebrateProfile;
    case State::DIZZY:      return kDizzyProfile;
    case State::SLEEPING:   return kSleepProfile;
    case State::IDLE:
    default: return kHappyProfile;
    }
}

static uint16_t DimColor(uint16_t color, float factor) {
    factor = std::max(0.0f, std::min(1.0f, factor));
    uint8_t r = (uint8_t)(((color >> 11) & 0x1F) * factor + 0.5f);
    uint8_t g = (uint8_t)(((color >> 5)  & 0x3F) * factor + 0.5f);
    uint8_t b = (uint8_t)((color & 0x1F) * factor + 0.5f);
    return ((uint16_t)r << 11) | ((uint16_t)g << 5) | (uint16_t)b;
}

void CatDisplay::Redraw() {
    if (!canvas_buf_ || !canvas_) {
        return;
    }

    if (!lvgl_port_lock(250)) {
        needs_redraw_ = true;
        return;
    }

    const EmotionProfile& profile = CurrentProfile();
    const float breath = 0.5f + 0.5f * sinf(breath_phase_);
    int face_bob = (int)lroundf((breath - 0.5f) * 3.0f);
    if (state_ == State::DANCE || state_ == State::CELEBRATE || state_ == State::DIZZY) {
        face_bob += (int)lroundf(sinf(pulse_phase_) * 3.0f);
    }
    float blink_ratio = 1.0f - blink_t_;
    if (state_ == State::SLEEPING) blink_ratio = 0.05f;
    if (state_ == State::CONNECTING) blink_ratio = 0.75f + 0.18f * sinf(pulse_phase_);
    if (state_ == State::DANCE) blink_ratio = 0.78f + 0.18f * (0.5f + 0.5f * sinf(pulse_phase_ * 0.5f));
    if (state_ == State::DIZZY) blink_ratio = 0.34f + 0.14f * (0.5f + 0.5f * sinf(pulse_phase_));

    DrawBackground();
    DrawStatusBar();
    DrawCatEars(face_bob, profile);

    const int eye_l = X(kBaseCx - 38);
    const int eye_r = X(kBaseCx + 38);
    const int eye_y = Y(kBaseCy - 15 + face_bob);
    const int brow_y = Y(kBaseCy - 46 + face_bob + profile.brow_lift);

    DrawEyebrow(eye_l, brow_y, S(10), S(profile.left_tilt), profile.accent);
    DrawEyebrow(eye_r, brow_y - S(ear_perk_ * 2), S(10), S(profile.right_tilt), profile.accent);
    DrawEyeRing(eye_l, eye_y, S(28), S(6), blink_ratio, profile.accent,
                profile.eye_scale, profile.heart_eyes);
    DrawEyeRing(eye_r, eye_y, S(28), S(6), blink_ratio, profile.accent,
                profile.eye_scale, profile.heart_eyes);
    DrawNose(X(kBaseCx), Y(kBaseCy + 24 + face_bob), S(6), profile.accent);
    DrawMouth(X(kBaseCx), Y(kBaseCy + 40 + face_bob), mouth_open_, profile);
    DrawWhiskers(X(kBaseCx), Y(kBaseCy + 40 + face_bob), profile.accent, whisker_spread_, profile.whisker_bias);
    if (state_ == State::COFFEE) {
        DrawCoffeeCup(X(kBaseCx), Y(kBaseCy + 73 + face_bob), profile.accent);
    }
    if (state_ == State::DIZZY) {
        DrawDizzyOrbit(face_bob, profile.accent);
    }
    if (state_ == State::SLEEPING) {
        DrawZzz(X(kBaseCx + 52), Y(kBaseCy - 30 + face_bob), profile.accent);
    }
    for (const auto& b : bubbles_) {
        if (!b.active) continue;
        DrawBubble((int)b.x, (int)b.y, (int)b.radius, DimColor(C_GLOW, b.alpha));
    }
    if (profile.blush) DrawBlush(X(kBaseCx), Y(kBaseCy + face_bob), C_PINK);
    if (profile.sparkle) {
        DrawSparkle(X(kBaseCx - 58), Y(kBaseCy - 38 + face_bob), S(3), profile.accent);
        DrawSparkle(X(kBaseCx + 58), Y(kBaseCy - 44 + face_bob), S(3), profile.accent);
        if (state_ == State::DANCE || state_ == State::CELEBRATE) {
            DrawSparkle(X(kBaseCx - 80), Y(kBaseCy + 22 - face_bob), S(4), profile.accent);
            DrawSparkle(X(kBaseCx + 78), Y(kBaseCy + 28 + face_bob), S(4), profile.accent);
        }
    }

    // Flush core-0's D-cache to PSRAM so the LVGL task on core-1 sees new pixels.
    esp_cache_msync(canvas_buf_, (size_t)physical_width_ * physical_height_ * sizeof(uint16_t),
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    lv_obj_invalidate(canvas_);
    lvgl_port_unlock();
}

void CatDisplay::SetPixel(int x, int y, uint16_t color) {
    if ((unsigned)x < (unsigned)width_ && (unsigned)y < (unsigned)height_) {
        int px = x;
        int py = y;
        if (rotate_cw_) {
            px = height_ - 1 - y;
            py = x;
        }
        if ((unsigned)px < (unsigned)physical_width_ && (unsigned)py < (unsigned)physical_height_) {
            canvas_buf_[py * physical_width_ + px] = color;
        }
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

void CatDisplay::DrawZzz(int cx, int cy, uint16_t color) {
    // Three Z letters appear in sequence, each drifting upward in a staircase
    static constexpr float kCycle = 3.0f;
    const float phase = fmodf(pulse_phase_ * 0.5f, kCycle);
    for (int i = 0; i < 3; i++) {
        float t = fmodf(phase - (float)i + kCycle, kCycle);
        if (t > 1.0f) continue;
        // fade in (0..0.25), hold (0.25..0.75), fade out (0.75..1.0)
        float alpha = (t < 0.25f) ? (t / 0.25f) : (t > 0.75f) ? ((1.0f - t) / 0.25f) : 1.0f;
        int sz = S(3 + i);
        int zx = cx + S(i * 14);
        int zy = cy - S(i * 14) - (int)(t * S(20));
        uint16_t zcol = DimColor(color, alpha);
        DrawStroke(zx - sz, zy - sz, zx + sz, zy - sz, 1, zcol);  // top bar
        DrawStroke(zx + sz, zy - sz, zx - sz, zy + sz, 1, zcol);  // diagonal
        DrawStroke(zx - sz, zy + sz, zx + sz, zy + sz, 1, zcol);  // bottom bar
    }
}

void CatDisplay::DrawBubble(int cx, int cy, int radius, uint16_t color) {
    if (radius <= 0) { SetPixel(cx, cy, color); return; }
    // Bresenham circle ring
    int x = radius, y = 0, d = 1 - radius;
    while (x >= y) {
        SetPixel(cx + x, cy + y, color); SetPixel(cx - x, cy + y, color);
        SetPixel(cx + x, cy - y, color); SetPixel(cx - x, cy - y, color);
        SetPixel(cx + y, cy + x, color); SetPixel(cx - y, cy + x, color);
        SetPixel(cx + y, cy - x, color); SetPixel(cx - y, cy - x, color);
        y++;
        if (d < 0) { d += 2 * y + 1; }
        else       { x--; d += 2 * (y - x) + 1; }
    }
    // Bright highlight dot at top-left for 3D look
    if (radius > 4) {
        int hx = cx - radius / 3;
        int hy = cy - radius / 3;
        uint16_t hi = DimColor(0xFFFF, 0.85f);
        SetPixel(hx,     hy,     hi);
        SetPixel(hx + 1, hy,     hi);
        SetPixel(hx,     hy + 1, hi);
    }
}

void CatDisplay::SpawnTouchBubbles(int raw_x, int raw_y) {
    // Convert physical touch coords to logical canvas coords
    float lx = rotate_cw_ ? (float)raw_y : (float)raw_x;
    float ly = rotate_cw_ ? (float)(height_ - 1 - raw_x) : (float)raw_y;

    for (int i = 0; i < 5; i++) {
        for (auto& b : bubbles_) {
            if (b.active) continue;
            bubble_seed_ = bubble_seed_ * 1664525u + 1013904223u;
            float ox = (float)((int)((bubble_seed_ >> 16) & 0x3F) - 32);
            bubble_seed_ = bubble_seed_ * 1664525u + 1013904223u;
            float oy = (float)((int)((bubble_seed_ >> 14) & 0x1F) - 8);
            bubble_seed_ = bubble_seed_ * 1664525u + 1013904223u;
            b.x  = lx + ox;
            b.y  = ly + oy;
            b.vx = ((float)((bubble_seed_ >> 8) & 0x3F) - 32) * 0.6f;
            b.vy = -35.0f - (float)((bubble_seed_ >> 16) & 0x3F) * 0.8f;
            b.radius = 5.0f + (float)((bubble_seed_ & 0x0F));
            b.alpha  = 1.0f;
            b.active = true;
            break;
        }
    }
    needs_redraw_ = true;
}

void CatDisplay::DrawBackground() {
    std::fill(canvas_buf_, canvas_buf_ + (size_t)physical_width_ * physical_height_, C_BG);
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
    DrawSignalIcon(U(130), U(9), C_TEXT);
    DrawClock(width_ / 2, U(8), C_TEXT);
    DrawBatteryIcon(width_ - U(162), U(10), C_TEXT);
}

void CatDisplay::DrawSignalIcon(int x, int y, uint16_t color) {
    int bars = 0;
    auto& wifi = WifiStation::GetInstance();
    if (wifi.IsConnected()) {
        int rssi = wifi.GetRssi();
        bars = (rssi >= -60) ? 4 : (rssi >= -70) ? 3 : (rssi >= -80) ? 2 : 1;
    }
    for (int i = 0; i < 4; i++) {
        int h = U(6 + i * 7);
        uint16_t c = (i < bars) ? color : CatDisplay::Color565(35, 55, 75);
        FillRect(x + i * U(10), y + U(30) - h, U(6), h, c);
    }
}

void CatDisplay::DrawBatteryIcon(int x, int y, uint16_t color) {
    const int w = U(45);
    const int h = U(24);
    DrawStroke(x, y, x + w, y, U(1), color);
    DrawStroke(x, y + h, x + w, y + h, U(1), color);
    DrawStroke(x, y, x, y + h, U(1), color);
    DrawStroke(x + w, y, x + w, y + h, U(1), color);
    FillRect(x + w + U(3), y + U(8), U(4), U(10), color);
    int level = battery_level_ < 0 ? 60 : battery_level_;
    uint16_t fill = battery_charging_ ? C_GREEN : (level <= 20 ? C_RED : color);
    FillRect(x + U(4), y + U(4), ((w - U(8)) * level) / 100, h - U(8), fill);
    if (battery_charging_) {
        DrawStroke(x + U(21), y + U(4), x + U(15), y + U(12), U(1), C_BG);
        DrawStroke(x + U(15), y + U(12), x + U(27), y + U(12), U(1), C_BG);
        DrawStroke(x + U(27), y + U(12), x + U(21), y + U(20), U(1), C_BG);
    }
}

void CatDisplay::DrawTinyDigit(int x, int y, int digit, uint16_t color) {
    static const uint8_t segs[10] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F };
    uint8_t s = segs[digit % 10];
    if (s & 0x01) FillRect(x + U(2), y, U(12), U(3), color);
    if (s & 0x02) FillRect(x + U(14), y + U(2), U(3), U(10), color);
    if (s & 0x04) FillRect(x + U(14), y + U(14), U(3), U(10), color);
    if (s & 0x08) FillRect(x + U(2), y + U(24), U(12), U(3), color);
    if (s & 0x10) FillRect(x, y + U(14), U(3), U(10), color);
    if (s & 0x20) FillRect(x, y + U(2), U(3), U(10), color);
    if (s & 0x40) FillRect(x + U(2), y + U(12), U(12), U(3), color);
}

void CatDisplay::DrawTinyColon(int x, int y, uint16_t color) {
    FillRect(x, y + U(8), U(3), U(3), color);
    FillRect(x, y + U(18), U(3), U(3), color);
}

void CatDisplay::DrawClock(int cx, int y, uint16_t color) {
    char text[6] = "--:--";
    time_t now = time(nullptr);
    struct tm tm_now = {};
    if (now > 1700000000 && localtime_r(&now, &tm_now)) {
        snprintf(text, sizeof(text), "%02d:%02d", tm_now.tm_hour, tm_now.tm_min);
    }
    const int total_w = U(93);
    int x = cx - total_w / 2;
    for (int i = 0; i < 5; i++) {
        if (text[i] == ':') { DrawTinyColon(x, y, color); x += U(10); }
        else if (text[i] >= '0' && text[i] <= '9') { DrawTinyDigit(x, y, text[i] - '0', color); x += U(21); }
        else { FillRect(x + U(2), y + U(12), U(14), U(3), color); x += U(21); }
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
    (void)half_width;
    const int radius = std::max(2, S(5.0f));
    FillCircle(cx, cy - tilt / 3, radius, color);
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

void CatDisplay::DrawCoffeeCup(int cx, int cy, uint16_t color) {
    const int cup_w = S(26);
    const int cup_h = S(13);
    FillRect(cx - cup_w / 2, cy, cup_w, cup_h, color);
    FillEllipse(cx, cy + cup_h, cup_w / 2, S(4), color);
    FillEllipse(cx, cy + S(2), cup_w / 2 - S(2), S(3), C_COFFEE);
    DrawStroke(cx + cup_w / 2, cy + S(4), cx + cup_w / 2 + S(7), cy + S(7), S(2), color);
    DrawStroke(cx + cup_w / 2 + S(7), cy + S(7), cx + cup_w / 2, cy + S(11), S(2), color);

    const int drift = (int)lroundf(sinf(pulse_phase_) * S(2));
    DrawStroke(cx - S(7), cy - S(4), cx - S(5) + drift, cy - S(13), S(1), C_CREAM);
    DrawStroke(cx + S(2), cy - S(5), cx + drift, cy - S(17), S(1), C_CREAM);
    DrawStroke(cx + S(9), cy - S(4), cx + S(7) - drift, cy - S(12), S(1), C_CREAM);
}

void CatDisplay::DrawDizzyOrbit(int face_bob, uint16_t color) {
    const int cx = X(kBaseCx);
    const int cy = Y(kBaseCy - 74 + face_bob);
    const float phase = pulse_phase_;
    for (int i = 0; i < 3; ++i) {
        const float angle = phase + i * 2.0943951f;
        const int x = cx + (int)lroundf(cosf(angle) * S(55));
        const int y = cy + (int)lroundf(sinf(angle) * S(12));
        DrawSparkle(x, y, S(4), (i == 1) ? C_SOFT_BLUE : color);
    }

    // Two minimal cartoon birds circle in the opposite direction.
    for (int i = 0; i < 2; ++i) {
        const float angle = -phase * 0.82f + 1.2f + i * 3.1415926f;
        const int x = cx + (int)lroundf(cosf(angle) * S(73));
        const int y = cy - S(12) + (int)lroundf(sinf(angle) * S(17));
        DrawStroke(x - S(8), y + S(2), x, y - S(3), S(1), C_CREAM);
        DrawStroke(x, y - S(3), x + S(8), y + S(2), S(1), C_CREAM);
        FillCircle(x + S(9), y + S(1), S(1), C_GOLD);
    }
}

void CatDisplay::DrawCatEars(int face_bob, const EmotionProfile& profile) {
    const int ear_half = S(21);
    const int ear_base_y = Y(kBaseCy - 45 + face_bob);
    const int ear_tip_lift = (int)(ear_perk_ * S(7));

    for (int side : {-1, 1}) {
        const int ear_cx = X(kBaseCx + side * 67);
        const int tip_x  = ear_cx + side * S(24);
        const int tip_y  = std::max(status_bar_height_ + S(1),
                                    Y(kBaseCy - 90 + face_bob) - ear_tip_lift);

        // Outer ear fur
        FillTriangle(ear_cx - ear_half, ear_base_y,
                     ear_cx + ear_half, ear_base_y,
                     tip_x, tip_y,
                     profile.accent);
        // Inner ear skin (pink)
        const int inner_half = std::max(2, ear_half * 5 / 8);
        FillTriangle(ear_cx - inner_half, ear_base_y - S(4),
                     ear_cx + inner_half, ear_base_y - S(4),
                     tip_x, tip_y + S(8),
                     C_PINK);
    }
}

void CatDisplay::FlushCanvas() {
    if (!canvas_) return;
    // Must hold LVGL lock — lv_obj_invalidate is not thread-safe; calling it
    // from the ESP timer task without the lock silently loses the dirty mark.
    if (lvgl_port_lock(200)) {
        lv_obj_invalidate(canvas_);
        lvgl_port_unlock();
    }
}

void CatDisplay::FillScreen(uint16_t color) {
    if (!canvas_buf_ || !canvas_) return;
    if (!lvgl_port_lock(250)) return;
    std::fill(canvas_buf_, canvas_buf_ + (size_t)physical_width_ * physical_height_, color);
    esp_cache_msync(canvas_buf_, (size_t)physical_width_ * physical_height_ * sizeof(uint16_t),
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_UNALIGNED);
    lv_obj_invalidate(canvas_);
    lvgl_port_unlock();
}
