#pragma once

#include <functional>

#include <esp_timer.h>
#include <esp_pm.h>

class PowerSaveTimer {
public:
    PowerSaveTimer(int cpu_max_freq, int seconds_to_sleep = 20, int seconds_to_shutdown = -1, int seconds_to_dim = -1);
    ~PowerSaveTimer();

    void SetEnabled(bool enabled);
    void OnEnterSleepMode(std::function<void()> callback);
    void OnExitSleepMode(std::function<void()> callback);
    void OnShutdownRequest(std::function<void()> callback);
    void OnEnterDimMode(std::function<void()> callback);
    void OnExitDimMode(std::function<void()> callback);
    void WakeUp();
    bool IsSleeping() const { return in_sleep_mode_; }
    bool IsDimmed() const { return in_dim_mode_; }

private:
    void PowerSaveCheck();

    esp_timer_handle_t power_save_timer_ = nullptr;
    bool enabled_ = false;
    bool in_sleep_mode_ = false;
    bool in_dim_mode_ = false;
    bool is_wake_word_running_ = false;
    int ticks_ = 0;
    int cpu_max_freq_;
    int seconds_to_sleep_;
    int seconds_to_shutdown_;
    int seconds_to_dim_;

    std::function<void()> on_enter_sleep_mode_;
    std::function<void()> on_exit_sleep_mode_;
    std::function<void()> on_shutdown_request_;
    std::function<void()> on_enter_dim_mode_;
    std::function<void()> on_exit_dim_mode_;
};
