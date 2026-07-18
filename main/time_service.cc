#include "time_service.h"
#include "config.h"
#include "settings.h"

#include <cstring>
#include <ctime>
#include <cstdlib>
#include <sys/time.h>

#include <esp_log.h>
#include <esp_netif_sntp.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#ifndef DEFAULT_TIMEZONE
#define DEFAULT_TIMEZONE "UTC0"
#endif

#ifndef DEFAULT_SNTP_SERVER
#define DEFAULT_SNTP_SERVER "pool.ntp.org"
#endif

static const char* TAG = "TimeService";

TimeService& TimeService::GetInstance() {
    static TimeService instance;
    return instance;
}

static const char* NormalizeTimezone(const char* timezone) {
    if (!timezone || timezone[0] == '\0') {
        return DEFAULT_TIMEZONE;
    }
    if (strcmp(timezone, "Asia/Yekaterinburg") == 0 ||
        strcmp(timezone, "Asia/Ekaterinburg") == 0 ||
        strcmp(timezone, "Europe/Yekaterinburg") == 0) {
        return "YEKT-5";
    }
    return timezone;
}

void TimeService::ApplyTimezone() {
    Settings settings("homebot", true);
    std::string configured = settings.GetString("timezone");
    const char* timezone = NormalizeTimezone(configured.empty() ? DEFAULT_TIMEZONE : configured.c_str());
    setenv("TZ", timezone, 1);
    tzset();
    ESP_LOGI(TAG, "timezone=%s", timezone);
}

bool TimeService::IsTimeValid() const {
    time_t now = time(nullptr);
    return now > 1700000000;
}

void TimeService::OnSntpSync(struct timeval* tv) {
    time_t now = tv ? tv->tv_sec : time(nullptr);
    struct tm local = {};
    char buffer[32] = {};
    if (localtime_r(&now, &local)) {
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local);
    }
    ESP_LOGI(TAG, "SNTP synchronized: %s", buffer[0] ? buffer : "time set");
}

void TimeService::SyncTask(void* arg) {
    auto* self = static_cast<TimeService*>(arg);

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(DEFAULT_SNTP_SERVER);
    config.sync_cb = &TimeService::OnSntpSync;
    esp_err_t err = esp_netif_sntp_init(&config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SNTP init failed: %s", esp_err_to_name(err));
        self->started_ = false;
        vTaskDelete(nullptr);
        return;
    }

    for (int attempt = 1; attempt <= 6 && !self->IsTimeValid(); ++attempt) {
        err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000));
        if (err == ESP_OK) {
            break;
        }
        ESP_LOGW(TAG, "SNTP sync wait failed: %s attempt=%d", esp_err_to_name(err), attempt);
    }

    if (!self->IsTimeValid()) {
        ESP_LOGW(TAG, "time is still not synchronized; SNTP will keep retrying in background");
    }

    vTaskDelete(nullptr);
}

void TimeService::Start() {
    ApplyTimezone();
    if (started_) {
        return;
    }
    started_ = true;

    if (xTaskCreate(SyncTask, "time_sync", 4096, this, 4, nullptr) != pdPASS) {
        ESP_LOGE(TAG, "failed to start time sync task");
        started_ = false;
    }
}
