#include "startup_media.h"
#include "config.h"
#include "audio_codec.h"

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <driver/i2s_std.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>

#define TAG_SM "StartupMedia"

// ── lodepng ───────────────────────────────────────────────────────────────────
// lodepng.c is already compiled as part of LVGL (CONFIG_LV_USE_LODEPNG=y).
// We only need the two decode functions.
extern "C" {
    unsigned lodepng_decode32(unsigned char** out, unsigned* w, unsigned* h,
                              const unsigned char* in, size_t insize);
    const char* lodepng_error_text(unsigned code);
}

// ── Splash screen ─────────────────────────────────────────────────────────────

void startup_show_splash(esp_lcd_panel_handle_t panel,
                         int display_w, int display_h,
                         const char* png_path)
{
    if (!panel || !png_path) return;

    FILE* f = fopen(png_path, "rb");
    if (!f) {
        ESP_LOGW(TAG_SM, "Splash PNG not found: %s", png_path);
        return;
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);
    if (fsize <= 8 || fsize > 4 * 1024 * 1024) {
        fclose(f);
        ESP_LOGW(TAG_SM, "Splash PNG invalid size: %ld", fsize);
        return;
    }

    unsigned char* png_buf = (unsigned char*)malloc((size_t)fsize);
    if (!png_buf) { fclose(f); return; }
    if (fread(png_buf, 1, (size_t)fsize, f) != (size_t)fsize) {
        free(png_buf); fclose(f); return;
    }
    fclose(f);

    unsigned char* rgba = nullptr;
    unsigned img_w = 0, img_h = 0;
    unsigned err = lodepng_decode32(&rgba, &img_w, &img_h, png_buf, (size_t)fsize);
    free(png_buf);
    if (err || !rgba) {
        ESP_LOGW(TAG_SM, "PNG decode error %u: %s", err, lodepng_error_text(err));
        return;
    }

    // Allocate an RGB565 framebuffer for the whole display in PSRAM.
    const size_t buf_bytes = (size_t)display_w * display_h * sizeof(uint16_t);
    uint16_t* rgb565 = (uint16_t*)heap_caps_malloc(
            buf_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!rgb565) {
        free(rgba);
        ESP_LOGE(TAG_SM, "No PSRAM for splash buffer (%u bytes)", (unsigned)buf_bytes);
        return;
    }
    memset(rgb565, 0, buf_bytes);

    // Scale image to fit the display (nearest-neighbour, black letterbox).
    const float scale = std::min(
            (float)display_w / (float)img_w,
            (float)display_h / (float)img_h);
    const int dst_w  = (int)((float)img_w * scale);
    const int dst_h  = (int)((float)img_h * scale);
    const int off_x  = (display_w - dst_w) / 2;
    const int off_y  = (display_h - dst_h) / 2;

    for (int dy = 0; dy < dst_h; dy++) {
        int sy = (int)((float)dy / scale);
        if (sy >= (int)img_h) sy = (int)img_h - 1;
        const int row_off = (off_y + dy) * display_w + off_x;
        for (int dx = 0; dx < dst_w; dx++) {
            int sx = (int)((float)dx / scale);
            if (sx >= (int)img_w) sx = (int)img_w - 1;
            const unsigned char* p = &rgba[(sy * img_w + sx) * 4];
            // Alpha-blend against black background.
            const uint8_t a = p[3];
            const uint8_t r = (uint8_t)((p[0] * a) / 255u);
            const uint8_t g = (uint8_t)((p[1] * a) / 255u);
            const uint8_t b = (uint8_t)((p[2] * a) / 255u);
            rgb565[row_off + dx] =
                (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
        }
    }
    free(rgba);

    esp_lcd_panel_draw_bitmap(panel, 0, 0, display_w, display_h, rgb565);
    heap_caps_free(rgb565);
    ESP_LOGI(TAG_SM, "Splash: %s (%ux%u → %dx%d+%d+%d)",
             png_path, img_w, img_h, dst_w, dst_h, off_x, off_y);
}

// ── WAV parser ────────────────────────────────────────────────────────────────

struct WavInfo {
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits_per_sample;
    uint32_t data_offset;
    uint32_t data_size;
};

static bool parse_wav_header(FILE* f, WavInfo* out)
{
    auto r32 = [&](uint32_t* v) -> bool {
        uint8_t b[4];
        if (fread(b, 1, 4, f) != 4) return false;
        *v = (uint32_t)b[0] | ((uint32_t)b[1]<<8) | ((uint32_t)b[2]<<16) | ((uint32_t)b[3]<<24);
        return true;
    };
    auto r16 = [&](uint16_t* v) -> bool {
        uint8_t b[2];
        if (fread(b, 1, 2, f) != 2) return false;
        *v = (uint16_t)((uint16_t)b[0] | ((uint16_t)b[1]<<8));
        return true;
    };

    char tag[4];
    uint32_t u32; uint16_t u16;

    if (fread(tag, 1, 4, f) != 4 || memcmp(tag, "RIFF", 4) != 0) return false;
    if (!r32(&u32)) return false;
    if (fread(tag, 1, 4, f) != 4 || memcmp(tag, "WAVE", 4) != 0) return false;

    out->sample_rate    = 0;
    out->data_offset    = 0;
    out->data_size      = 0;

    // Walk chunks until we find both "fmt " and "data".
    for (int itr = 0; itr < 32; itr++) {
        char chunk_id[4];
        uint32_t chunk_size;
        if (fread(chunk_id, 1, 4, f) != 4) break;
        if (!r32(&chunk_size)) break;

        if (memcmp(chunk_id, "fmt ", 4) == 0) {
            if (chunk_size < 16) return false;
            if (!r16(&u16) || u16 != 1) return false; // must be PCM (format = 1)
            if (!r16(&out->channels))       return false;
            if (!r32(&out->sample_rate))    return false;
            if (!r32(&u32)) return false; // byte rate
            if (!r16(&u16)) return false; // block align
            if (!r16(&out->bits_per_sample)) return false;
            if (chunk_size > 16) fseek(f, (long)(chunk_size - 16), SEEK_CUR);
        } else if (memcmp(chunk_id, "data", 4) == 0) {
            out->data_offset = (uint32_t)ftell(f);
            out->data_size   = chunk_size;
            return out->sample_rate > 0 && out->data_size > 0;
        } else {
            fseek(f, (long)chunk_size, SEEK_CUR);
        }
    }
    return false;
}

// ── WAV playback ──────────────────────────────────────────────────────────────

void startup_play_wav(i2c_master_bus_handle_t i2c_bus, const char* wav_path)
{
    if (!i2c_bus || !wav_path) return;

    FILE* f = fopen(wav_path, "rb");
    if (!f) {
        ESP_LOGW(TAG_SM, "Startup WAV not found: %s", wav_path);
        return;
    }

    WavInfo wav;
    if (!parse_wav_header(f, &wav)) {
        fclose(f);
        ESP_LOGW(TAG_SM, "Invalid WAV header: %s", wav_path);
        return;
    }
    if (wav.bits_per_sample != 16) {
        fclose(f);
        ESP_LOGW(TAG_SM, "WAV must be 16-bit PCM (got %u-bit)", wav.bits_per_sample);
        return;
    }
    ESP_LOGI(TAG_SM, "Playing WAV: %s (%" PRIu32 "Hz %uch %u-bit, %" PRIu32 " bytes)",
             wav_path, wav.sample_rate, wav.channels, wav.bits_per_sample, wav.data_size);

    // ── I2S TX channel (I2S_NUM_0, TX only) ──────────────────────────────────
    // BoxAudioCodec creates a full-duplex I2S_NUM_0 later; we delete ours first.
    i2s_chan_config_t chan_cfg = {
        .id               = I2S_NUM_0,
        .role             = I2S_ROLE_MASTER,
        .dma_desc_num     = AUDIO_CODEC_DMA_DESC_NUM,
        .dma_frame_num    = AUDIO_CODEC_DMA_FRAME_NUM,
        .auto_clear_after_cb  = true,
        .auto_clear_before_cb = false,
        .intr_priority    = 0,
    };
    i2s_chan_handle_t tx_handle = nullptr;
    if (i2s_new_channel(&chan_cfg, &tx_handle, nullptr) != ESP_OK) {
        fclose(f);
        ESP_LOGE(TAG_SM, "I2S channel create failed");
        return;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = {
            .sample_rate_hz = wav.sample_rate,
            .clk_src        = I2S_CLK_SRC_DEFAULT,
            .ext_clk_freq_hz = 0,
            .mclk_multiple  = I2S_MCLK_MULTIPLE_256,
        },
        .slot_cfg = {
            .data_bit_width = I2S_DATA_BIT_WIDTH_16BIT,
            .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO,
            .slot_mode      = I2S_SLOT_MODE_STEREO,
            .slot_mask      = I2S_STD_SLOT_BOTH,
            .ws_width       = I2S_DATA_BIT_WIDTH_16BIT,
            .ws_pol         = false,
            .bit_shift      = true,
            .left_align     = true,
            .big_endian     = false,
            .bit_order_lsb  = false,
        },
        .gpio_cfg = {
            .mclk = AUDIO_I2S_GPIO_MCLK,
            .bclk = AUDIO_I2S_GPIO_BCLK,
            .ws   = AUDIO_I2S_GPIO_WS,
            .dout = AUDIO_I2S_GPIO_DOUT,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };
    i2s_channel_init_std_mode(tx_handle, &std_cfg);
    i2s_channel_enable(tx_handle);

    // ── ES8311 output via esp_codec_dev ──────────────────────────────────────
    audio_codec_i2s_cfg_t i2s_data_cfg = {
        .port      = I2S_NUM_0,
        .rx_handle = nullptr,
        .tx_handle = tx_handle,
    };
    const audio_codec_data_if_t* data_if = audio_codec_new_i2s_data(&i2s_data_cfg);

    audio_codec_i2c_cfg_t i2c_cfg = {
        .port       = (uint8_t)1,
        .addr       = AUDIO_CODEC_ES8311_ADDR,
        .bus_handle = i2c_bus,
    };
    const audio_codec_ctrl_if_t* ctrl_if = audio_codec_new_i2c_ctrl(&i2c_cfg);
    const audio_codec_gpio_if_t* gpio_if = audio_codec_new_gpio();

    es8311_codec_cfg_t es8311_cfg = {};
    es8311_cfg.ctrl_if              = ctrl_if;
    es8311_cfg.gpio_if              = gpio_if;
    es8311_cfg.codec_mode           = ESP_CODEC_DEV_WORK_MODE_DAC;
    es8311_cfg.pa_pin               = AUDIO_CODEC_PA_PIN;
    es8311_cfg.use_mclk             = true;
    es8311_cfg.hw_gain.pa_voltage   = 5.0f;
    es8311_cfg.hw_gain.codec_dac_voltage = 3.3f;
    const audio_codec_if_t* codec_if = es8311_codec_new(&es8311_cfg);

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = codec_if,
        .data_if  = data_if,
    };
    esp_codec_dev_handle_t dev = esp_codec_dev_new(&dev_cfg);

    esp_codec_dev_sample_info_t info = {
        .bits_per_sample = 16,
        .channel         = (uint8_t)std::min((int)wav.channels, 2),
        .channel_mask    = 0,
        .sample_rate     = wav.sample_rate,
        .mclk_multiple   = 0,
    };
    esp_codec_dev_open(dev, &info);
    esp_codec_dev_set_out_vol(dev, 80);

    // ── Stream PCM ────────────────────────────────────────────────────────────
    constexpr size_t READ_SIZE = 2048; // bytes per read (mono or stereo)
    uint8_t*  pcm_in  = (uint8_t*)  malloc(READ_SIZE);
    int16_t*  pcm_out = (int16_t*)  malloc(READ_SIZE * 2); // stereo expansion

    if (pcm_in && pcm_out) {
        fseek(f, (long)wav.data_offset, SEEK_SET);
        uint32_t remaining = wav.data_size;

        while (remaining > 0) {
            size_t to_read = std::min((size_t)remaining, READ_SIZE);
            size_t got = fread(pcm_in, 1, to_read, f);
            if (got == 0) break;
            remaining -= (uint32_t)got;

            if (wav.channels == 1) {
                // Mono → stereo interleave
                const int16_t* src = (const int16_t*)pcm_in;
                int frames = (int)(got / 2);
                for (int i = 0; i < frames; i++) {
                    pcm_out[i * 2]     = src[i];
                    pcm_out[i * 2 + 1] = src[i];
                }
                esp_codec_dev_write(dev, pcm_out, frames * 4);
            } else {
                esp_codec_dev_write(dev, pcm_in, (int)got);
            }
        }
    }
    free(pcm_out);
    free(pcm_in);
    fclose(f);

    // ── Teardown — BoxAudioCodec will reclaim I2S_NUM_0 later ────────────────
    esp_codec_dev_close(dev);
    esp_codec_dev_delete(dev);
    audio_codec_delete_codec_if(codec_if);
    audio_codec_delete_ctrl_if(ctrl_if);
    audio_codec_delete_gpio_if(gpio_if);
    audio_codec_delete_data_if(data_if);

    i2s_channel_disable(tx_handle);
    i2s_del_channel(tx_handle);

    ESP_LOGI(TAG_SM, "Startup audio done");
}
