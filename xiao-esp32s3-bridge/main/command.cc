#include "command.h"
#include "config.h"
#include "camera.h"
#include "led_ring.h"
#include "servo.h"
#include "vl53l0x.h"
#include "c1001.h"
#include "espnow_bridge.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static const char* TAG = "CMD";

// ── Global state ──────────────────────────────────────────────────────────────
static bool s_power_save        = false;
static int64_t s_last_activity_us = 0;

bool   g_stream_enabled        = false;
uint32_t g_stream_interval_ms  = STREAM_DEFAULT_MS;
int64_t  g_last_stream_us      = 0;

#if ENABLE_VL53
bool     g_dist_stream_enabled = false;
uint32_t g_dist_stream_ms      = 500;
int64_t  g_last_dist_stream_us = 0;
#endif

#if ENABLE_C1001
bool     g_radar_stream_enabled = false;
uint32_t g_radar_stream_ms      = 500;
int64_t  g_last_radar_stream_us = 0;
#endif

// ── Helpers ───────────────────────────────────────────────────────────────────
static inline int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static void activity(void) { s_last_activity_us = esp_timer_get_time(); }

static int read_int(const char* s, int* pos) {
    while (s[*pos] == ' ') (*pos)++;
    int start = *pos;
    if (s[*pos] == '-') (*pos)++;
    while (isdigit((unsigned char)s[*pos])) (*pos)++;
    char tmp[16] = {};
    int len = *pos - start;
    if (len <= 0) return 0;
    if (len >= (int)sizeof(tmp)) len = sizeof(tmp) - 1;
    memcpy(tmp, s + start, len);
    return atoi(tmp);
}

static int clamp(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void enter_power_save(char* out, size_t sz) {
    if (s_power_save) return;
    s_power_save = true;
    g_stream_enabled = false;
#if ENABLE_VL53
    g_dist_stream_enabled = false;
#endif
#if ENABLE_C1001
    g_radar_stream_enabled = false;
#endif
    led_ring_rainbow_enable(false);
    led_ring_clear();
    servo_set_target(SERVO_HOME_ANGLE, SERVO_HOME_ANGLE);
    if (out) snprintf(out, sz, "OK POWER_SLEEP\n");
}

static void exit_power_save(void) {
    s_last_activity_us = esp_timer_get_time();
    if (!s_power_save) return;
    s_power_save = false;
    led_ring_set_default();  // снова бегущая радуга
}

bool command_power_save_active(void) { return s_power_save; }

void command_check_idle_timeout(void) {
    if (s_power_save) return;
    int64_t elapsed_ms = (esp_timer_get_time() - s_last_activity_us) / 1000;
    if (elapsed_ms >= (int64_t)XIAO_POWER_SAVE_TIMEOUT_MS) {
        char tmp[32];
        enter_power_save(tmp, sizeof(tmp));
    }
}

// ── Main command handler ──────────────────────────────────────────────────────
void command_execute(const char* raw, char* out, size_t sz) {
    // Uppercase copy
    char cmd[200] = {};
    size_t rlen = strlen(raw);
    if (rlen >= sizeof(cmd)) rlen = sizeof(cmd) - 1;
    for (size_t i = 0; i < rlen; i++)
        cmd[i] = (char)toupper((unsigned char)raw[i]);
    // Trim trailing whitespace
    int end = (int)strlen(cmd) - 1;
    while (end >= 0 && (cmd[end] == ' ' || cmd[end] == '\r' || cmd[end] == '\n')) cmd[end--] = '\0';
    if (cmd[0] == '\0') return;

    if (strcmp(cmd, "POWER SLEEP") != 0) exit_power_save();

    if (strcmp(cmd, "PING") == 0) {
        snprintf(out, sz, "PONG\n");

    } else if (strcmp(cmd, "HELP") == 0) {
        snprintf(out, sz,
            "CMDS: PING STATUS? WIFI? POWER SLEEP|WAKE\n"
            "SERVO <y> <p> YAW <a> PITCH <a>\n"
            "LED <r> <g> <b> PIX <i> <r> <g> <b>\n"
            "LEDDEFAULT LEDOFF LEDTEST\n"
#if ENABLE_VL53
            "DIST? DIST_RAW? DIST_STREAM ON [ms]|OFF\n"
#endif
#if ENABLE_C1001
            "RADAR? RADAR_STREAM ON [ms]|OFF\n"
#endif
            "FRAME STREAM ON [ms]|OFF\n");

    } else if (strcmp(cmd, "STATUS?") == 0) {
        uint8_t ch = 0;
        wifi_second_chan_t sec = WIFI_SECOND_CHAN_NONE;
        esp_wifi_get_channel(&ch, &sec);
        snprintf(out, sz,
            "STATUS camera=%d vl53=%d c1001=%d servos=%d led=%d"
            " stream=%d yaw=%d pitch=%d espnow=%d channel=%u power_save=%d\n",
            camera_is_ok()   ? 1 : 0,
            vl53_is_ok()     ? 1 : 0,
            c1001_is_ok()    ? 1 : 0,
            servo_is_ok()    ? 1 : 0,
            led_ring_is_ok() ? 1 : 0,
            g_stream_enabled ? 1 : 0,
            servo_get_yaw(), servo_get_pitch(),
            espnow_is_ok()   ? 1 : 0,
            (unsigned)ch,
            s_power_save ? 1 : 0);

    } else if (strcmp(cmd, "WIFI?") == 0) {
        esp_netif_ip_info_t ip;
        esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif && esp_netif_get_ip_info(netif, &ip) == ESP_OK) {
            snprintf(out, sz, "WIFI ip=" IPSTR " host=%s\n",
                     IP2STR(&ip.ip), WIFI_HOSTNAME);
        } else {
            snprintf(out, sz, "ERR WIFI_DOWN\n");
        }

    } else if (strcmp(cmd, "POWER SLEEP") == 0) {
        enter_power_save(out, sz);

    } else if (strcmp(cmd, "POWER WAKE") == 0) {
        snprintf(out, sz, "OK POWER_WAKE\n");

    } else if (strncmp(cmd, "SERVO ", 6) == 0) {
        int p = 6;
        int yaw   = read_int(cmd, &p);
        int pitch = read_int(cmd, &p);
        servo_set_target(yaw, pitch);
        // Echo the accepted (clamped) targets: the move is asynchronous, so
        // the current position still shows the previous angles here.
        snprintf(out, sz, "OK SERVO yaw=%d pitch=%d\n",
                 servo_get_target_yaw(), servo_get_target_pitch());

    } else if (strncmp(cmd, "YAW ", 4) == 0) {
        int p = 4; servo_set_target(read_int(cmd, &p), servo_get_target_pitch());
        snprintf(out, sz, "OK YAW %d\n", servo_get_target_yaw());

    } else if (strncmp(cmd, "PITCH ", 6) == 0) {
        int p = 6; servo_set_target(servo_get_target_yaw(), read_int(cmd, &p));
        snprintf(out, sz, "OK PITCH %d\n", servo_get_target_pitch());

    } else if (strncmp(cmd, "LED ", 4) == 0) {
        if (!led_ring_is_ok()) { snprintf(out, sz, "ERR LED_RING_DISABLED\n"); return; }
        int p = 4;
        int r = clamp(read_int(cmd, &p), 0, 255);
        int g = clamp(read_int(cmd, &p), 0, 255);
        int b = clamp(read_int(cmd, &p), 0, 255);
        led_ring_set_all(r, g, b);
        snprintf(out, sz, "OK LED\n");

    } else if (strncmp(cmd, "PIX ", 4) == 0) {
        if (!led_ring_is_ok()) { snprintf(out, sz, "ERR LED_RING_DISABLED\n"); return; }
        int p = 4;
        int idx = read_int(cmd, &p);
        int r = clamp(read_int(cmd, &p), 0, 255);
        int g = clamp(read_int(cmd, &p), 0, 255);
        int b = clamp(read_int(cmd, &p), 0, 255);
        if (idx < 0 || idx >= LED_RING_COUNT) { snprintf(out, sz, "ERR PIX_INDEX\n"); return; }
        led_ring_set_pixel(idx, r, g, b);
        snprintf(out, sz, "OK PIX\n");

    } else if (strcmp(cmd, "LEDOFF") == 0) {
        led_ring_clear();
        snprintf(out, sz, "OK LEDOFF\n");

    } else if (strcmp(cmd, "LEDDEFAULT") == 0) {
        led_ring_set_default();
        snprintf(out, sz, "OK LEDDEFAULT\n");

    } else if (strcmp(cmd, "LEDTEST") == 0) {
        led_ring_test();
        snprintf(out, sz, "OK LEDTEST\n");

    } else if (strcmp(cmd, "DIST?") == 0) {
#if ENABLE_VL53
        uint16_t mm = 0; uint8_t st = 0;
        if (!vl53_read(&mm, &st)) { snprintf(out, sz, "ERR VL53L0X_NOT_READY\n"); }
        else if (st == 4)         { snprintf(out, sz, "ERR DIST_OUT_OF_RANGE\n"); }
        else                      { snprintf(out, sz, "DIST %u\n", (unsigned)mm); }
#else
        snprintf(out, sz, "ERR VL53_DISABLED\n");
#endif

    } else if (strcmp(cmd, "DIST_RAW?") == 0) {
#if ENABLE_VL53
        uint16_t mm = 0; uint8_t st = 0xFF;
        if (!vl53_read(&mm, &st)) snprintf(out, sz, "ERR VL53L0X_NOT_READY\n");
        else snprintf(out, sz, "DIST_RAW status=%u mm=%u\n", (unsigned)st, (unsigned)mm);
#else
        snprintf(out, sz, "ERR VL53_DISABLED\n");
#endif

    } else if (strncmp(cmd, "DIST_STREAM ON", 14) == 0) {
#if ENABLE_VL53
        int p = 14; int ms = read_int(cmd, &p);
        if (ms > 0) g_dist_stream_ms = (uint32_t)clamp(ms, 50, 10000);
        g_dist_stream_enabled = true; g_last_dist_stream_us = 0;
        snprintf(out, sz, "OK DIST_STREAM interval=%lu\n", (unsigned long)g_dist_stream_ms);
#else
        snprintf(out, sz, "ERR VL53_DISABLED\n");
#endif

    } else if (strcmp(cmd, "DIST_STREAM OFF") == 0) {
#if ENABLE_VL53
        g_dist_stream_enabled = false;
        snprintf(out, sz, "OK DIST_STREAM_OFF\n");
#else
        snprintf(out, sz, "ERR VL53_DISABLED\n");
#endif

    } else if (strcmp(cmd, "RADAR?") == 0) {
#if ENABLE_C1001
        uint8_t pres = 0, mot = 0; uint16_t rng = 0;
        if (!c1001_query(&pres, &mot, &rng)) { snprintf(out, sz, "ERR C1001_NOT_READY\n"); }
        else snprintf(out, sz, "RADAR presence=%u motion=%u range=%u\n",
                     (unsigned)pres, (unsigned)mot, (unsigned)rng);
#else
        snprintf(out, sz, "ERR C1001_DISABLED\n");
#endif

    } else if (strncmp(cmd, "RADAR_STREAM ON", 15) == 0) {
#if ENABLE_C1001
        int p = 15; int ms = read_int(cmd, &p);
        if (ms > 0) g_radar_stream_ms = (uint32_t)clamp(ms, 50, 10000);
        g_radar_stream_enabled = true; g_last_radar_stream_us = 0;
        snprintf(out, sz, "OK RADAR_STREAM interval=%lu\n", (unsigned long)g_radar_stream_ms);
#else
        snprintf(out, sz, "ERR C1001_DISABLED\n");
#endif

    } else if (strcmp(cmd, "RADAR_STREAM OFF") == 0) {
#if ENABLE_C1001
        g_radar_stream_enabled = false;
        snprintf(out, sz, "OK RADAR_STREAM_OFF\n");
#else
        snprintf(out, sz, "ERR C1001_DISABLED\n");
#endif

    } else if (strcmp(cmd, "FRAME") == 0) {
        snprintf(out, sz, "ERR USE_HTTP_FOR_FRAMES\n"); // frames via HTTP only

    } else if (strncmp(cmd, "STREAM ON", 9) == 0) {
        int p = 9; int ms = read_int(cmd, &p);
        if (ms > 0) g_stream_interval_ms = (uint32_t)clamp(ms, 100, 10000);
        g_stream_enabled = true; g_last_stream_us = 0;
        snprintf(out, sz, "OK STREAM interval=%lu\n", (unsigned long)g_stream_interval_ms);

    } else if (strcmp(cmd, "STREAM OFF") == 0) {
        g_stream_enabled = false;
        snprintf(out, sz, "OK STREAM_OFF\n");

    } else {
        snprintf(out, sz, "ERR UNKNOWN_CMD\n");
    }
    (void)TAG;
    activity();
}
