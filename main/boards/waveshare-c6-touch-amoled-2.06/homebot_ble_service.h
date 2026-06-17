#ifndef HOMEBOT_BLE_SERVICE_H
#define HOMEBOT_BLE_SERVICE_H

#include <cstdint>
#include <string>

struct ble_gap_event;
struct ble_gatt_access_ctxt;
class ServoController;

class HomeBotBleService {
public:
    HomeBotBleService();

    void Start();
    void SetXiaoController(ServoController* controller);
    void SendEvent(const std::string& type, const std::string& message, const std::string& status = "ok");
    static int CommandAccess(
        uint16_t connection_handle,
        uint16_t attribute_handle,
        struct ble_gatt_access_ctxt* context,
        void* arg);
    static int EventAccess(
        uint16_t connection_handle,
        uint16_t attribute_handle,
        struct ble_gatt_access_ctxt* context,
        void* arg);

private:
    static HomeBotBleService* instance_;

    uint16_t connection_handle_ = 0xffff;
    uint8_t address_type_ = 0;
    ServoController* xiao_controller_ = nullptr;

    static int GapEvent(struct ble_gap_event* event, void* arg);
    static void OnHostSync();
    static void HostTask(void* parameter);

    void Advertise();
    void SendDialogReply(const std::string& reply);
    void HandleCommand(const std::string& json);
};

#endif  // HOMEBOT_BLE_SERVICE_H
