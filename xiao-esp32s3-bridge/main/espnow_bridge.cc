#include "espnow_bridge.h"
#include "command.h"
#include "config.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <string.h>

static const char* TAG = "ESPNOW";

static QueueHandle_t s_queue = nullptr;
static bool s_ok = false;

static void rx_callback(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    if (!s_queue || !info) return;
    const size_t min_len = sizeof(espnow_packet_t) - ESPNOW_PAYLOAD_SIZE + 1;
    if (len < (int)min_len) return;

    const auto* pkt = (const espnow_packet_t*)data;
    if (pkt->magic   != ESPNOW_MAGIC   ||
        pkt->version != ESPNOW_VERSION ||
        pkt->type    != ESPNOW_COMMAND) return;

    const size_t avail = (size_t)len - (sizeof(espnow_packet_t) - ESPNOW_PAYLOAD_SIZE);
    espnow_queued_cmd_t q = {};
    memcpy(q.sender, info->src_addr, 6);
    q.sequence = pkt->sequence;
    size_t cmd_len = strnlen(pkt->payload, avail);
    if (cmd_len >= ESPNOW_PAYLOAD_SIZE) cmd_len = ESPNOW_PAYLOAD_SIZE - 1;
    memcpy(q.command, pkt->payload, cmd_len);
    q.command[cmd_len] = '\0';

    xQueueSendFromISR(s_queue, &q, nullptr);
}

bool espnow_init(void) {
    s_queue = xQueueCreate(ESPNOW_QUEUE_LEN, sizeof(espnow_queued_cmd_t));
    if (!s_queue) { ESP_LOGE(TAG, "queue alloc failed"); return false; }

    // WiFi must already be in STA mode; if not connected, fix the channel.
    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_NULL) esp_wifi_set_mode(WIFI_MODE_STA);

    uint8_t primary = 0;
    wifi_second_chan_t secondary = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&primary, &secondary);
    if (primary == 0) {
        esp_wifi_set_channel(ESPNOW_FALLBACK_CHANNEL, WIFI_SECOND_CHAN_NONE);
        primary = ESPNOW_FALLBACK_CHANNEL;
        ESP_LOGI(TAG, "WiFi offline, forced channel=%u", primary);
    }

    esp_err_t err = esp_now_init();
    if (err != ESP_OK) { ESP_LOGE(TAG, "init: %s", esp_err_to_name(err)); return false; }
    err = esp_now_register_recv_cb(rx_callback);
    if (err != ESP_OK) { esp_now_deinit(); ESP_LOGE(TAG, "recv_cb: %s", esp_err_to_name(err)); return false; }

    s_ok = true;
    ESP_LOGI(TAG, "ready channel=%u", primary);
    return true;
}

bool espnow_is_ok(void) { return s_ok; }

void espnow_service(void) {
    if (!s_ok || !s_queue) return;
    espnow_queued_cmd_t q = {};
    while (xQueueReceive(s_queue, &q, 0) == pdTRUE) {
        // Execute command, capture reply into a string buffer.
        char reply_buf[ESPNOW_PAYLOAD_SIZE] = {};
        size_t reply_len = 0;

        // FRAME / STREAM commands are not supported over ESP-NOW (no bandwidth).
        if (strcmp(q.command, "FRAME") == 0 ||
            strncmp(q.command, "STREAM ", 7) == 0) {
            snprintf(reply_buf, sizeof(reply_buf), "ERR CAMERA_USE_WIFI_OR_UART");
        } else {
            command_execute(q.command, reply_buf, sizeof(reply_buf));
            reply_len = strnlen(reply_buf, sizeof(reply_buf) - 1);
            // Trim trailing newline
            while (reply_len > 0 && (reply_buf[reply_len-1] == '\n' || reply_buf[reply_len-1] == '\r'))
                reply_buf[--reply_len] = '\0';
        }

        // Register peer if first time seen.
        if (!esp_now_is_peer_exist(q.sender)) {
            esp_now_peer_info_t peer = {};
            memcpy(peer.peer_addr, q.sender, 6);
            peer.channel = 0;
            peer.ifidx   = WIFI_IF_STA;
            peer.encrypt = false;
            esp_now_add_peer(&peer);
        }

        espnow_packet_t resp = {};
        resp.magic    = ESPNOW_MAGIC;
        resp.version  = ESPNOW_VERSION;
        resp.type     = ESPNOW_RESPONSE;
        resp.sequence = q.sequence;
        strncpy(resp.payload, reply_buf, ESPNOW_PAYLOAD_SIZE - 1);

        const size_t send_len = sizeof(espnow_packet_t) - ESPNOW_PAYLOAD_SIZE +
                                strlen(resp.payload) + 1;
        esp_now_send(q.sender, (const uint8_t*)&resp, send_len);
        ESP_LOGI(TAG, "cmd=%s reply=%s", q.command, resp.payload);
    }
}

static void espnow_task(void*) {
    while (true) {
        espnow_service();
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Called after espnow_init() to offload processing to a dedicated task.
bool espnow_start_task(void) {
    return xTaskCreate(espnow_task, "espnow", 4096, nullptr, 2, nullptr) == pdPASS;
}
