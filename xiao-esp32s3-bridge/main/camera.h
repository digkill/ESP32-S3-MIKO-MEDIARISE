#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_camera.h"

#ifdef __cplusplus
extern "C" {
#endif

bool camera_init(void);
void camera_deinit(void);
bool camera_is_ok(void);

// Returns a locked framebuffer. Caller must call camera_fb_release() when done.
camera_fb_t* camera_fb_get_locked(uint32_t timeout_ms);
void         camera_fb_release(camera_fb_t* fb);

extern volatile uint32_t g_frame_id;

#ifdef __cplusplus
}
#endif
