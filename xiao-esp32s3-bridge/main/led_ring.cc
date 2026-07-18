#include "led_ring.h"
#include "config.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "led_strip.h"

static const char* TAG = "LEDRing";

static led_strip_handle_t s_strip = nullptr;
static bool s_ok = false;
static bool s_rainbow = true;
static uint8_t s_rainbow_pos = 0;
static int64_t s_last_frame_us = 0;

#ifndef LED_RAINBOW_MS
#define LED_RAINBOW_MS 60
#endif

static void hsv_to_rgb(uint8_t h, uint8_t s, uint8_t v, uint8_t* r, uint8_t* g, uint8_t* b) {
    if (s == 0) {
        *r = *g = *b = v;
        return;
    }
    uint8_t region = h / 43;
    uint8_t remainder = (h - region * 43) * 6;
    uint8_t p = (v * (255 - s)) >> 8;
    uint8_t q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    uint8_t t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;
    switch (region) {
        case 0:  *r = v; *g = t; *b = p; break;
        case 1:  *r = q; *g = v; *b = p; break;
        case 2:  *r = p; *g = v; *b = t; break;
        case 3:  *r = p; *g = q; *b = v; break;
        case 4:  *r = t; *g = p; *b = v; break;
        default: *r = v; *g = p; *b = q; break;
    }
}

static void led_ring_draw_rainbow(void) {
    for (int i = 0; i < LED_RING_COUNT; i++) {
        // Смещение s_rainbow_pos — цвета «бегут» по кольцу.
        uint8_t h = (uint8_t)(s_rainbow_pos + i * (256 / LED_RING_COUNT));
        uint8_t r, g, b;
        hsv_to_rgb(h, 255, 100, &r, &g, &b);
        led_strip_set_pixel(s_strip, i, r, g, b);
    }
    led_strip_refresh(s_strip);
    s_rainbow_pos++;
}

bool led_ring_init(void) {
    gpio_reset_pin((gpio_num_t)LED_RING_GPIO);
    gpio_set_direction((gpio_num_t)LED_RING_GPIO, GPIO_MODE_OUTPUT);

    led_strip_config_t strip_cfg = {};
    strip_cfg.strip_gpio_num = LED_RING_GPIO;
    strip_cfg.max_leds       = LED_RING_COUNT;
    strip_cfg.led_model      = LED_MODEL_WS2812;

    led_strip_rmt_config_t rmt_cfg = {};
    rmt_cfg.clk_src           = RMT_CLK_SRC_DEFAULT;
    rmt_cfg.resolution_hz     = 10000000;
    rmt_cfg.mem_block_symbols = 64;
    rmt_cfg.flags.with_dma    = false;

    esp_err_t err = led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_strip);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "RMT init failed: %s", esp_err_to_name(err));
        return false;
    }

    s_ok = true;
    s_rainbow = true;
    s_rainbow_pos = 0;
    s_last_frame_us = 0;
    led_ring_draw_rainbow();

    ESP_LOGI(TAG, "OK gpio=%d count=%d rainbow=on", LED_RING_GPIO, LED_RING_COUNT);
    return true;
}

bool led_ring_is_ok(void) { return s_ok; }

void led_ring_rainbow_enable(bool on) {
    s_rainbow = on;
    if (on && s_ok) {
        s_last_frame_us = 0;
    }
}

bool led_ring_rainbow_active(void) { return s_rainbow; }

void led_ring_service(void) {
    if (!s_ok || !s_rainbow) return;
    int64_t now = esp_timer_get_time();
    if (s_last_frame_us != 0 &&
        (now - s_last_frame_us) / 1000 < (int64_t)LED_RAINBOW_MS) {
        return;
    }
    s_last_frame_us = now;
    led_ring_draw_rainbow();
}

void led_ring_set_all(uint8_t r, uint8_t g, uint8_t b) {
    if (!s_ok) return;
    s_rainbow = false;
    for (int i = 0; i < LED_RING_COUNT; i++)
        led_strip_set_pixel(s_strip, i, r, g, b);
    led_strip_refresh(s_strip);
}

void led_ring_set_pixel(int idx, uint8_t r, uint8_t g, uint8_t b) {
    if (!s_ok || idx < 0 || idx >= LED_RING_COUNT) return;
    s_rainbow = false;
    led_strip_set_pixel(s_strip, idx, r, g, b);
    led_strip_refresh(s_strip);
}

void led_ring_clear(void) {
    if (!s_ok) return;
    s_rainbow = false;
    led_strip_clear(s_strip);
}

void led_ring_set_default(void) {
    s_rainbow = true;
    s_last_frame_us = 0;
}

void led_ring_test(void) {
    if (!s_ok) return;
    s_rainbow = true;
    s_last_frame_us = 0;
}
