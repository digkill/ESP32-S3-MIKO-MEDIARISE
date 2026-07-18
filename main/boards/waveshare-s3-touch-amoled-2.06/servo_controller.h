#ifndef SERVO_CONTROLLER_H
#define SERVO_CONTROLLER_H

#include <driver/uart.h>
#include <esp_now.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

class ServoController {
public:
    ServoController();
    ~ServoController();

    bool Init();
    bool InitEspNow();

    bool SetServoAngle(int servo_num, int angle);
    bool SetMultipleServos(const std::vector<std::pair<int, int>>& commands);
    bool SetPose(const std::string& pose_name);

    bool SetLed(int r, int g, int b);
    bool SetDefaultLed();
    bool LedTest();
    bool LedOff();
    bool SetPowerSaveMode(bool sleeping);

    bool RequestDistance(std::string& response);
    bool RequestRadar(std::string& response);
    bool RequestStatus(std::string& response);
    bool RequestFrame(std::string& jpeg_data, uint32_t& frame_id, std::string& error);
    bool CameraStreamOff();

private:
    uart_port_t uart_port_;
    bool initialized_;
    bool espnow_initialized_ = false;
    bool espnow_peer_online_ = false;
    bool espnow_peer_mac_known_ = false;
    uint8_t espnow_peer_mac_[ESP_NOW_ETH_ALEN] = {};
    int espnow_failures_ = 0;
    int64_t last_espnow_probe_us_ = 0;
    SemaphoreHandle_t espnow_response_sem_ = nullptr;
    uint16_t espnow_sequence_ = 0;
    uint16_t espnow_waiting_sequence_ = 0;
    std::string espnow_response_;
    std::mutex uart_mutex_;

    bool local_servos_ready_ = false;
    bool local_servos_attached_ = false;
    int local_yaw_angle_ = -1;
    int local_pitch_angle_ = -1;
    std::mutex local_servo_mutex_;
    esp_timer_handle_t local_servo_release_timer_ = nullptr;

    static ServoController* espnow_instance_;
    bool InitLocalServos();
    bool SetLocalServoAngle(int servo_num, int angle);
    bool AttachLocalServosLocked();
    void ReleaseLocalServos(bool force);
    static void LocalServoReleaseCallback(void* arg);
    static void EspNowReceiveCallback(const esp_now_recv_info_t* info, const uint8_t* data, int len);
    void HandleEspNowPacket(const esp_now_recv_info_t* info, const uint8_t* data, int len);
    bool EnsureEspNowPeerLocked(const uint8_t* mac);
    bool RefreshEspNowPeerLocked();
    bool SendEspNowCommandLocked(const std::string& command, std::string* response, int timeout_ms);
    bool SendCommandLocked(const std::string& command);
    bool SendCommand(const std::string& command);
    bool SendCommandAndReadLine(const std::string& command, std::string& response, int timeout_ms = 500);
    bool ReadLineLocked(std::string& response, int timeout_ms);
    bool ReadBytesLocked(uint8_t* data, size_t len, int timeout_ms);
    int ClampAngle(int angle) const;
};

#endif // SERVO_CONTROLLER_H
