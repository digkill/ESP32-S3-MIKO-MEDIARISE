#pragma once

#include <driver/i2c_master.h>
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
