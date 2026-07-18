#pragma once

#include <driver/i2c_master.h>
#include <esp_lcd_panel_io.h>
#include <esp_lcd_panel_ops.h>

// Show a PNG image from the SD card on the raw LCD panel (bypasses LVGL).
// Call this AFTER the hardware panel is initialised but BEFORE LVGL starts.
// png_path  : absolute path, e.g. "/sdcard/assets/logo_mediarise.png"
void startup_show_splash(esp_lcd_panel_handle_t panel,
                         int display_w, int display_h,
                         const char* png_path);

// Play a 16-bit stereo (or mono) PCM WAV file from the SD card.
// Opens a temporary I2S+ES8311 channel on I2S_NUM_0 and tears it down
// completely when done, so BoxAudioCodec can reclaim I2S_NUM_0 later.
// Call this AFTER the hardware panel is initialised but BEFORE LVGL starts.
// wav_path  : absolute path, e.g. "/sdcard/assets/load.wav"
//
// To convert your MP3 to a compatible WAV:
//   ffmpeg -i load.mp3 -ar 24000 -ac 2 -acodec pcm_s16le load.wav
void startup_play_wav(i2c_master_bus_handle_t i2c_bus, const char* wav_path);

// Play an MJPEG MP4 video from the SD card on the raw LCD panel.
// Draws directly to the panel, bypassing LVGL:
//  - at startup, call AFTER the hardware panel is initialised but BEFORE
//    LVGL starts;
//  - at runtime, hold lvgl_port_lock() for the whole call and invalidate the
//    screen afterwards so LVGL repaints over the last video frame.
// mp4_path  : absolute path, e.g. "/sdcard/intro.mp4"
//
// The file must use MJPEG video (not H.264), no audio track:
//   ffmpeg -y -i input.mp4 -an -vf "scale=502:410:flags=lanczos" -c:v mjpeg -q:v 8 -pix_fmt yuvj420p -r 12 out.mp4
void startup_play_mp4(esp_lcd_panel_handle_t panel,
                      int display_w, int display_h,
                      const char* mp4_path);

// Same as startup_play_mp4, but streams stripes through the caller-provided
// DMA-capable buffer instead of allocating one (at runtime the internal DMA
// heap is exhausted, so borrow LVGL's idle draw buffer while holding the
// LVGL lock). Needs at least 4 display rows worth of pixels.
void startup_play_mp4_with_blit_buffer(esp_lcd_panel_handle_t panel,
                                       int display_w, int display_h,
                                       const char* mp4_path,
                                       void* blit_mem, size_t blit_bytes);
