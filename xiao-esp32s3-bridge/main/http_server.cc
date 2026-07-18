#include "http_server.h"
#include "camera.h"
#include "command.h"
#include "config.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* TAG = "HTTP";
static httpd_handle_t s_server = nullptr;

// httpd_query_key_value() returns the raw value: "%20" and "+" arrive
// verbatim, so "SERVO 60 60" would never match a command. Decode in place.
static void url_decode(char* s) {
    char* out = s;
    for (char* p = s; *p; ++p) {
        if (*p == '%' && isxdigit((unsigned char)p[1]) && isxdigit((unsigned char)p[2])) {
            const char hex[3] = {p[1], p[2], '\0'};
            *out++ = (char)strtol(hex, nullptr, 16);
            p += 2;
        } else if (*p == '+') {
            *out++ = ' ';
        } else {
            *out++ = *p;
        }
    }
    *out = '\0';
}

// ── /capture ──────────────────────────────────────────────────────────────────
static esp_err_t handler_capture(httpd_req_t* req) {
    camera_fb_t* fb = camera_fb_get_locked(4000);
    if (!fb) {
        httpd_resp_set_status(req, "503 Service Unavailable");
        return httpd_resp_send(req, "camera busy", -1);
    }
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t err = httpd_resp_send(req, (const char*)fb->buf, fb->len);
    camera_fb_release(fb);
    return err;
}

// ── /stream (MJPEG) ───────────────────────────────────────────────────────────
static esp_err_t handler_stream(httpd_req_t* req) {
    httpd_resp_set_type(req, "multipart/x-mixed-replace; boundary=frame");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

    char part_buf[128];
    while (true) {
        camera_fb_t* fb = camera_fb_get_locked(2000);
        if (!fb) { vTaskDelay(pdMS_TO_TICKS(5)); continue; }

        int hdr_len = snprintf(part_buf, sizeof(part_buf),
            "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
            (unsigned)fb->len);

        esp_err_t err = httpd_resp_send_chunk(req, part_buf, hdr_len);
        if (err == ESP_OK) err = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
        if (err == ESP_OK) err = httpd_resp_send_chunk(req, "\r\n", 2);
        camera_fb_release(fb);

        if (err != ESP_OK) break; // client disconnected
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    httpd_resp_send_chunk(req, nullptr, 0);
    return ESP_OK;
}

// ── /status ───────────────────────────────────────────────────────────────────
static esp_err_t handler_status(httpd_req_t* req) {
    char buf[256];
    command_execute("STATUS?", buf, sizeof(buf));
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, buf, strlen(buf));
}

// ── /cmd?q=<COMMAND> ──────────────────────────────────────────────────────────
static esp_err_t handler_cmd(httpd_req_t* req) {
    char q[200] = {};
    if (httpd_req_get_url_query_str(req, q, sizeof(q)) == ESP_OK) {
        char val[200] = {};
        if (httpd_query_key_value(q, "q", val, sizeof(val)) == ESP_OK) {
            url_decode(val);
            char reply[512] = {};
            command_execute(val, reply, sizeof(reply));
            httpd_resp_set_type(req, "text/plain");
            httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
            return httpd_resp_send(req, reply, strlen(reply));
        }
    }
    httpd_resp_set_status(req, "400 Bad Request");
    return httpd_resp_send(req, "missing ?q=", -1);
}

// ── / (web UI) ────────────────────────────────────────────────────────────────
static const char* HTML =
    "<!DOCTYPE html><html><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>XIAO S3</title>"
    "<style>body{font-family:system-ui,sans-serif;margin:16px;"
    "background:#1a1a1a;color:#eee}"
    "button{font:inherit;padding:10px 14px;margin:0 8px 12px 0;"
    "border:0;border-radius:4px;cursor:pointer;background:#333;color:#eee;"
    "border:1px solid #555}"
    "img{max-width:100%;border-radius:4px;border:1px solid #444;background:#111}"
    "#st{margin:10px 0;color:#b8d7ff;font-size:.9em}</style></head><body>"
    "<h1>XIAO ESP32-S3</h1>"
    "<p><a href='/stream'>MJPEG stream</a> &middot; <a href='/capture'>JPEG</a></p>"
    "<div id=st>–</div><img id=cam><br>"
    "<script>"
    "const img=document.getElementById('cam'),st=document.getElementById('st');"
    "async function poll(){try{let r=await fetch('/status');st.textContent=await r.text();}catch(e){}}"
    "setInterval(()=>{img.src='/capture?t='+Date.now();},700);"
    "setInterval(poll,1500);poll();"
    "</script></body></html>";

static esp_err_t handler_root(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, HTML, strlen(HTML));
}

bool http_server_start(void) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_uri_handlers = 8;
    cfg.stack_size       = 8192;

    if (httpd_start(&s_server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "start failed");
        return false;
    }

    const httpd_uri_t uris[] = {
        { "/",        HTTP_GET, handler_root,    nullptr },
        { "/capture", HTTP_GET, handler_capture, nullptr },
        { "/stream",  HTTP_GET, handler_stream,  nullptr },
        { "/status",  HTTP_GET, handler_status,  nullptr },
        { "/cmd",     HTTP_GET, handler_cmd,     nullptr },
    };
    for (auto& u : uris) httpd_register_uri_handler(s_server, &u);

    ESP_LOGI(TAG, "started on port %d", cfg.server_port);
    return true;
}

void http_server_stop(void) {
    if (s_server) { httpd_stop(s_server); s_server = nullptr; }
}
