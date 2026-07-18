#pragma once
// Local WiFi credentials — DO NOT COMMIT (add to .gitignore)
// Networks are tried in order, cycling until one connects.

// Hardware fitted on this XIAO bridge. Enable these only when the sensors are wired.
#define ENABLE_CAMERA 0   // no camera module on this XIAO
#define ENABLE_VL53   0   // laser rangefinder not wired yet
#define ENABLE_C1001  1   // radar on the XIAO TX/RX pins (D6/D7)

// WS2812 DIN → пин D10 на плате XIAO = GPIO9 (НЕ GPIO10!)
#define LED_RING_GPIO       9

#define WIFI_AP_1_SSID  "muza 2.4"
#define WIFI_AP_1_PASS  "s1h4sr39@nlwg43wd"

#define WIFI_AP_2_SSID  "Hotspot"
#define WIFI_AP_2_PASS  "s1h4sr39@nlwg43wd"

#define WIFI_AP_3_SSID  "poem"
#define WIFI_AP_3_PASS  "ED04AA56"

#define WIFI_AP_COUNT   3
