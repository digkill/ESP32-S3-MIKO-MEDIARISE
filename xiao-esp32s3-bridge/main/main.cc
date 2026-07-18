#include "config.h"
#include "camera.h"
#include "led_ring.h"
#include "servo.h"
#include "vl53l0x.h"
#include "c1001.h"
#include "espnow_bridge.h"
#include "http_server.h"
#include "command.h"
#include "vision_upload.h"

#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#if CONFIG_IDF_TARGET_ESP32
#include "driver/uart.h"       // classic ESP32 has no USB-Serial/JTAG
#else
#include "driver/usb_serial_jtag.h"
#endif

#include <stdio.h>
#include <string.h>

static const char* TAG = "MAIN";

// ── Global flags ──────────────────────────────────────────────────────────────
extern bool   g_stream_enabled;
extern int64_t g_last_stream_us;
extern uint32_t g_stream_interval_ms;
#if ENABLE_VL53
extern bool     g_dist_stream_enabled;
extern uint32_t g_dist_stream_ms;
extern int64_t  g_last_dist_stream_us;
#endif
#if ENABLE_C1001
extern bool     g_radar_stream_enabled;
extern uint32_t g_radar_stream_ms;
extern int64_t  g_last_radar_stream_us;
#endif

// D6/D7 (GPIO43/44) share the USB Serial/JTAG block — C1001 on those pins
// cannot coexist with the USB command console.
#if ENABLE_C1001 && !defined(BOARD_ESP32CAM) && \
    ((C1001_TX_GPIO == 43) || (C1001_TX_GPIO == 44) || \
     (C1001_RX_GPIO == 43) || (C1001_RX_GPIO == 44))
#define XIAO_CONSOLE_USB_SERIAL 0
#else
#define XIAO_CONSOLE_USB_SERIAL 1
#endif
static char s_usb_buf[200];
static int  s_usb_pos = 0;
static bool s_usb_serial_ok = false;

#if CONFIG_IDF_TARGET_ESP32
static void usb_serial_init(void) {
    // Console commands over UART0 (same pins as the flashing adapter).
    if (!uart_is_driver_installed(UART_NUM_0)) {
        if (uart_driver_install(UART_NUM_0, 512, 0, 0, nullptr, 0) != ESP_OK) {
            ESP_LOGW(TAG, "UART0 console init failed");
            return;
        }
    }
    s_usb_serial_ok = true;
}

static void serial_write(const char* data) {
    if (!s_usb_serial_ok || !data || data[0] == '\0') return;
    uart_write_bytes(UART_NUM_0, data, strlen(data));
}

static int serial_read_byte(uint8_t* b) {
    return uart_read_bytes(UART_NUM_0, b, 1, 0);
}
#else
#if XIAO_CONSOLE_USB_SERIAL
static void usb_serial_init(void) {
    if (!usb_serial_jtag_is_driver_installed()) {
        usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
        esp_err_t err = usb_serial_jtag_driver_install(&cfg);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "USB serial init failed: %s", esp_err_to_name(err));
            s_usb_serial_ok = false;
            return;
        }
    }
    s_usb_serial_ok = true;
}
#else
static void usb_serial_init(void) {}
#endif

static void serial_write(const char* data) {
    if (!s_usb_serial_ok || !data || data[0] == '\0') return;
    usb_serial_jtag_write_bytes(data, strlen(data), pdMS_TO_TICKS(20));
}

static int serial_read_byte(uint8_t* b) {
    return usb_serial_jtag_read_bytes(b, 1, 0);
}
#endif

static void poll_usb_serial(void) {
    if (!s_usb_serial_ok) return;
    uint8_t b;
    while (serial_read_byte(&b) == 1) {
        char c = (char)b;
        if (c == '\r') continue;
        if (c == '\n') {
            s_usb_buf[s_usb_pos] = '\0';
            char reply[512] = {};
            command_execute(s_usb_buf, reply, sizeof(reply));
            serial_write(reply);
            s_usb_pos = 0;
        } else if (s_usb_pos < (int)sizeof(s_usb_buf) - 1) {
            s_usb_buf[s_usb_pos++] = c;
        } else {
            const char* err = "ERR LINE_TOO_LONG\n";
            serial_write(err);
            s_usb_pos = 0;
        }
    }
}

// ── WiFi ──────────────────────────────────────────────────────────────────────
static bool s_wifi_ok = false;
static volatile bool s_wifi_got_ip = false;
static volatile bool s_wifi_disconnected = false;
static bool s_wifi_services_started = false;

static const struct { const char* ssid; const char* pass; } WIFI_APS[] = {
#if WIFI_AP_COUNT >= 1
    { WIFI_AP_1_SSID, WIFI_AP_1_PASS },
#endif
#if WIFI_AP_COUNT >= 2
    { WIFI_AP_2_SSID, WIFI_AP_2_PASS },
#endif
#if WIFI_AP_COUNT >= 3
    { WIFI_AP_3_SSID, WIFI_AP_3_PASS },
#endif
#if WIFI_AP_COUNT >= 4
    { WIFI_AP_4_SSID, WIFI_AP_4_PASS },
#endif
};
static const int WIFI_APS_COUNT = (int)(sizeof(WIFI_APS) / sizeof(WIFI_APS[0]));

static void wifi_event_handler(void*, esp_event_base_t base,
                               int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        auto* e = (wifi_event_sta_disconnected_t*)data;
        ESP_LOGW(TAG, "WiFi disconnect: \"%.32s\" reason=%d rssi=%d",
                 (const char*)e->ssid, (int)e->reason, (int)e->rssi);
        s_wifi_got_ip       = false;
        s_wifi_ok           = false;
        s_wifi_disconnected = true;
        // wifi_monitor_task handles reconnection — no esp_wifi_connect() here
        // (calling it here races with set_config+connect in the cycling loop)
    }
}
static void ip_event_handler(void*, esp_event_base_t base,
                             int32_t id, void* data) {
    if (id == IP_EVENT_STA_GOT_IP) s_wifi_got_ip = true;
}

// Blocking all-channel scan: shows whether the radio hears anything at all.
// A healthy antenna sees the home AP at -40..-70 dBm; an unplugged U.FL
// pigtail shows an empty list or everything below -85 dBm.
static void wifi_scan_report(void) {
    wifi_scan_config_t sc = {};
    sc.show_hidden = true;
    if (esp_wifi_scan_start(&sc, true) != ESP_OK) {
        ESP_LOGW(TAG, "WiFi scan failed to start");
        return;
    }
    uint16_t n = 12;
    wifi_ap_record_t recs[12];
    if (esp_wifi_scan_get_ap_records(&n, recs) != ESP_OK) {
        return;
    }
    ESP_LOGI(TAG, "WiFi scan: %u AP(s) visible", (unsigned)n);
    for (int i = 0; i < (int)n; i++) {
        ESP_LOGI(TAG, "  \"%.32s\" ch=%d rssi=%d", (const char*)recs[i].ssid,
                 (int)recs[i].primary, (int)recs[i].rssi);
    }
}

// Cycles through known APs until one connects. Blocks until success.
static void wifi_cycle_until_connected(void) {
    int ap_idx  = 0;
    int attempt = 0;
    bool first  = true;
    vTaskDelay(pdMS_TO_TICKS(200)); // let stack settle before first attempt

    while (true) {
        if (ap_idx == 0) {
            wifi_scan_report(); // once per full pass: antenna/RSSI diagnostics
        }

        const char* ssid = WIFI_APS[ap_idx].ssid;
        const char* pass = WIFI_APS[ap_idx].pass;

        wifi_config_t cfg = {};
        strncpy((char*)cfg.sta.ssid,     ssid, sizeof(cfg.sta.ssid)     - 1);
        strncpy((char*)cfg.sta.password, pass, sizeof(cfg.sta.password) - 1);
        // WPA2_PSK threshold: accepts WPA2 and stronger (WPA3).
        // OPEN → IDF auto-overrides to WPA2, blocking WPA3.
        // WPA2_WPA3_PSK → requires AP in transition mode (both simultaneously) — too strict.
        cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        cfg.sta.pmf_cfg.capable    = true;
        cfg.sta.pmf_cfg.required   = false;
        // Scan every channel and pick the strongest BSSID; marginal signal
        // needs every dB it can get.
        cfg.sta.scan_method        = WIFI_ALL_CHANNEL_SCAN;
        cfg.sta.sort_method        = WIFI_CONNECT_AP_BY_SIGNAL;
        // Let the driver retry auth/assoc a few times on its own before
        // reporting a disconnect.
        cfg.sta.failure_retry_cnt  = 3;

        s_wifi_got_ip = false;
        if (!first) {
            esp_wifi_disconnect();
            vTaskDelay(pdMS_TO_TICKS(300)); // let disconnect + internal state reset complete
        }
        first = false;
        esp_wifi_set_config(WIFI_IF_STA, &cfg);
        s_wifi_disconnected = false;
        esp_wifi_connect();

        ESP_LOGI(TAG, "WiFi [%d/%d] \"%s\"...", ap_idx + 1, WIFI_APS_COUNT, ssid);

        const int64_t deadline = esp_timer_get_time() + 20000LL * 1000; // 20s per AP
        while (!s_wifi_got_ip && esp_timer_get_time() < deadline) {
            vTaskDelay(pdMS_TO_TICKS(300));
            // One esp_wifi_connect() is a single shot: after NO_AP_FOUND or a
            // failed handshake the STA sits idle. Re-arm it so the whole 20 s
            // window keeps trying instead of wasting the remainder.
            if (s_wifi_disconnected && !s_wifi_got_ip) {
                s_wifi_disconnected = false;
                vTaskDelay(pdMS_TO_TICKS(500));
                esp_wifi_connect();
            }
        }

        if (s_wifi_got_ip) {
            s_wifi_ok = true;
            esp_netif_ip_info_t ip;
            esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), &ip);
            ESP_LOGI(TAG, "WiFi OK \"%s\" " IPSTR, ssid, IP2STR(&ip.ip));
            if (!s_wifi_services_started) {
                http_server_start();
                if (mdns_init() == ESP_OK) {
                    mdns_hostname_set(WIFI_HOSTNAME);
                    mdns_service_add(nullptr, "_http", "_tcp", 80, nullptr, 0);
                    ESP_LOGI(TAG, "mDNS: http://%s.local/", WIFI_HOSTNAME);
                }
                s_wifi_services_started = true;
            }
            return;
        }

        ESP_LOGW(TAG, "WiFi \"%s\": нет ответа", ssid);
        ap_idx = (ap_idx + 1) % WIFI_APS_COUNT;
        attempt++;
        if (ap_idx == 0) {
            ESP_LOGW(TAG, "WiFi: перебрал все сети (%d), повтор...", attempt);
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
    }
}

// Background task: reconnects whenever s_wifi_ok drops (AP drop, deauth, etc.)
static void wifi_monitor_task(void*) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (!s_wifi_ok) {
            ESP_LOGW(TAG, "WiFi: соединение потеряно, сканирую...");
            wifi_cycle_until_connected();
        }
    }
}

static bool wifi_start(void) {
    if (WIFI_APS_COUNT == 0 || WIFI_APS[0].ssid[0] == '\0') {
        ESP_LOGI(TAG, "WiFi: skipped (no SSIDs configured)");
        return false;
    }

    esp_netif_create_default_wifi_sta();
    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&init_cfg);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, nullptr);
    esp_event_handler_register(IP_EVENT,   IP_EVENT_STA_GOT_IP, ip_event_handler, nullptr);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_netif_set_hostname(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"), WIFI_HOSTNAME);
    esp_wifi_start();

    // Don't block boot on WiFi: the monitor task cycles until connected, so
    // the USB console and the peripherals stay usable with no AP (or no
    // antenna) around.
    xTaskCreate(wifi_monitor_task, "wifi_mon", 3072, nullptr, 3, nullptr);
    return true;
}

// ── Heartbeat ─────────────────────────────────────────────────────────────────
static void print_heartbeat(void) {
    uint8_t ch = 0; wifi_second_chan_t sec = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&ch, &sec);
    ESP_LOGI(TAG, "ALIVE cam=%d vl53=%d c1001=%d servos=%d led=%d"
             " yaw=%d pitch=%d espnow=%d ch=%u ps=%d",
             camera_is_ok()   ? 1 : 0,
             vl53_is_ok()     ? 1 : 0,
             c1001_is_ok()    ? 1 : 0,
             servo_is_ok()    ? 1 : 0,
             led_ring_is_ok() ? 1 : 0,
             servo_get_yaw(), servo_get_pitch(),
             espnow_is_ok()   ? 1 : 0,
             (unsigned)ch,
             command_power_save_active() ? 1 : 0);
}

// ── app_main ──────────────────────────────────────────────────────────────────
extern "C" void app_main(void) {
    // NVS (required by WiFi)
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    esp_event_loop_create_default();
    esp_netif_init();

    // USB-Serial/JTAG commands. Skip when C1001 uses D6/D7 (GPIO43/44).
    usb_serial_init();
#if XIAO_CONSOLE_USB_SERIAL
    serial_write("\nBOOT XIAO_SENSOR_BRIDGE\n");
#else
    ESP_LOGW(TAG, "USB console off — C1001 on D6/D7; use ESP-NOW / WiFi / mDNS");
#endif
    ESP_LOGI(TAG, "Build: VL53=%d C1001=%d SERVOS=%d VISION=%d",
             ENABLE_VL53, ENABLE_C1001, ENABLE_SERVOS, ENABLE_VISION_UPLOAD);

    // WiFi (must be first; ESP-NOW needs STA mode). Connection happens in the
    // background: s_wifi_ok flips when the monitor task gets an IP.
    wifi_start();

#if ENABLE_LED_RING
    led_ring_init();   // D10 = GPIO9, до долгого init C1001
#endif

    // ESP-NOW
#if ENABLE_ESPNOW
    if (espnow_init()) espnow_start_task();
#endif

    // Camera
#if ENABLE_CAMERA
    ESP_LOGI(TAG, "Camera init...");
    camera_init();
#else
    ESP_LOGI(TAG, "Camera disabled (ENABLE_CAMERA=0)");
#endif

    // VL53L0X
#if ENABLE_VL53
    ESP_LOGI(TAG, "VL53L0X SDA=GPIO%d SCL=GPIO%d", I2C_SDA_GPIO, I2C_SCL_GPIO);
    vl53_init();
#endif

    // C1001 radar (needs GPIO43/44 free — USJ disabled in sdkconfig)
#if ENABLE_C1001
    ESP_LOGI(TAG, "C1001 TX=GPIO%d RX=GPIO%d baud=%d", C1001_TX_GPIO, C1001_RX_GPIO, C1001_UART_BAUD);
    c1001_init();
#endif

    // Servos
#if ENABLE_SERVOS
    ESP_LOGI(TAG, "Servos yaw=GPIO%d pitch=GPIO%d", SERVO_YAW_GPIO, SERVO_PITCH_GPIO);
    servo_init();
#endif

    // HTTP server (only if WiFi is up)
    if (s_wifi_ok) http_server_start();

    // Periodic vision upload task
#if ENABLE_VISION_UPLOAD
    if (s_wifi_ok) vision_upload_start_task();
#endif

    ESP_LOGI(TAG, "Ready. Type HELP.");

    // ── Main loop ─────────────────────────────────────────────────────────────
    int64_t last_heartbeat_us = 0;

    while (true) {
        poll_usb_serial();

#if ENABLE_LED_RING
        led_ring_service();
#endif

#if ENABLE_SERVOS
        servo_service();
        if (command_power_save_active() && servo_is_ok() &&
            servo_is_attached() && !servo_is_moving()) {
            servo_detach();
        }
#if SERVO_RELAX_MS > 0
        if (servo_is_ok() && servo_is_attached() && !servo_is_moving()) {
            // servo.cc handles detach after SERVO_RELAX_MS internally
        }
#endif
#endif

#if ENABLE_VL53
        if (g_dist_stream_enabled && vl53_is_ok()) {
            int64_t now = esp_timer_get_time();
            if ((now - g_last_dist_stream_us) / 1000 >= (int64_t)g_dist_stream_ms) {
                g_last_dist_stream_us = now;
                uint16_t mm = 0; uint8_t st = 0;
                if (vl53_read(&mm, &st)) {
                    if (st != 4) {
                        char buf[32];
                        snprintf(buf, sizeof(buf), "DIST %u\n", (unsigned)mm);
                        serial_write(buf);
                    }
                }
            }
        }
#endif

#if ENABLE_C1001
        if (g_radar_stream_enabled && c1001_is_ok()) {
            int64_t now = esp_timer_get_time();
            if ((now - g_last_radar_stream_us) / 1000 >= (int64_t)g_radar_stream_ms) {
                g_last_radar_stream_us = now;
                uint8_t pres = 0, mot = 0; uint16_t rng = 0;
                if (c1001_query(&pres, &mot, &rng)) {
                    char buf[64];
                    snprintf(buf, sizeof(buf), "RADAR presence=%u motion=%u range=%u\n",
                             (unsigned)pres, (unsigned)mot, (unsigned)rng);
                    serial_write(buf);
                }
            }
        }
#endif

        // Heartbeat
        int64_t now = esp_timer_get_time();
        if ((now - last_heartbeat_us) / 1000 >= HEARTBEAT_INTERVAL_MS) {
            last_heartbeat_us = now;
            print_heartbeat();
        }

        command_check_idle_timeout();
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
