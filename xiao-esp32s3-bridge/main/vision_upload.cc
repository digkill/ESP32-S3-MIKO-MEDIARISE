#include "vision_upload.h"
#include "camera.h"
#include "command.h"
#include "config.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>
#include <string.h>

#if ENABLE_VISION_UPLOAD

static const char* TAG = "VISION";
static uint32_t s_upload_failures = 0;

static void upload_frame(void) {
    if (command_power_save_active()) return;

    camera_fb_t* fb = camera_fb_get_locked(2000);
    if (!fb) return;

    uint8_t* frame = (uint8_t*)malloc(fb->len);
    if (!frame) {
        ESP_LOGW(TAG, "no memory for frame copy bytes=%u", (unsigned)fb->len);
        camera_fb_release(fb);
        return;
    }
    size_t frame_len = fb->len;
    memcpy(frame, fb->buf, frame_len);
    camera_fb_release(fb);

    esp_http_client_config_t cfg = {};
    cfg.url     = VISION_UPLOAD_URL;
    cfg.timeout_ms = 3000;
    cfg.method  = HTTP_METHOD_POST;

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    esp_http_client_set_header(client, "Content-Type",  "image/jpeg");
    esp_http_client_set_header(client, "X-Device-Id",   VISION_DEVICE_ID);
    esp_http_client_set_post_field(client, (const char*)frame, (int)frame_len);

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status >= 200 && status < 300) {
            s_upload_failures = 0;
            ESP_LOGI(TAG, "frame accepted bytes=%u", (unsigned)frame_len);
        } else {
            s_upload_failures++;
            ESP_LOGW(TAG, "upload failed status=%d", status);
        }
    } else {
        s_upload_failures++;
        if (s_upload_failures <= 3 || (s_upload_failures % 4) == 0) {
            ESP_LOGW(TAG, "upload error: %s failures=%lu",
                     esp_err_to_name(err), (unsigned long)s_upload_failures);
        }
    }
    esp_http_client_cleanup(client);
    free(frame);
}

static void vision_task(void*) {
    int64_t last_upload_us = 0;
    while (true) {
        int64_t now = esp_timer_get_time();
        if ((now - last_upload_us) / 1000 >= (int64_t)VISION_UPLOAD_INTERVAL_MS) {
            last_upload_us = now;
            upload_frame();
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

bool vision_upload_start_task(void) {
    if (!camera_is_ok()) return false;
    bool ok = xTaskCreate(vision_task, "vision_upload", 6144, nullptr, 1, nullptr) == pdPASS;
    if (ok) ESP_LOGI(TAG, "started interval=%lums url=%s",
                    (long)VISION_UPLOAD_INTERVAL_MS, VISION_UPLOAD_URL);
    else    ESP_LOGE(TAG, "task create failed");
    return ok;
}

#else // !ENABLE_VISION_UPLOAD

bool vision_upload_start_task(void) { return true; }

#endif
