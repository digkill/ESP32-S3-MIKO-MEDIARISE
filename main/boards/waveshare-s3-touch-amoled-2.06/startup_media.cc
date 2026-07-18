#include "startup_media.h"
#include "config.h"
#include "audio_codec.h"
#include "jpeg_to_image.h"

#include <esp_log.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <esp_lcd_panel_io.h>
#include <driver/i2s_std.h>
#include <esp_codec_dev.h>
#include <esp_codec_dev_defaults.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <vector>

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
    esp_codec_dev_set_out_vol(dev, AUDIO_MAX_OUTPUT_VOLUME);

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

// ── MP4 (MJPEG) intro video ───────────────────────────────────────────────────

struct Mp4Atom {
    uint64_t offset;
    uint64_t size;
};

static uint32_t read_be32(const uint8_t* p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static uint64_t read_be64(const uint8_t* p)
{
    return ((uint64_t)read_be32(p) << 32) | read_be32(p + 4);
}

static bool find_atom_in_range(FILE* f, const char* type, Mp4Atom* out, uint64_t range_start, uint64_t range_end)
{
    uint64_t pos = range_start;

    while (pos + 8 <= range_end) {
        fseek(f, (long)pos, SEEK_SET);
        uint8_t header[8];
        if (fread(header, 1, 8, f) != 8) {
            return false;
        }

        uint64_t size = read_be32(header);
        char atom_type[5] = {};
        memcpy(atom_type, header + 4, 4);

        uint64_t header_size = 8;
        if (size == 1) {
            uint8_t ext[8];
            if (fread(ext, 1, 8, f) != 8) {
                return false;
            }
            size = read_be64(ext);
            header_size = 16;
        }

        if (size < header_size || pos + size > range_end) {
            return false;
        }

        if (strcmp(atom_type, type) == 0) {
            out->offset = pos + header_size;
            out->size = size - header_size;
            return true;
        }

        const bool container =
            strcmp(atom_type, "moov") == 0 ||
            strcmp(atom_type, "trak") == 0 ||
            strcmp(atom_type, "mdia") == 0 ||
            strcmp(atom_type, "minf") == 0 ||
            strcmp(atom_type, "stbl") == 0;

        if (container && find_atom_in_range(f, type, out, pos + header_size, pos + size)) {
            return true;
        }

        pos += size;
    }

    return false;
}

static bool find_top_level_atom(FILE* f, const char* type, Mp4Atom* out)
{
    fseek(f, 0, SEEK_END);
    const uint64_t file_size = (uint64_t)ftell(f);
    return find_atom_in_range(f, type, out, 0, file_size);
}

static bool read_mp4_duration_ms(FILE* f, uint32_t* duration_ms)
{
    Mp4Atom mvhd;
    if (!find_top_level_atom(f, "mvhd", &mvhd)) {
        return false;
    }

    fseek(f, (long)mvhd.offset, SEEK_SET);
    uint8_t header[32];
    if (fread(header, 1, sizeof(header), f) != sizeof(header)) {
        return false;
    }

    const uint8_t version = header[0];
    uint32_t timescale = 0;
    uint64_t duration = 0;

    if (version == 0) {
        timescale = read_be32(header + 12);
        duration = read_be32(header + 16);
    } else if (version == 1) {
        duration = read_be64(header + 8);
        timescale = read_be32(header + 20);
    }

    if (timescale == 0 || duration == 0) {
        return false;
    }

    *duration_ms = (uint32_t)((duration * 1000ULL) / timescale);
    return true;
}

static bool collect_mjpeg_frames(const uint8_t* data, size_t size,
                                 std::vector<std::pair<uint32_t, uint32_t>>& frames)
{
    if (!data || size < 4) {
        return false;
    }

    bool in_frame = false;
    uint32_t frame_start = 0;

    for (size_t i = 1; i < size; ++i) {
        if (!in_frame && data[i - 1] == 0xFF && data[i] == 0xD8) {
            in_frame = true;
            frame_start = (uint32_t)(i - 1);
        } else if (in_frame && data[i - 1] == 0xFF && data[i] == 0xD9) {
            const uint32_t frame_end = (uint32_t)(i + 1);
            const uint32_t frame_size = frame_end - frame_start;
            if (frame_size > 4 && frame_size < 2 * 1024 * 1024) {
                frames.emplace_back(frame_start, frame_size);
            }
            in_frame = false;
        }
    }

    return !frames.empty();
}

// ── Stripe blitter ────────────────────────────────────────────────────────────
// esp_lcd_panel_draw_bitmap DMA-reads the pixel buffer while the CPU keeps
// running. Feeding the whole ~411 KB PSRAM framebuffer to the 40 MHz QSPI
// panel starves the SPI FIFO ("spi_master: DMA TX underflow detected") and
// permanently wedges the panel IO transaction queue, so every later draw
// fails with "recycle spi transactions failed". Instead the frame is sent as
// horizontal stripes copied through small internal-RAM bounce buffers.
//
// Buffer-reuse safety without any panel IO callback (so the player can also
// run at runtime without disturbing the callback LVGL registers on this IO):
// each draw_bitmap starts with CASET/RASET params, and panel_io_spi_tx_param
// recycles (waits for) every queued color transaction first. So when
// draw_bitmap(stripe N) returns, stripe N-1 has fully left its bounce buffer,
// and alternating two buffers is enough.

struct StripeBlitter {
    esp_lcd_panel_handle_t panel = nullptr;
    int display_w = 0;
    int stripe_rows = 0;
    uint16_t* bounce[2] = {nullptr, nullptr};
    int next_buf = 0;
    bool owns_buffers = false;
};

static void blitter_deinit(StripeBlitter* b)
{
    // The last stripe may still be streaming out of a bounce buffer
    // (~24 KB @ 40 MHz QSPI takes ~1.5 ms); let it finish before the memory
    // is freed or handed back to LVGL.
    vTaskDelay(pdMS_TO_TICKS(10));
    for (auto& buf : b->bounce) {
        if (b->owns_buffers) {
            heap_caps_free(buf);
        }
        buf = nullptr;
    }
}

static bool blitter_init(StripeBlitter* b,
                         esp_lcd_panel_handle_t panel,
                         int display_w,
                         void* external_mem, size_t external_bytes)
{
    b->panel = panel;
    b->display_w = display_w;

    const size_t row_bytes = (size_t)display_w * sizeof(uint16_t);

    // Runtime playback borrows LVGL's idle draw buffer (LVGL is locked for
    // the whole clip): the internal DMA heap is too depleted by then to
    // allocate anything. The buffer is split into a pair of bounce halves.
    // The SH8601 needs 2-pixel-aligned addresses (see rounder_event_cb in
    // the board display code), so the stripe height stays even.
    if (external_mem != nullptr) {
        const int rows = (int)(external_bytes / 2 / row_bytes) & ~1;
        if (rows < 2) {
            ESP_LOGE(TAG_SM, "External blit buffer too small: %u bytes",
                     (unsigned)external_bytes);
            return false;
        }
        b->stripe_rows = rows;
        b->bounce[0] = (uint16_t*)external_mem;
        b->bounce[1] = (uint16_t*)((uint8_t*)external_mem + (size_t)rows * row_bytes);
        b->owns_buffers = false;
        return true;
    }

    // Startup path: internal RAM is still plentiful, allocate ~24 KB stripes.
    b->owns_buffers = true;
    int rows = std::max(8, (int)(24 * 1024 / row_bytes)) & ~1;
    while (rows >= 2) {
        for (auto& buf : b->bounce) {
            buf = (uint16_t*)heap_caps_malloc((size_t)rows * row_bytes, MALLOC_CAP_DMA);
        }
        if (b->bounce[0] && b->bounce[1]) {
            b->stripe_rows = rows;
            if (rows < 8) {
                ESP_LOGW(TAG_SM, "Low DMA memory: using %d-row stripes", rows);
            }
            return true;
        }
        for (auto& buf : b->bounce) {
            heap_caps_free(buf);
            buf = nullptr;
        }
        rows = (rows / 2) & ~1;
    }
    ESP_LOGE(TAG_SM, "Stripe blitter init failed: internal DMA heap exhausted "
             "(largest block %u bytes)",
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
    return false;
}

static void blitter_blit(StripeBlitter* b, const uint16_t* framebuffer, int display_h)
{
    for (int y = 0; y < display_h; y += b->stripe_rows) {
        const int rows = std::min(b->stripe_rows, display_h - y);
        // Alternating two buffers is safe: draw_bitmap's leading CASET/RASET
        // params recycle the previously queued color transaction, so by the
        // time we come back to a buffer its transfer has finished.
        uint16_t* buf = b->bounce[b->next_buf];
        b->next_buf ^= 1;
        memcpy(buf, framebuffer + (size_t)y * b->display_w,
               (size_t)rows * b->display_w * sizeof(uint16_t));
        esp_lcd_panel_draw_bitmap(b->panel, 0, y, b->display_w, y + rows, buf);
    }
}

// The JPEG decoder pads its output up to whole MCUs (multiples of 8/16); the
// padding rows/columns contain garbage (typically green). Read the true frame
// dimensions from the SOF marker so rendering can crop the padding away.
static bool jpeg_sof_dimensions(const uint8_t* jpeg, size_t len, int* w, int* h)
{
    size_t i = 2; // skip SOI
    while (i + 9 < len && jpeg[i] == 0xFF) {
        const uint8_t marker = jpeg[i + 1];
        if (marker == 0xD8 || marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
            i += 2;
            continue;
        }
        if (marker >= 0xC0 && marker <= 0xCF &&
            marker != 0xC4 && marker != 0xC8 && marker != 0xCC) { // SOFn
            *h = ((int)jpeg[i + 5] << 8) | jpeg[i + 6];
            *w = ((int)jpeg[i + 7] << 8) | jpeg[i + 8];
            return *w > 0 && *h > 0;
        }
        const size_t seg_len = ((size_t)jpeg[i + 2] << 8) | jpeg[i + 3];
        if (seg_len < 2) {
            return false;
        }
        i += 2 + seg_len;
    }
    return false;
}

// Rendering bypasses LVGL; the panel expects the same byte order as
// lvgl_port with swap_bytes=1 (see SpiLcdDisplay).
static inline uint16_t panel_pixel(uint16_t le)
{
    return __builtin_bswap16(le);
}

static void render_rgb565_frame(int display_w, int display_h,
                                uint16_t* framebuffer,
                                const uint8_t* rgb565,
                                int img_w, int img_h, int src_stride_px)
{
    const bool rotate_cw = display_h > display_w;
    const int view_w = rotate_cw ? display_h : display_w;
    const int view_h = rotate_cw ? display_w : display_h;
    const uint16_t* src = (const uint16_t*)rgb565;

    if (rotate_cw && img_w == view_w && img_h == view_h) {
        // Full-screen rotation covers every framebuffer pixel, no clear
        // needed. Work in tiles: the rotated writes are strided by a whole
        // display row, so tiling keeps them inside the cache instead of
        // thrashing PSRAM (which made this path ~10x slower).
        constexpr int TILE = 32;
        for (int ty = 0; ty < img_h; ty += TILE) {
            const int ty_end = std::min(ty + TILE, img_h);
            for (int tx = 0; tx < img_w; tx += TILE) {
                const int tx_end = std::min(tx + TILE, img_w);
                for (int ly = ty; ly < ty_end; ++ly) {
                    const uint16_t* row = src + (size_t)ly * src_stride_px;
                    const int px = view_h - 1 - ly;
                    for (int lx = tx; lx < tx_end; ++lx) {
                        framebuffer[(size_t)lx * display_w + px] = panel_pixel(row[lx]);
                    }
                }
            }
        }
    } else {
        // Stretch to the full landscape view — the video always spans the
        // whole display, no letterbox bars. Covers every pixel, no clear
        // needed. Same tiling as above to keep the rotated writes cached.
        std::vector<int> sx_map(view_w);
        for (int dx = 0; dx < view_w; ++dx) {
            sx_map[dx] = (int)(((int64_t)dx * img_w) / view_w);
        }

        constexpr int TILE = 32;
        for (int tdy = 0; tdy < view_h; tdy += TILE) {
            const int tdy_end = std::min(tdy + TILE, view_h);
            for (int tdx = 0; tdx < view_w; tdx += TILE) {
                const int tdx_end = std::min(tdx + TILE, view_w);
                for (int dy = tdy; dy < tdy_end; ++dy) {
                    const int sy = (int)(((int64_t)dy * img_h) / view_h);
                    const uint16_t* src_row = src + (size_t)sy * src_stride_px;
                    if (rotate_cw) {
                        const int px = view_h - 1 - dy;
                        for (int dx = tdx; dx < tdx_end; ++dx) {
                            framebuffer[(size_t)dx * display_w + px] = panel_pixel(src_row[sx_map[dx]]);
                        }
                    } else {
                        uint16_t* dst_row = framebuffer + (size_t)dy * display_w;
                        for (int dx = tdx; dx < tdx_end; ++dx) {
                            dst_row[dx] = panel_pixel(src_row[sx_map[dx]]);
                        }
                    }
                }
            }
        }
    }

}

static void play_mp4_impl(esp_lcd_panel_handle_t panel,
                          int display_w, int display_h,
                          const char* mp4_path,
                          void* blit_mem, size_t blit_bytes)
{
    if (!panel || !mp4_path) {
        return;
    }

    FILE* f = fopen(mp4_path, "rb");
    if (!f) {
        ESP_LOGW(TAG_SM, "MP4 not found: %s", mp4_path);
        return;
    }

    Mp4Atom mdat;
    if (!find_top_level_atom(f, "mdat", &mdat)) {
        fclose(f);
        ESP_LOGW(TAG_SM, "MP4 mdat atom not found: %s", mp4_path);
        return;
    }

    if (mdat.size == 0 || mdat.size > 8 * 1024 * 1024) {
        fclose(f);
        ESP_LOGW(TAG_SM, "MP4 mdat too large for intro playback: %" PRIu64 " bytes", mdat.size);
        return;
    }

    ESP_LOGI(TAG_SM, "Loading intro MP4 mdat (%" PRIu64 " bytes)...", mdat.size);
    uint8_t* mdat_buf = (uint8_t*)heap_caps_malloc((size_t)mdat.size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!mdat_buf) {
        fclose(f);
        ESP_LOGE(TAG_SM, "No PSRAM for intro MP4 scan buffer");
        return;
    }

    fseek(f, (long)mdat.offset, SEEK_SET);
    if (fread(mdat_buf, 1, (size_t)mdat.size, f) != mdat.size) {
        heap_caps_free(mdat_buf);
        fclose(f);
        ESP_LOGW(TAG_SM, "Failed to read MP4 mdat: %s", mp4_path);
        return;
    }

    std::vector<std::pair<uint32_t, uint32_t>> frames;
    if (!collect_mjpeg_frames(mdat_buf, (size_t)mdat.size, frames)) {
        heap_caps_free(mdat_buf);
        fclose(f);
        ESP_LOGW(TAG_SM, "No MJPEG frames in: %s (use ffmpeg -c:v mjpeg)", mp4_path);
        return;
    }

    uint32_t duration_ms = 0;
    uint32_t frame_delay_ms = 66;
    if (read_mp4_duration_ms(f, &duration_ms) && duration_ms > 0) {
        frame_delay_ms = std::max<uint32_t>(1, duration_ms / (uint32_t)frames.size());
    }
    fclose(f);

    const size_t fb_bytes = (size_t)display_w * display_h * sizeof(uint16_t);
    uint16_t* framebuffer = (uint16_t*)heap_caps_malloc(fb_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!framebuffer) {
        heap_caps_free(mdat_buf);
        ESP_LOGE(TAG_SM, "No PSRAM for intro video framebuffer");
        return;
    }

    StripeBlitter blitter;
    if (!blitter_init(&blitter, panel, display_w, blit_mem, blit_bytes)) {
        heap_caps_free(framebuffer);
        heap_caps_free(mdat_buf);
        return;
    }

    ESP_LOGI(TAG_SM, "Playing MP4: %s (%u frames, %" PRIu32 " ms/frame)",
             mp4_path, (unsigned)frames.size(), frame_delay_ms);

    // At runtime this runs on the TWDT-subscribed main loop for the whole
    // clip; pet the watchdog so it doesn't spam backtraces. At startup the
    // caller is not subscribed and resetting would log an error every frame.
    const bool wdt_subscribed = esp_task_wdt_status(nullptr) == ESP_OK;

    size_t shown = 0;
    const int64_t frame_start_us = esp_timer_get_time();
    for (size_t i = 0; i < frames.size(); ++i) {
        if (wdt_subscribed) {
            esp_task_wdt_reset();
        }

        // Drop late frames (decode can be slower than the nominal frame rate)
        // so the intro keeps real-time pace instead of playing in slow motion.
        if (i + 1 < frames.size()) {
            const int64_t elapsed_us = esp_timer_get_time() - frame_start_us;
            const size_t due = (size_t)(elapsed_us / ((int64_t)frame_delay_ms * 1000LL));
            if (i < due) {
                continue;
            }
        }

        const auto& frame = frames[i];
        const uint8_t* jpeg_ptr = mdat_buf + frame.first;

        const int64_t t_decode = esp_timer_get_time();
        uint8_t* rgb565 = nullptr;
        size_t rgb_len = 0;
        size_t img_w = 0;
        size_t img_h = 0;
        size_t stride = 0;
        if (jpeg_to_image(jpeg_ptr, frame.second, &rgb565, &rgb_len, &img_w, &img_h, &stride) != ESP_OK ||
            !rgb565 || img_w == 0 || img_h == 0) {
            heap_caps_free(rgb565);
            ESP_LOGW(TAG_SM, "JPEG decode failed for intro frame %u", (unsigned)i);
            continue;
        }

        // Crop MCU padding (green garbage) using the true SOF dimensions.
        int true_w = (int)img_w;
        int true_h = (int)img_h;
        jpeg_sof_dimensions(jpeg_ptr, frame.second, &true_w, &true_h);
        true_w = std::min(true_w, (int)img_w);
        true_h = std::min(true_h, (int)img_h);

        const int64_t t_render = esp_timer_get_time();
        render_rgb565_frame(display_w, display_h, framebuffer, rgb565,
                            true_w, true_h, (int)(stride / sizeof(uint16_t)));
        heap_caps_free(rgb565);
        const int64_t t_blit = esp_timer_get_time();
        blitter_blit(&blitter, framebuffer, display_h);

        if (shown == 0) {
            ESP_LOGI(TAG_SM, "First frame: decoded %ux%u (SOF %dx%d), "
                     "decode %d ms, render %d ms, blit %d ms",
                     (unsigned)img_w, (unsigned)img_h, true_w, true_h,
                     (int)((t_render - t_decode) / 1000),
                     (int)((t_blit - t_render) / 1000),
                     (int)((esp_timer_get_time() - t_blit) / 1000));
        }
        shown++;

        const int64_t target_us = frame_start_us + (int64_t)(i + 1) * (int64_t)frame_delay_ms * 1000LL;
        const int64_t now_us = esp_timer_get_time();
        const int64_t wait_us = target_us - now_us;
        if (wait_us > 0) {
            vTaskDelay(pdMS_TO_TICKS((uint32_t)((wait_us + 999) / 1000)));
        }
    }

    blitter_deinit(&blitter);
    heap_caps_free(framebuffer);
    heap_caps_free(mdat_buf);
    ESP_LOGI(TAG_SM, "MP4 done (%u/%u frames shown)",
             (unsigned)shown, (unsigned)frames.size());
}

void startup_play_mp4(esp_lcd_panel_handle_t panel,
                      int display_w, int display_h,
                      const char* mp4_path)
{
    play_mp4_impl(panel, display_w, display_h, mp4_path, nullptr, 0);
}

void startup_play_mp4_with_blit_buffer(esp_lcd_panel_handle_t panel,
                                       int display_w, int display_h,
                                       const char* mp4_path,
                                       void* blit_mem, size_t blit_bytes)
{
    play_mp4_impl(panel, display_w, display_h, mp4_path, blit_mem, blit_bytes);
}
