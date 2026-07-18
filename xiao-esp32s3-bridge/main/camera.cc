#include "camera.h"
#include "config.h"
#include "esp_log.h"
#include "esp_psram.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char* TAG = "Camera";

static bool s_camera_ok = false;
static SemaphoreHandle_t s_mutex = nullptr;
volatile uint32_t g_frame_id = 0;

static bool init_once(int sda, int scl) {
    camera_config_t cfg = {};
    cfg.ledc_channel  = LEDC_CHANNEL_0;
    cfg.ledc_timer    = LEDC_TIMER_0;
    cfg.pin_d0        = CAM_PIN_D0;  cfg.pin_d1 = CAM_PIN_D1;
    cfg.pin_d2        = CAM_PIN_D2;  cfg.pin_d3 = CAM_PIN_D3;
    cfg.pin_d4        = CAM_PIN_D4;  cfg.pin_d5 = CAM_PIN_D5;
    cfg.pin_d6        = CAM_PIN_D6;  cfg.pin_d7 = CAM_PIN_D7;
    cfg.pin_xclk      = CAM_PIN_XCLK;
    cfg.pin_pclk      = CAM_PIN_PCLK;
    cfg.pin_vsync     = CAM_PIN_VSYNC;
    cfg.pin_href      = CAM_PIN_HREF;
    cfg.pin_sccb_sda  = sda;
    cfg.pin_sccb_scl  = scl;
    cfg.pin_pwdn      = CAM_PIN_PWDN;
    cfg.pin_reset     = CAM_PIN_RESET;
    cfg.xclk_freq_hz  = 20000000;
    cfg.frame_size    = FRAMESIZE_UXGA;
    cfg.pixel_format  = PIXFORMAT_JPEG;
    cfg.grab_mode     = CAMERA_GRAB_WHEN_EMPTY;
    cfg.fb_location   = CAMERA_FB_IN_PSRAM;
    cfg.jpeg_quality  = 12;
    cfg.fb_count      = 1;

    if (esp_psram_is_initialized()) {
        cfg.jpeg_quality = 10;
        cfg.fb_count     = 2;
        cfg.grab_mode    = CAMERA_GRAB_LATEST;
    } else {
        cfg.frame_size  = FRAMESIZE_SVGA;
        cfg.fb_location = CAMERA_FB_IN_DRAM;
    }

    if (esp_camera_init(&cfg) != ESP_OK) return false;

    sensor_t* s = esp_camera_sensor_get();
    if (!s) return false;
    if (s->id.PID == 0x3660) {
        s->set_vflip(s, 1);
        s->set_brightness(s, 1);
        s->set_saturation(s, -2);
    }
    if (cfg.pixel_format == PIXFORMAT_JPEG) {
        s->set_framesize(s, FRAMESIZE_QVGA);
        s->set_quality(s, 12);
    }
    return true;
}

bool camera_init(void) {
    s_mutex = xSemaphoreCreateMutex();

    ESP_LOGI(TAG, "PSRAM: %s", esp_psram_is_initialized() ? "OK" : "NOT FOUND");
    vTaskDelay(pdMS_TO_TICKS(150));

    if (init_once(CAM_PIN_SIOD, CAM_PIN_SIOC)) {
        s_camera_ok = true;
        ESP_LOGI(TAG, "OK");
        return true;
    }
    esp_camera_deinit();
    vTaskDelay(pdMS_TO_TICKS(80));
    if (init_once(CAM_PIN_SIOC, CAM_PIN_SIOD)) {
        s_camera_ok = true;
        ESP_LOGI(TAG, "OK (swapped SCCB)");
        return true;
    }
    esp_camera_deinit();
    ESP_LOGE(TAG, "FAILED — check Sense board seated");
    return false;
}

void camera_deinit(void) {
    esp_camera_deinit();
    s_camera_ok = false;
}

bool camera_is_ok(void) { return s_camera_ok; }

camera_fb_t* camera_fb_get_locked(uint32_t timeout_ms) {
    if (!s_camera_ok) return nullptr;
    if (s_mutex && xSemaphoreTake(s_mutex, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) return nullptr;
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) { if (s_mutex) xSemaphoreGive(s_mutex); return nullptr; }
    g_frame_id = g_frame_id + 1;
    return fb;
}

void camera_fb_release(camera_fb_t* fb) {
    if (!fb) return;
    esp_camera_fb_return(fb);
    if (s_mutex) xSemaphoreGive(s_mutex);
}
