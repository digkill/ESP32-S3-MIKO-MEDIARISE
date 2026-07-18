#include "servo.h"
#include "config.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "esp_timer.h"

static const char* TAG = "Servo";

static bool s_ok       = false;
static bool s_attached = false;
static bool s_moving   = false;

static int s_target_yaw   = SERVO_HOME_ANGLE;
static int s_target_pitch = SERVO_HOME_ANGLE;
static int s_cur_yaw      = SERVO_HOME_ANGLE;
static int s_cur_pitch    = SERVO_HOME_ANGLE;

static uint32_t s_last_duty_yaw   = 0xFFFFFFFFu;
static uint32_t s_last_duty_pitch = 0xFFFFFFFFu;
static int64_t  s_last_step_us    = 0;
static int64_t  s_last_move_us    = 0;

static int clamp_angle(int a) {
    if (a < SERVO_MIN_ANGLE) return SERVO_MIN_ANGLE;
    if (a > SERVO_MAX_ANGLE) return SERVO_MAX_ANGLE;
    return a;
}

static uint32_t angle_to_duty(int angle) {
    angle = clamp_angle(angle);
    const uint32_t period_us = 1000000UL / SERVO_PWM_FREQ_HZ;
    const uint32_t max_duty  = (1UL << 14) - 1; // LEDC_TIMER_14_BIT
    const uint32_t pulse_us  = SERVO_MIN_US +
        ((uint32_t)(SERVO_MAX_US - SERVO_MIN_US) * (uint32_t)(angle - SERVO_CALIB_MIN_ANGLE)) /
        (uint32_t)(SERVO_CALIB_MAX_ANGLE - SERVO_CALIB_MIN_ANGLE);
    uint32_t d = (pulse_us * max_duty) / period_us;
#if SERVO_DUTY_QUANTUM > 1
    d = (d / SERVO_DUTY_QUANTUM) * SERVO_DUTY_QUANTUM;
    if (d > max_duty) d = max_duty;
#endif
    return d;
}

static void write_duty(ledc_channel_t ch, uint32_t duty) {
    ledc_set_duty(SERVO_LEDC_MODE, ch, duty);
    ledc_update_duty(SERVO_LEDC_MODE, ch);
}

static bool attach_channel(ledc_channel_t ch, int gpio, uint32_t duty) {
    ledc_channel_config_t cfg = {};
    cfg.channel    = ch;
    cfg.duty       = duty;
    cfg.gpio_num   = gpio;
    cfg.speed_mode = SERVO_LEDC_MODE;
    cfg.hpoint     = 0;
    cfg.timer_sel  = SERVO_LEDC_TIMER;
    return ledc_channel_config(&cfg) == ESP_OK;
}

bool servo_init(void) {
    ledc_timer_config_t timer = {};
    timer.duty_resolution = SERVO_PWM_RES_BITS;
    timer.freq_hz         = SERVO_PWM_FREQ_HZ;
    timer.speed_mode      = SERVO_LEDC_MODE;
    timer.timer_num       = SERVO_LEDC_TIMER;
    timer.clk_cfg         = LEDC_AUTO_CLK;
    if (ledc_timer_config(&timer) != ESP_OK) {
        ESP_LOGE(TAG, "timer config failed"); return false;
    }

    const uint32_t duty = angle_to_duty(SERVO_HOME_ANGLE);
    if (!attach_channel(SERVO_LEDC_CH_YAW,   SERVO_YAW_GPIO,   duty)) {
        ESP_LOGE(TAG, "yaw channel failed"); return false;
    }
    if (!attach_channel(SERVO_LEDC_CH_PITCH, SERVO_PITCH_GPIO, duty)) {
        ESP_LOGE(TAG, "pitch channel failed"); return false;
    }

    s_cur_yaw = s_target_yaw = SERVO_HOME_ANGLE;
    s_cur_pitch = s_target_pitch = SERVO_HOME_ANGLE;
    s_last_duty_yaw = s_last_duty_pitch = duty;
    s_attached = true;
    s_moving   = false;
    s_last_move_us = s_last_step_us = esp_timer_get_time();
    s_ok = true;

    ESP_LOGI(TAG, "OK yaw=GPIO%d pitch=GPIO%d home=%d range=%d..%d",
             SERVO_YAW_GPIO, SERVO_PITCH_GPIO,
             SERVO_HOME_ANGLE, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
    return true;
}

bool servo_is_ok(void)       { return s_ok; }
bool servo_is_attached(void) { return s_attached; }
bool servo_is_moving(void)   { return s_moving; }
int  servo_get_yaw(void)     { return s_cur_yaw; }
int  servo_get_pitch(void)   { return s_cur_pitch; }
int  servo_get_target_yaw(void)   { return s_target_yaw; }
int  servo_get_target_pitch(void) { return s_target_pitch; }

void servo_set_target(int yaw, int pitch) {
    s_target_yaw   = clamp_angle(yaw);
    s_target_pitch = clamp_angle(pitch);
    if (!s_ok) return;
    if (!s_attached) servo_reattach();
    s_moving = (s_cur_yaw != s_target_yaw) || (s_cur_pitch != s_target_pitch);
    s_last_move_us = esp_timer_get_time();
    s_last_step_us = 0;
}

void servo_detach(void) {
    if (!s_attached) return;
    ledc_stop(SERVO_LEDC_MODE, SERVO_LEDC_CH_YAW,   0);
    ledc_stop(SERVO_LEDC_MODE, SERVO_LEDC_CH_PITCH,  0);
    s_attached = false;
}

bool servo_reattach(void) {
    if (s_attached) return true;
    const uint32_t dy = angle_to_duty(s_cur_yaw);
    const uint32_t dp = angle_to_duty(s_cur_pitch);
    if (!attach_channel(SERVO_LEDC_CH_YAW,   SERVO_YAW_GPIO,   dy)) return false;
    if (!attach_channel(SERVO_LEDC_CH_PITCH, SERVO_PITCH_GPIO, dp)) {
        ledc_stop(SERVO_LEDC_MODE, SERVO_LEDC_CH_YAW, 0);
        return false;
    }
    s_last_duty_yaw   = dy;
    s_last_duty_pitch = dp;
    s_attached = true;
    return true;
}

static int step_toward(int cur, int tgt) {
    if (cur == tgt) return cur;
    int d = tgt - cur;
    int step = (abs(d) < SERVO_STEP_DEG) ? abs(d) : SERVO_STEP_DEG;
    return cur + (d > 0 ? step : -step);
}

void servo_service(void) {
    if (!s_ok || !s_moving) return;
    const int64_t now = esp_timer_get_time();
    if (s_last_step_us && (now - s_last_step_us) < (int64_t)SERVO_MOVE_INTERVAL_MS * 1000) return;
    if (!s_attached && !servo_reattach()) return;

    s_last_step_us = now;
    s_cur_yaw   = step_toward(s_cur_yaw,   s_target_yaw);
    s_cur_pitch = step_toward(s_cur_pitch, s_target_pitch);

    const uint32_t dy = angle_to_duty(s_cur_yaw);
    const uint32_t dp = angle_to_duty(s_cur_pitch);
    if (dy != s_last_duty_yaw)   { write_duty(SERVO_LEDC_CH_YAW,   dy); s_last_duty_yaw   = dy; }
    if (dp != s_last_duty_pitch) { write_duty(SERVO_LEDC_CH_PITCH, dp); s_last_duty_pitch = dp; }

    s_last_move_us = now;
    s_moving = (s_cur_yaw != s_target_yaw) || (s_cur_pitch != s_target_pitch);

#if SERVO_RELAX_MS > 0
    if (!s_moving) {
        // Will detach from main loop after SERVO_RELAX_MS
    }
#endif
}
