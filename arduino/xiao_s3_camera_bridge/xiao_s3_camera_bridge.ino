/*
 * Seeed Studio XIAO ESP32-S3 Sense — sensor / actuator bridge.
 *
 * Peripherals:
 *   - Camera          OV3660/OV2640 Sense module (DVP)
 *   - LED ring        8× WS2812B NeoPixel on D10 (GPIO9)
 *   - Servo yaw       PWM on D1 (GPIO2)
 *   - Servo pitch     PWM on D2 (GPIO3)
 *   - VL53L0X V2      I2C on D4/D5 (SDA=GPIO5 SCL=GPIO6)
 *   - DFRobot C1001   60 GHz radar, UART1 on D6/D7 (TX=GPIO43 RX=GPIO44)
 *   - Main MCU link    ESP-NOW over the shared 2.4 GHz Wi-Fi channel
 *
 * NOTE: SD SPI uses D8/D9/D10 — the same pins as LED ring and servos.
 *       ENABLE_SD_RECORDING=0 frees those pins. Re-enable only if servos/LED ring
 *       are not connected.
 *
 * Required Arduino libraries (Library Manager):
 *   Adafruit NeoPixel, Adafruit VL53L0X, DFRobot_HumanDetection
 *
 * Wiki: https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/
 */

#include <Arduino.h>
#include "esp_camera.h"

#ifndef ENABLE_VL53
#define ENABLE_VL53 1
#endif
#ifndef ENABLE_SERVOS
#define ENABLE_SERVOS 0           // head servos are driven by the AMOLED board now
#endif
#ifndef ENABLE_C1001
#define ENABLE_C1001 0            // radar disabled for now (was: UART1 on D6/D7)
#endif
#ifndef ENABLE_SD_RECORDING
#define ENABLE_SD_RECORDING 0     // SD shares D8/D9/D10 with LED ring + servos
#endif
#ifndef ENABLE_AUDIO_RECORDING
#define ENABLE_AUDIO_RECORDING 0  // audio writes to SD which is disabled
#endif
#ifndef ENABLE_ESPNOW
#define ENABLE_ESPNOW 1            // wireless command/reply link to Waveshare S3
#endif
#ifndef ENABLE_UART_LINK
#define ENABLE_UART_LINK 1         // D2/D3 are free again with ENABLE_SERVOS=0
#endif
#ifndef ESPNOW_FALLBACK_CHANNEL
#define ESPNOW_FALLBACK_CHANNEL 1  // Must match the Wi-Fi channel used by the AMOLED board.
#endif
#ifndef XIAO_POWER_SAVE_TIMEOUT_MS
#define XIAO_POWER_SAVE_TIMEOUT_MS (30UL * 60UL * 1000UL)
#endif
#ifndef ENABLE_VISION_UPLOAD
#define ENABLE_VISION_UPLOAD 1
#endif
#ifndef VISION_UPLOAD_INTERVAL_MS
#define VISION_UPLOAD_INTERVAL_MS 15000UL
#endif
#ifndef VISION_UPLOAD_URL
#define VISION_UPLOAD_URL "http://90.156.254.46:8080/api/robot/vision/frame"
#endif
#ifndef VISION_DEVICE_ID
#define VISION_DEVICE_ID "homebot-xiao-camera"
#endif

// WiFi
#ifndef WIFI_SSID
#define WIFI_SSID "muza 2.4"
#endif
#ifndef WIFI_PASSWORD
#define WIFI_PASSWORD "s1h4sr39@nlwg43wd"
#endif
#ifndef WIFI_HOSTNAME
#define WIFI_HOSTNAME "miko-cam"
#endif

#if ENABLE_VL53
#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#endif
#if ENABLE_C1001
#include <DFRobot_HumanDetection.h>        // Arduino Library Manager: "DFRobot_HumanDetection"
#endif
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <freertos/queue.h>
#if ENABLE_SD_RECORDING
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <driver/i2s.h>
#endif

// ── Pin aliases (fallback if board package defines them differently) ──────────
#ifndef D0
#define D0  1
#define D1  2
#define D2  3
#define D3  4
#define D4  5
#define D5  6
#define D6  43
#define D7  44
#define D8  7
#define D9  8
#define D10 9
#endif

// ── User wiring ───────────────────────────────────────────────────────────────
#define LED_RING_PIN        D10
#define LED_RING_COUNT      8
#define LED_DEFAULT_R       0
#define LED_DEFAULT_G       120
#define LED_DEFAULT_B       255

#define I2C_SDA_PIN         D4   // VL53L0X SDA
#define I2C_SCL_PIN         D5   // VL53L0X SCL

// Link to Waveshare AMOLED — moved to D2/D3 so D6/D7 (TX/RX labels) are free for C1001
#define LINK_UART_TX_PIN    D2   // XIAO TX → Waveshare RX (GPIO18)
#define LINK_UART_RX_PIN    D3   // XIAO RX ← Waveshare TX (GPIO17)
#define LINK_UART_BAUD      115200

// C1001 radar on the board-labeled TX/RX pins (D6=TX label, D7=RX label)
#define C1001_TX_PIN        D6   // XIAO TX → C1001 RX
#define C1001_RX_PIN        D7   // XIAO RX ← C1001 TX
#define C1001_UART_BAUD     115200

#define SERVO_YAW_PIN       D1
#define SERVO_PITCH_PIN     D2
#define SERVO_HOME_ANGLE    60
#define SERVO_MAX_DEVIATION 20
#define SERVO_MIN_ANGLE     (SERVO_HOME_ANGLE - SERVO_MAX_DEVIATION)
#define SERVO_MAX_ANGLE     (SERVO_HOME_ANGLE + SERVO_MAX_DEVIATION)
#define SERVO_CALIB_MIN_ANGLE 0
#define SERVO_CALIB_MAX_ANGLE 120
#define SERVO_MIN_US        500
#define SERVO_MAX_US        2500
#define SERVO_PWM_FREQ_HZ   50
#define SERVO_PWM_RES_BITS  14
#ifndef SERVO_DUTY_QUANTUM
#define SERVO_DUTY_QUANTUM  8
#endif
#ifndef SERVO_RELAX_MS
#define SERVO_RELAX_MS      0
#endif
#ifndef SERVO_MOVE_INTERVAL_MS
#define SERVO_MOVE_INTERVAL_MS 20
#endif
#ifndef SERVO_STEP_DEG
#define SERVO_STEP_DEG      1
#endif
#define SERVO_LEDC_CH_YAW   6
#define SERVO_LEDC_CH_PITCH 7

// ── SD (used only when ENABLE_SD_RECORDING=1) ─────────────────────────────────
#define SD_PIN_CS    21
#define SD_PIN_SCK   D8
#define SD_PIN_MISO  D9
#define SD_PIN_MOSI  D10
#define RECORD_DIR   "/rec"
#define RECORD_FPS   8
#define RECORD_FLUSH_FRAMES 16

// ── PDM mic ───────────────────────────────────────────────────────────────────
#define MIC_PDM_CLK_PIN  42
#define MIC_PDM_DAT_PIN  41
#define MIC_SAMPLE_RATE  16000
#define MIC_GAIN         4
#define MIC_DMA_SAMPLES  512

// ── Camera pin map (Seeed XIAO ESP32-S3 Sense) ────────────────────────────────
#define CAM_PIN_PWDN   -1
#define CAM_PIN_RESET  -1
#define CAM_PIN_XCLK   10
#define CAM_PIN_SIOD   40
#define CAM_PIN_SIOC   39
#define CAM_PIN_D7     48
#define CAM_PIN_D6     11
#define CAM_PIN_D5     12
#define CAM_PIN_D4     14
#define CAM_PIN_D3     16
#define CAM_PIN_D2     18
#define CAM_PIN_D1     17
#define CAM_PIN_D0     15
#define CAM_PIN_VSYNC  38
#define CAM_PIN_HREF   47
#define CAM_PIN_PCLK   13

#if ENABLE_VL53
#if (CAM_PIN_SIOD == I2C_SDA_PIN) || (CAM_PIN_SIOC == I2C_SCL_PIN) || \
    (CAM_PIN_SIOD == I2C_SCL_PIN) || (CAM_PIN_SIOC == I2C_SDA_PIN)
#error "Camera SCCB and VL53L0X Wire pins overlap — VL53 must use D4/D5, not 39/40."
#endif
#endif

// ── Objects ───────────────────────────────────────────────────────────────────
static HardwareSerial LinkSerial(2);   // UART2: D2(TX)/D3(RX) — Waveshare link
static Adafruit_NeoPixel ring(LED_RING_COUNT, LED_RING_PIN, NEO_GRB + NEO_KHZ800);

#if ENABLE_VL53
static Adafruit_VL53L0X vl53;
#endif

#if ENABLE_C1001
static HardwareSerial C1001Serial(1);  // UART1: D6(TX)/D7(RX) — labeled TX/RX on board
static DFRobot_HumanDetection radar(&C1001Serial);
#endif

static WebServer server(80);

// ── State ─────────────────────────────────────────────────────────────────────
static SemaphoreHandle_t s_cam_mutex = nullptr;
static bool camera_ok   = false;
static bool led_ring_ok = false;
static bool sd_ok       = false;
static bool wifi_ok     = false;
static bool servos_ok   = false;

#if ENABLE_VL53
static bool vl53_ok = false;
#endif
#if ENABLE_C1001
static bool c1001_ok               = false;
static bool radar_stream_enabled   = false;
static uint32_t radar_stream_ms    = 500;
static uint32_t last_radar_ms      = 0;
#endif

static String usb_line;
static String link_line;
static bool stream_enabled        = false;
static uint32_t stream_interval_ms = 500;
static uint32_t last_stream_ms    = 0;
static uint32_t last_heartbeat_ms = 0;
static uint32_t frame_id          = 0;
static uint32_t last_vision_upload_ms = 0;
static TaskHandle_t vision_upload_task = nullptr;
static int yaw_angle   = SERVO_HOME_ANGLE;
static int pitch_angle = SERVO_HOME_ANGLE;
static bool xiao_power_save_enabled = false;
static uint32_t last_activity_ms = 0;

// ── ESP-NOW command/reply protocol ────────────────────────────────────────────
#if ENABLE_ESPNOW
static constexpr uint32_t ESPNOW_MAGIC = 0x574e4248;  // HBNW in little endian.
static constexpr uint8_t ESPNOW_VERSION = 1;
static constexpr uint8_t ESPNOW_COMMAND = 1;
static constexpr uint8_t ESPNOW_RESPONSE = 2;
static constexpr size_t ESPNOW_PAYLOAD_SIZE = 192;

struct __attribute__((packed)) EspNowPacket {
  uint32_t magic;
  uint8_t version;
  uint8_t type;
  uint16_t sequence;
  char payload[ESPNOW_PAYLOAD_SIZE];
};

struct EspNowQueuedCommand {
  uint8_t sender[ESP_NOW_ETH_ALEN];
  uint16_t sequence;
  char command[ESPNOW_PAYLOAD_SIZE];
};

static QueueHandle_t espnow_command_queue = nullptr;
static bool espnow_ok = false;
#endif

#if ENABLE_SERVOS
static int s_servo_current_yaw    = SERVO_HOME_ANGLE;
static int s_servo_current_pitch  = SERVO_HOME_ANGLE;
static int s_servo_last_yaw       = -1000;
static int s_servo_last_pitch     = -1000;
static uint32_t s_servo_last_duty_yaw   = 0xFFFFFFFFu;
static uint32_t s_servo_last_duty_pitch = 0xFFFFFFFFu;
static uint32_t s_servo_last_move_ms = 0;
static uint32_t s_servo_last_step_ms = 0;
static bool s_servos_attached = false;
static bool s_servo_moving    = false;
#endif

#if ENABLE_VL53
static bool dist_stream_enabled      = false;
static uint32_t dist_stream_ms       = 500;
static uint32_t last_dist_stream_ms  = 0;
#endif

#if ENABLE_SD_RECORDING
static bool recording           = false;
static bool audio_ok            = false;
static bool audio_recording     = false;
static volatile bool audio_task_run = false;
static TaskHandle_t audio_task_handle = nullptr;
static File record_video_file;
static File record_audio_file;
static String record_video_path;
static String record_audio_path;
static uint32_t record_started_ms   = 0;
static uint32_t record_last_frame_ms = 0;
static uint32_t record_frames       = 0;
static uint32_t record_video_bytes  = 0;
static uint32_t record_audio_bytes  = 0;
static uint32_t record_seq          = 0;
#endif

// ── Helpers ───────────────────────────────────────────────────────────────────
static int clamp_angle(int a) {
  if (a < SERVO_MIN_ANGLE) return SERVO_MIN_ANGLE;
  if (a > SERVO_MAX_ANGLE) return SERVO_MAX_ANGLE;
  return a;
}

static void send_line(Stream& out, const char* text) {
  out.print(text);
  out.print('\n');
}
static void send_ok(Stream& out, const String& msg) {
  out.print("OK ");
  out.print(msg);
  out.print('\n');
}
static void send_err(Stream& out, const String& msg) {
  out.print("ERR ");
  out.print(msg);
  out.print('\n');
}

#if ENABLE_ESPNOW
class EspNowResponseStream : public Stream {
public:
  String value;

  size_t write(uint8_t byte) override {
    if (value.length() < ESPNOW_PAYLOAD_SIZE - 1) value += (char)byte;
    return 1;
  }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
};
#endif

static void set_ring(uint8_t r, uint8_t g, uint8_t b) {
  if (!led_ring_ok) return;
  for (int i = 0; i < LED_RING_COUNT; i++)
    ring.setPixelColor(i, ring.Color(r, g, b));
  ring.show();
}

static void led_test(Stream& out) {
  if (!led_ring_ok) { send_err(out, "LED_RING_DISABLED"); return; }
  out.printf("LEDTEST pin=%d count=%d\n", LED_RING_PIN, LED_RING_COUNT);
  const uint32_t colors[] = {
    ring.Color(255, 0, 0), ring.Color(0, 255, 0),
    ring.Color(0, 0, 255), ring.Color(255, 255, 255),
  };
  for (uint32_t color : colors) {
    for (int i = 0; i < LED_RING_COUNT; i++) {
      ring.clear();
      ring.setPixelColor(i, color);
      ring.show();
      delay(120);
    }
  }
  set_ring(0, 0, 0);
  send_ok(out, "LEDTEST");
}

// ── Camera mutex ──────────────────────────────────────────────────────────────
static bool cam_mutex_take(uint32_t ms) {
  if (!s_cam_mutex) return true;
  return xSemaphoreTake(s_cam_mutex, pdMS_TO_TICKS(ms)) == pdTRUE;
}
static void cam_mutex_give() {
  if (s_cam_mutex) xSemaphoreGive(s_cam_mutex);
}

// ── SD helpers ────────────────────────────────────────────────────────────────
static bool pin_uses_sd_spi(int pin) {
#if ENABLE_SD_RECORDING
  return pin == SD_PIN_SCK || pin == SD_PIN_MISO || pin == SD_PIN_MOSI || pin == SD_PIN_CS;
#else
  (void)pin; return false;
#endif
}

#if ENABLE_SD_RECORDING
static bool init_sd_card() {
  SPI.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);
  if (!SD.begin(SD_PIN_CS, SPI, 8000000)) {
    Serial.printf("SD: begin failed cs=%d sck=%d miso=%d mosi=%d\n",
                  SD_PIN_CS, SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI);
    SD.end(); SPI.end(); return false;
  }
  if (SD.cardType() == CARD_NONE) {
    Serial.println("SD: no card"); SD.end(); SPI.end(); return false;
  }
  if (!SD.exists(RECORD_DIR)) SD.mkdir(RECORD_DIR);
  Serial.printf("SD: OK %lluMB dir=%s\n",
                (unsigned long long)(SD.cardSize() / (1024ULL*1024ULL)), RECORD_DIR);
  return true;
}
#else
static bool init_sd_card() {
  Serial.println("SD recording: disabled (ENABLE_SD_RECORDING=0)");
  return false;
}
#endif

// ── Audio (SD-dependent) ─────────────────────────────────────────────────────
#if ENABLE_AUDIO_RECORDING && ENABLE_SD_RECORDING
#include <driver/i2s.h>

static void write_wav_header(File& f, uint32_t data_bytes) {
  const uint32_t chunk_size = data_bytes + 36;
  const uint32_t byte_rate = MIC_SAMPLE_RATE * 2;
  const uint16_t audio_format = 1, channels = 1, block_align = 2, bps = 16;
  f.seek(0);
  f.write((const uint8_t*)"RIFF", 4); f.write((const uint8_t*)&chunk_size, 4);
  f.write((const uint8_t*)"WAVEfmt ", 8);
  const uint32_t sub1 = 16; f.write((const uint8_t*)&sub1, 4);
  f.write((const uint8_t*)&audio_format, 2); f.write((const uint8_t*)&channels, 2);
  const uint32_t sr = MIC_SAMPLE_RATE; f.write((const uint8_t*)&sr, 4);
  f.write((const uint8_t*)&byte_rate, 4); f.write((const uint8_t*)&block_align, 2);
  f.write((const uint8_t*)&bps, 2);
  f.write((const uint8_t*)"data", 4); f.write((const uint8_t*)&data_bytes, 4);
}

static bool install_audio_driver() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
  cfg.sample_rate = MIC_SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 4; cfg.dma_buf_len = MIC_DMA_SAMPLES;
  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) return false;
  i2s_pin_config_t pins = {};
  pins.bck_io_num = I2S_PIN_NO_CHANGE; pins.ws_io_num = MIC_PDM_CLK_PIN;
  pins.data_out_num = I2S_PIN_NO_CHANGE; pins.data_in_num = MIC_PDM_DAT_PIN;
  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
    i2s_driver_uninstall(I2S_NUM_0); return false;
  }
  i2s_zero_dma_buffer(I2S_NUM_0);
  return true;
}

static void audio_record_task(void*) {
  const size_t buf_bytes = MIC_DMA_SAMPLES * sizeof(int16_t);
  int16_t* buf = (int16_t*)malloc(buf_bytes);
  if (!buf) { audio_task_run = false; audio_recording = false; audio_task_handle = nullptr; vTaskDelete(nullptr); return; }
  while (audio_task_run) {
    size_t nr = 0;
    if (i2s_read(I2S_NUM_0, buf, buf_bytes, &nr, pdMS_TO_TICKS(200)) == ESP_OK && nr > 0) {
      const int n = nr / sizeof(int16_t);
      for (int i = 0; i < n; i++)
        buf[i] = constrain((int32_t)buf[i] * MIC_GAIN, (int32_t)INT16_MIN, (int32_t)INT16_MAX);
      if (record_audio_file) { record_audio_file.write((uint8_t*)buf, nr); record_audio_bytes += nr; }
    }
  }
  free(buf); audio_recording = false; audio_task_handle = nullptr; vTaskDelete(nullptr);
}

static bool start_audio_recording(const String& path) {
  record_audio_file = SD.open(path, FILE_WRITE);
  if (!record_audio_file) return false;
  record_audio_bytes = 0; write_wav_header(record_audio_file, 0);
  if (!install_audio_driver()) { record_audio_file.close(); return false; }
  audio_task_run = true; audio_recording = true;
  if (xTaskCreatePinnedToCore(audio_record_task, "audio_rec", 4096, nullptr, 1, &audio_task_handle, 0) != pdPASS) {
    audio_task_run = false; audio_recording = false; i2s_driver_uninstall(I2S_NUM_0); record_audio_file.close(); return false;
  }
  return true;
}

static void stop_audio_recording() {
  if (!audio_recording && !record_audio_file) return;
  audio_task_run = false;
  const uint32_t t0 = millis();
  while (audio_task_handle && millis() - t0 < 1500) delay(20);
  i2s_driver_uninstall(I2S_NUM_0);
  if (record_audio_file) { write_wav_header(record_audio_file, record_audio_bytes); record_audio_file.close(); }
  audio_recording = false;
}
#endif // ENABLE_AUDIO_RECORDING && ENABLE_SD_RECORDING

// ── Camera ────────────────────────────────────────────────────────────────────
static bool init_camera_once(int sda, int scl) {
  camera_config_t cfg = {};
  cfg.ledc_channel = LEDC_CHANNEL_0; cfg.ledc_timer = LEDC_TIMER_0;
  cfg.pin_d0 = CAM_PIN_D0; cfg.pin_d1 = CAM_PIN_D1; cfg.pin_d2 = CAM_PIN_D2;
  cfg.pin_d3 = CAM_PIN_D3; cfg.pin_d4 = CAM_PIN_D4; cfg.pin_d5 = CAM_PIN_D5;
  cfg.pin_d6 = CAM_PIN_D6; cfg.pin_d7 = CAM_PIN_D7;
  cfg.pin_xclk = CAM_PIN_XCLK; cfg.pin_pclk = CAM_PIN_PCLK;
  cfg.pin_vsync = CAM_PIN_VSYNC; cfg.pin_href = CAM_PIN_HREF;
  cfg.pin_sccb_sda = sda; cfg.pin_sccb_scl = scl;
  cfg.pin_pwdn = CAM_PIN_PWDN; cfg.pin_reset = CAM_PIN_RESET;
  cfg.xclk_freq_hz = 20000000;
  cfg.frame_size = FRAMESIZE_UXGA;
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  cfg.fb_location = CAMERA_FB_IN_PSRAM;
  cfg.jpeg_quality = 12;
  cfg.fb_count = 1;
  if (psramFound()) {
    cfg.jpeg_quality = 10; cfg.fb_count = 2; cfg.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    cfg.frame_size = FRAMESIZE_SVGA; cfg.fb_location = CAMERA_FB_IN_DRAM;
  }
  if (esp_camera_init(&cfg) != ESP_OK) return false;
  sensor_t* s = esp_camera_sensor_get();
  if (!s) return false;
  if (s->id.PID == 0x3660) { s->set_vflip(s, 1); s->set_brightness(s, 1); s->set_saturation(s, -2); }
  if (cfg.pixel_format == PIXFORMAT_JPEG) { s->set_framesize(s, FRAMESIZE_QVGA); s->set_quality(s, 12); }
  return true;
}

static bool init_camera() {
  Serial.printf("PSRAM: %s\n", psramFound() ? "OK" : "NOT FOUND — enable OPI PSRAM");
  delay(150);
  if (init_camera_once(CAM_PIN_SIOD, CAM_PIN_SIOC)) return true;
  esp_camera_deinit(); delay(80);
  if (init_camera_once(CAM_PIN_SIOC, CAM_PIN_SIOD)) return true;
  esp_camera_deinit();
  Serial.println("Camera: check Sense board seated; Tools board = XIAO ESP32S3");
  return false;
}

static void send_frame(Stream& out) {
  if (!camera_ok)                   { send_err(out, "CAMERA_NOT_READY"); return; }
  if (!cam_mutex_take(3000))        { send_err(out, "CAMERA_BUSY"); return; }
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb)                          { cam_mutex_give(); send_err(out, "FRAME_CAPTURE_FAILED"); return; }
  const uint32_t id = ++frame_id;
  out.printf("@FRAME %lu %u\n", (unsigned long)id, (unsigned)fb->len);
  out.write(fb->buf, fb->len);
  out.printf("\n@END %lu\n", (unsigned long)id);
  out.flush();
  esp_camera_fb_return(fb); cam_mutex_give();
}

// ── SD recording (enabled block) ─────────────────────────────────────────────
#if ENABLE_SD_RECORDING
static void service_recording();
static bool start_recording(Stream* out = nullptr);
static void stop_recording(Stream* out = nullptr);
static String record_status_json();

static void make_record_paths() {
  const uint32_t stamp = millis() / 1000;
  for (int i = 0; i < 100; i++) {
    record_seq++;
    char base[48];
    snprintf(base, sizeof(base), "%s/rec_%lu_%lu", RECORD_DIR, (unsigned long)stamp, (unsigned long)record_seq);
    record_video_path = String(base) + ".mjpg";
    record_audio_path = String(base) + ".wav";
    if (!SD.exists(record_video_path) && !SD.exists(record_audio_path)) return;
  }
}

static bool start_recording(Stream* out) {
  if (recording) { if (out) send_err(*out, "RECORD_ALREADY_ON"); return false; }
  if (!camera_ok){ if (out) send_err(*out, "CAMERA_NOT_READY"); return false; }
  if (!sd_ok)    { if (out) send_err(*out, "SD_NOT_READY"); return false; }
  make_record_paths();
  record_video_file = SD.open(record_video_path, FILE_WRITE);
  if (!record_video_file) { if (out) send_err(*out, "RECORD_OPEN_FAILED"); return false; }
  record_video_file.print("Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n");
  record_frames = 0; record_video_bytes = 0;
  record_started_ms = millis(); record_last_frame_ms = 0; recording = true;
#if ENABLE_AUDIO_RECORDING
  audio_ok = start_audio_recording(record_audio_path);
#endif
  if (out) out->printf("OK RECORD_ON video=%s\n", record_video_path.c_str());
  return true;
}

static void stop_recording(Stream* out) {
  if (!recording) { if (out) send_err(*out, "RECORD_NOT_ON"); return; }
  recording = false;
#if ENABLE_AUDIO_RECORDING
  stop_audio_recording();
#endif
  if (record_video_file) { record_video_file.flush(); record_video_file.close(); }
  if (out) out->printf("OK RECORD_OFF frames=%lu video=%s\n",
                       (unsigned long)record_frames, record_video_path.c_str());
}

static void service_recording() {
  if (!recording || !record_video_file || !camera_ok) return;
  const uint32_t interval_ms = 1000UL / max(1, RECORD_FPS);
  const uint32_t now = millis();
  if (record_last_frame_ms && now - record_last_frame_ms < interval_ms) return;
  if (!cam_mutex_take(10)) return;
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { cam_mutex_give(); return; }
  record_video_file.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", (unsigned)fb->len);
  record_video_file.write(fb->buf, fb->len);
  record_video_file.print("\r\n");
  record_video_bytes += fb->len; record_frames++; record_last_frame_ms = now;
  if ((record_frames % RECORD_FLUSH_FRAMES) == 0) record_video_file.flush();
  esp_camera_fb_return(fb); cam_mutex_give();
}

static String record_status_json() {
  String j = "{";
  j += "\"camera\":" + String(camera_ok ? 1 : 0);
  j += ",\"sd\":" + String(sd_ok ? 1 : 0);
  j += ",\"recording\":" + String(recording ? 1 : 0);
  j += ",\"frames\":" + String((unsigned long)record_frames);
  j += ",\"video\":\"" + record_video_path + "\"";
  j += "}";
  return j;
}
#endif // ENABLE_SD_RECORDING

// ── Servo PWM ─────────────────────────────────────────────────────────────────
static uint32_t servo_angle_to_duty(int angle) {
  angle = clamp_angle(angle);
  const uint32_t pulse_us = SERVO_MIN_US +
    ((uint32_t)(SERVO_MAX_US - SERVO_MIN_US) * (uint32_t)(angle - SERVO_CALIB_MIN_ANGLE)) /
    (uint32_t)(SERVO_CALIB_MAX_ANGLE - SERVO_CALIB_MIN_ANGLE);
  const uint32_t period_us = 1000000UL / SERVO_PWM_FREQ_HZ;
  const uint32_t max_duty = (1UL << SERVO_PWM_RES_BITS) - 1;
  uint32_t d = (pulse_us * max_duty) / period_us;
#if SERVO_DUTY_QUANTUM > 1
  const uint32_t q = (uint32_t)SERVO_DUTY_QUANTUM;
  d = (d / q) * q;
  if (d > max_duty) d = max_duty;
#endif
  return d;
}

static bool write_servo_pwm(int pin, int angle) {
  angle = clamp_angle(angle);
  const uint32_t duty = servo_angle_to_duty(angle);
#if ENABLE_SERVOS
  if (pin == SERVO_YAW_PIN) {
    if (duty == s_servo_last_duty_yaw) { s_servo_last_yaw = angle; return true; }
  } else if (pin == SERVO_PITCH_PIN) {
    if (duty == s_servo_last_duty_pitch) { s_servo_last_pitch = angle; return true; }
  }
#endif
  if (!ledcWrite(pin, duty)) return false;
#if ENABLE_SERVOS
  if (pin == SERVO_YAW_PIN)   { s_servo_last_yaw = angle; s_servo_last_duty_yaw = duty; }
  else if (pin == SERVO_PITCH_PIN) { s_servo_last_pitch = angle; s_servo_last_duty_pitch = duty; }
#endif
  return true;
}

static bool init_servo_pwm() {
#if !ENABLE_SERVOS
  return false;
#else
  delay(40);
  if (!ledcAttachChannel(SERVO_YAW_PIN,   SERVO_PWM_FREQ_HZ, SERVO_PWM_RES_BITS, SERVO_LEDC_CH_YAW)) {
    Serial.printf("Servo: ledcAttachChannel yaw gpio=%d failed\n", (int)SERVO_YAW_PIN); return false;
  }
  if (!ledcAttachChannel(SERVO_PITCH_PIN, SERVO_PWM_FREQ_HZ, SERVO_PWM_RES_BITS, SERVO_LEDC_CH_PITCH)) {
    Serial.printf("Servo: ledcAttachChannel pitch gpio=%d failed\n", (int)SERVO_PITCH_PIN); return false;
  }
  const uint32_t duty = servo_angle_to_duty(SERVO_HOME_ANGLE);
  ledcWrite(SERVO_YAW_PIN, duty); ledcWrite(SERVO_PITCH_PIN, duty);
  delay(25);
  ledcWrite(SERVO_YAW_PIN, duty); ledcWrite(SERVO_PITCH_PIN, duty);
  s_servo_last_yaw = s_servo_current_yaw = SERVO_HOME_ANGLE;
  s_servo_last_pitch = s_servo_current_pitch = SERVO_HOME_ANGLE;
  s_servo_last_duty_yaw = s_servo_last_duty_pitch = duty;
  s_servos_attached = true;
  s_servo_last_move_ms = s_servo_last_step_ms = millis();
  s_servo_moving = false;
  return true;
#endif
}

#if ENABLE_SERVOS
static void servo_detach() {
  if (!s_servos_attached) return;
  ledcDetach(SERVO_YAW_PIN); ledcDetach(SERVO_PITCH_PIN);
  s_servos_attached = false;
}

static bool servo_reattach() {
  if (s_servos_attached) return true;
  const uint32_t dy = servo_angle_to_duty(s_servo_current_yaw);
  const uint32_t dp = servo_angle_to_duty(s_servo_current_pitch);
  if (!ledcAttachChannel(SERVO_YAW_PIN, SERVO_PWM_FREQ_HZ, SERVO_PWM_RES_BITS, SERVO_LEDC_CH_YAW)) return false;
  ledcWrite(SERVO_YAW_PIN, dy);
  if (!ledcAttachChannel(SERVO_PITCH_PIN, SERVO_PWM_FREQ_HZ, SERVO_PWM_RES_BITS, SERVO_LEDC_CH_PITCH)) {
    ledcDetach(SERVO_YAW_PIN); return false;
  }
  ledcWrite(SERVO_PITCH_PIN, dp);
  s_servo_last_duty_yaw = dy; s_servo_last_duty_pitch = dp;
  s_servo_last_yaw = s_servo_current_yaw; s_servo_last_pitch = s_servo_current_pitch;
  s_servos_attached = true;
  return true;
}
#endif

static void set_servo_angles(int yaw, int pitch) {
  yaw_angle   = clamp_angle(yaw);
  pitch_angle = clamp_angle(pitch);
#if ENABLE_SERVOS
  if (servos_ok) {
    if (!s_servos_attached) servo_reattach();
    s_servo_moving = (s_servo_current_yaw != yaw_angle) || (s_servo_current_pitch != pitch_angle);
    s_servo_last_move_ms = millis(); s_servo_last_step_ms = 0;
  }
#endif
}

#if ENABLE_SERVOS
static int servo_step_toward(int cur, int tgt) {
  if (cur == tgt) return cur;
  const int d = tgt - cur;
  const int step = min(abs(d), max(1, SERVO_STEP_DEG));
  return cur + (d > 0 ? step : -step);
}

static void service_servo_motion() {
  if (!servos_ok || !s_servo_moving) return;
  const uint32_t now = millis();
  if (s_servo_last_step_ms && now - s_servo_last_step_ms < SERVO_MOVE_INTERVAL_MS) return;
  if (!s_servos_attached && !servo_reattach()) return;
  s_servo_last_step_ms = now;
  s_servo_current_yaw   = servo_step_toward(s_servo_current_yaw,   yaw_angle);
  s_servo_current_pitch = servo_step_toward(s_servo_current_pitch, pitch_angle);
  write_servo_pwm(SERVO_YAW_PIN,   s_servo_current_yaw);
  write_servo_pwm(SERVO_PITCH_PIN, s_servo_current_pitch);
  s_servo_last_move_ms = now;
  s_servo_moving = (s_servo_current_yaw != yaw_angle) || (s_servo_current_pitch != pitch_angle);
}
#endif

static void exit_power_save() {
  last_activity_ms = millis();
  if (!xiao_power_save_enabled) return;
  xiao_power_save_enabled = false;
  set_ring(LED_DEFAULT_R, LED_DEFAULT_G, LED_DEFAULT_B);
  Serial.println("POWER: awake");
}

static void enter_power_save() {
  if (xiao_power_save_enabled) return;
  xiao_power_save_enabled = true;
  stream_enabled = false;
#if ENABLE_VL53
  dist_stream_enabled = false;
#endif
#if ENABLE_C1001
  radar_stream_enabled = false;
#endif
  set_ring(0, 0, 0);
  set_servo_angles(SERVO_HOME_ANGLE, SERVO_HOME_ANGLE);
  Serial.println("POWER: sleep; LED off, streams stopped, servos centering for detach");
}

// ── VL53L0X ───────────────────────────────────────────────────────────────────
#if ENABLE_VL53
static bool vl53_read_mm(uint16_t* mm_out, uint8_t* status_out) {
  if (!vl53_ok) return false;
  VL53L0X_RangingMeasurementData_t m;
  vl53.rangingTest(&m, false);
  if (status_out) *status_out = m.RangeStatus;
  if (mm_out)     *mm_out     = m.RangeMilliMeter;
  return true;
}

static void i2c_scan_print(Stream& out) {
  out.println("I2C scan (Wire D4/D5):");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) { out.printf("  0x%02x\n", addr); found++; }
  }
  if (!found) out.println("  (none — check wiring)");
}

static bool init_vl53_sensor() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setTimeOut(250);
  const uint32_t clocks[] = { 100000UL, 50000UL, 400000UL };
  for (uint32_t hz : clocks) {
    Wire.setClock(hz); delay(120);
    if (vl53.begin(0x29, false, &Wire)) {
      Wire.setClock(100000);
      if (hz != 100000) Serial.printf("VL53L0X: OK (init at %lu Hz)\n", (unsigned long)hz);
      return true;
    }
  }
  Serial.println("VL53L0X: begin() failed — I2C scan:");
  i2c_scan_print(Serial);
  return false;
}
#endif

static void send_distance(Stream& out) {
#if !ENABLE_VL53
  send_err(out, "VL53_DISABLED");
#else
  uint16_t mm = 0; uint8_t st = 0;
  if (!vl53_read_mm(&mm, &st)) { send_err(out, "VL53L0X_NOT_READY"); return; }
  if (st == 4)                  { send_err(out, "DIST_OUT_OF_RANGE"); return; }
  out.printf("DIST %u\n", (unsigned)mm);
#endif
}

static void send_distance_raw(Stream& out) {
#if !ENABLE_VL53
  send_err(out, "VL53_DISABLED");
#else
  uint16_t mm = 0; uint8_t st = 0xFF;
  if (!vl53_read_mm(&mm, &st)) { send_err(out, "VL53L0X_NOT_READY"); return; }
  out.printf("DIST_RAW status=%u mm=%u\n", (unsigned)st, (unsigned)mm);
#endif
}

// ── DFRobot C1001 radar ───────────────────────────────────────────────────────
#if ENABLE_C1001

static bool init_c1001_radar() {
  // UART1: XIAO TX D6 -> sensor RX, XIAO RX D7 <- sensor TX.
  C1001Serial.begin(C1001_UART_BAUD, SERIAL_8N1, C1001_RX_PIN, C1001_TX_PIN);

  // SEN0623 needs time after power-up; the DFRobot library then adds a 10 s begin delay.
  Serial.println("C1001: waiting 3 s for sensor boot, then begin() waits up to 10 s...");
  delay(3000);

  // begin() returns 0 on success; retry up to 5 times with 1 s gaps
  uint8_t ret = 1;
  for (int attempt = 1; attempt <= 5 && ret != 0; attempt++) {
    ret = radar.begin();
    Serial.printf("C1001: begin() attempt %d → %d\n", attempt, ret);
    if (ret != 0) delay(1000);
  }
  if (ret != 0) {
    Serial.println("C1001: init failed — check 5V/GND and crossed UART wiring: sensor RX<-D6, sensor TX->D7");
    return false;
  }
  Serial.println("C1001: UART communication OK");

  // eFallingMode: full feature set (presence + motion + fall detection)
  Serial.println("C1001: configuring falling mode (can take 10 s when mode changes)...");
  bool falling_mode_ok = false;
  for (int attempt = 1; attempt <= 2 && !falling_mode_ok; ++attempt) {
    const uint8_t mode_ret = radar.configWorkMode(radar.eFallingMode);
    falling_mode_ok = mode_ret == 0;
    Serial.printf("C1001: falling mode attempt %d -> %u\n", attempt, (unsigned)mode_ret);
  }
  if (falling_mode_ok) {
    const uint8_t hp_led_ret = radar.configLEDLight(radar.eHPLed, 1);
    const uint8_t fall_led_ret = radar.configLEDLight(radar.eFALLLed, 0);
    if (hp_led_ret != 0 || fall_led_ret != 0) {
      Serial.printf("C1001: LED config skipped after timeout (hp=%u fall=%u); detection remains usable\n",
                    (unsigned)hp_led_ret, (unsigned)fall_led_ret);
    }
    radar.dmUnmannedTime(1);
    Serial.println("C1001: applying configuration and restarting sensor (about 10 s)...");
    if (radar.sensorRet() != 0) {
      Serial.println("C1001: sensor restart timed out; continuing with presence queries");
    }
    delay(200);
  } else {
    Serial.println("C1001: mode setup timed out; continuing in current mode for presence queries");
  }

  Serial.println("C1001: READY for RADAR? queries");
  return true;
}

static void send_radar(Stream& out) {
  if (!c1001_ok) { send_err(out, "C1001_NOT_READY"); return; }
  // smHumanData(type): eHumanPresence  0=absent 1=present
  //                    eHumanMovement  0=none 1=still 2=active
  //                    eHumanMovingRange — movement magnitude (cm)
  const uint8_t  presence = radar.smHumanData(radar.eHumanPresence);
  const uint8_t  motion   = radar.smHumanData(radar.eHumanMovement);
  const uint16_t range    = radar.smHumanData(radar.eHumanMovingRange);
  out.printf("RADAR presence=%u motion=%u range=%u\n",
             (unsigned)presence, (unsigned)motion, (unsigned)range);
}

static void service_radar_stream() {
  if (!radar_stream_enabled || !c1001_ok) return;
  const uint32_t now = millis();
  if (now - last_radar_ms < radar_stream_ms) return;
  last_radar_ms = now;
  const uint8_t  presence = radar.smHumanData(radar.eHumanPresence);
  const uint8_t  motion   = radar.smHumanData(radar.eHumanMovement);
  const uint16_t range    = radar.smHumanData(radar.eHumanMovingRange);
#if ENABLE_UART_LINK
  LinkSerial.printf("RADAR presence=%u motion=%u range=%u\n",
                    (unsigned)presence, (unsigned)motion, (unsigned)range);
#endif
  Serial.printf("RADAR presence=%u motion=%u range=%u\n",
                (unsigned)presence, (unsigned)motion, (unsigned)range);
}
#endif // ENABLE_C1001

// ── Command parser ────────────────────────────────────────────────────────────
static int read_int_arg(const String& s, int& pos) {
  while (pos < (int)s.length() && s[pos] == ' ') pos++;
  int start = pos;
  while (pos < (int)s.length() && (isDigit(s[pos]) || s[pos] == '-')) pos++;
  return s.substring(start, pos).toInt();
}

static void print_help(Stream& out) {
  send_line(out, "CMDS:");
  send_line(out, "PING");
  send_line(out, "STATUS?");
  send_line(out, "WIFI?");
  out.printf("SERVO <yaw> <pitch>  — %d..%d deg\n", SERVO_MIN_ANGLE, SERVO_MAX_ANGLE);
  send_line(out, "YAW <angle>");
  send_line(out, "PITCH <angle>");
  send_line(out, "LED <r> <g> <b>      — all 8 pixels");
  send_line(out, "PIX <idx> <r> <g> <b>");
  send_line(out, "LEDDEFAULT | LEDOFF | LEDTEST");
  send_line(out, "POWER SLEEP | POWER WAKE");
#if ENABLE_VL53
  send_line(out, "DIST? | DIST_RAW?");
  send_line(out, "DIST_STREAM ON [ms] | DIST_STREAM OFF");
#endif
#if ENABLE_C1001
  send_line(out, "RADAR?               — one-shot: presence motion dist");
  send_line(out, "RADAR_STREAM ON [ms] | RADAR_STREAM OFF");
#endif
  send_line(out, "FRAME");
  send_line(out, "STREAM ON [ms] | STREAM OFF");
#if ENABLE_SD_RECORDING
  send_line(out, "RECORD ON | RECORD OFF | RECORD?");
#endif
}

static void handle_command(String cmd, Stream& out) {
  cmd.trim();
  if (cmd.length() == 0) return;
  cmd.toUpperCase();
  if (cmd != "POWER SLEEP") {
    exit_power_save();
  }

  if (cmd == "PING") {
    send_line(out, "PONG");

  } else if (cmd == "HELP") {
    print_help(out);

  } else if (cmd == "STATUS?") {
    uint8_t wifi_channel = 0;
    wifi_second_chan_t secondary_channel = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&wifi_channel, &secondary_channel);
    out.printf("STATUS camera=%d vl53=%d c1001=%d sd=%d servos=%d led=%d"
               " stream=%d yaw=%d pitch=%d wifi=%d espnow=%d channel=%u power_save=%d\n",
               camera_ok ? 1 : 0,
#if ENABLE_VL53
               vl53_ok ? 1 : 0,
#else
               -1,
#endif
#if ENABLE_C1001
               c1001_ok ? 1 : 0,
#else
               -1,
#endif
               sd_ok ? 1 : 0,
               servos_ok ? 1 : 0,
               led_ring_ok ? 1 : 0,
               stream_enabled ? 1 : 0,
               yaw_angle, pitch_angle,
               wifi_ok ? 1 : 0,
#if ENABLE_ESPNOW
               espnow_ok ? 1 : 0,
#else
               -1,
#endif
               (unsigned)wifi_channel,
               xiao_power_save_enabled ? 1 : 0);

  } else if (cmd == "WIFI?") {
    if (!wifi_ok) {
      send_err(out, "WIFI_DOWN");
    } else {
      out.printf("WIFI ip=%s rssi=%d host=%s url=http://%s/\n",
                 WiFi.localIP().toString().c_str(), (int)WiFi.RSSI(),
                 WIFI_HOSTNAME, WiFi.localIP().toString().c_str());
    }

  } else if (cmd == "POWER SLEEP") {
    enter_power_save();
    send_ok(out, "POWER_SLEEP");

  } else if (cmd == "POWER WAKE") {
    exit_power_save();
    send_ok(out, "POWER_WAKE");

  } else if (cmd.startsWith("SERVO ")) {
    int pos = 6;
    int yaw = read_int_arg(cmd, pos), pitch = read_int_arg(cmd, pos);
    set_servo_angles(yaw, pitch);
    out.printf("OK SERVO yaw=%d pitch=%d\n", yaw_angle, pitch_angle);

  } else if (cmd.startsWith("YAW ")) {
    int pos = 4; set_servo_angles(read_int_arg(cmd, pos), pitch_angle);
    out.printf("OK YAW %d\n", yaw_angle);

  } else if (cmd.startsWith("PITCH ")) {
    int pos = 6; set_servo_angles(yaw_angle, read_int_arg(cmd, pos));
    out.printf("OK PITCH %d\n", pitch_angle);

  } else if (cmd.startsWith("LED ")) {
    if (!led_ring_ok) { send_err(out, "LED_RING_DISABLED"); return; }
    int pos = 4;
    int r = constrain(read_int_arg(cmd, pos), 0, 255);
    int g = constrain(read_int_arg(cmd, pos), 0, 255);
    int b = constrain(read_int_arg(cmd, pos), 0, 255);
    set_ring(r, g, b); send_ok(out, "LED");

  } else if (cmd.startsWith("PIX ")) {
    if (!led_ring_ok) { send_err(out, "LED_RING_DISABLED"); return; }
    int pos = 4;
    int idx = read_int_arg(cmd, pos);
    int r = constrain(read_int_arg(cmd, pos), 0, 255);
    int g = constrain(read_int_arg(cmd, pos), 0, 255);
    int b = constrain(read_int_arg(cmd, pos), 0, 255);
    if (idx < 0 || idx >= LED_RING_COUNT) { send_err(out, "PIX_INDEX"); return; }
    ring.setPixelColor(idx, ring.Color(r, g, b)); ring.show(); send_ok(out, "PIX");

  } else if (cmd == "LEDOFF") {
    if (!led_ring_ok) { send_err(out, "LED_RING_DISABLED"); return; }
    set_ring(0, 0, 0); send_ok(out, "LEDOFF");

  } else if (cmd == "LEDDEFAULT") {
    if (!led_ring_ok) { send_err(out, "LED_RING_DISABLED"); return; }
    set_ring(LED_DEFAULT_R, LED_DEFAULT_G, LED_DEFAULT_B); send_ok(out, "LEDDEFAULT");

  } else if (cmd == "LEDTEST") {
    led_test(out);

  } else if (cmd == "DIST?") {
    send_distance(out);

  } else if (cmd == "I2C?") {
    i2c_scan_print(out);

  } else if (cmd == "VL53 RETRY") {
#if ENABLE_VL53
    vl53_ok = init_vl53_sensor();
    if (vl53_ok) send_ok(out, "VL53"); else send_err(out, "VL53L0X_NOT_READY");
#else
    send_err(out, "VL53_DISABLED");
#endif

  } else if (cmd == "DIST_RAW?") {
    send_distance_raw(out);

  } else if (cmd.startsWith("DIST_STREAM ON")) {
#if ENABLE_VL53
    int pos = 14; int ms = read_int_arg(cmd, pos);
    if (ms > 0) dist_stream_ms = (uint32_t)constrain(ms, 50, 10000);
    dist_stream_enabled = true; last_dist_stream_ms = 0;
    out.printf("OK DIST_STREAM interval=%lu\n", (unsigned long)dist_stream_ms);
#else
    send_err(out, "VL53_DISABLED");
#endif

  } else if (cmd == "DIST_STREAM OFF") {
#if ENABLE_VL53
    dist_stream_enabled = false; send_ok(out, "DIST_STREAM_OFF");
#else
    send_err(out, "VL53_DISABLED");
#endif

  } else if (cmd == "RADAR?") {
#if ENABLE_C1001
    send_radar(out);
#else
    send_err(out, "C1001_DISABLED");
#endif

  } else if (cmd.startsWith("RADAR_STREAM ON")) {
#if ENABLE_C1001
    int pos = 15; int ms = read_int_arg(cmd, pos);
    if (ms > 0) radar_stream_ms = (uint32_t)constrain(ms, 50, 10000);
    radar_stream_enabled = true; last_radar_ms = 0;
    out.printf("OK RADAR_STREAM interval=%lu\n", (unsigned long)radar_stream_ms);
#else
    send_err(out, "C1001_DISABLED");
#endif

  } else if (cmd == "RADAR_STREAM OFF") {
#if ENABLE_C1001
    radar_stream_enabled = false; send_ok(out, "RADAR_STREAM_OFF");
#else
    send_err(out, "C1001_DISABLED");
#endif

  } else if (cmd == "FRAME") {
    send_frame(out);

  } else if (cmd.startsWith("STREAM ON")) {
    int pos = 9; int ms = read_int_arg(cmd, pos);
    if (ms > 0) stream_interval_ms = constrain(ms, 100, 10000);
    stream_enabled = true; last_stream_ms = 0;
    out.printf("OK STREAM interval=%lu\n", (unsigned long)stream_interval_ms);

  } else if (cmd == "STREAM OFF") {
    stream_enabled = false; send_ok(out, "STREAM_OFF");

#if ENABLE_SD_RECORDING
  } else if (cmd == "RECORD ON")  { start_recording(&out);
  } else if (cmd == "RECORD OFF") { stop_recording(&out);
  } else if (cmd == "RECORD?") {
    out.printf("RECORD sd=%d rec=%d frames=%lu video=%s\n",
               sd_ok ? 1 : 0, recording ? 1 : 0,
               (unsigned long)record_frames, record_video_path.c_str());
#endif

  } else {
    send_err(out, "UNKNOWN_CMD");
  }
}

#if ENABLE_ESPNOW
static void espnow_receive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (!espnow_command_queue || !info ||
      len < (int)(sizeof(EspNowPacket) - ESPNOW_PAYLOAD_SIZE + 1)) {
    return;
  }
  const auto* packet = reinterpret_cast<const EspNowPacket*>(data);
  if (packet->magic != ESPNOW_MAGIC || packet->version != ESPNOW_VERSION ||
      packet->type != ESPNOW_COMMAND) {
    return;
  }
  const size_t header_len = sizeof(EspNowPacket) - ESPNOW_PAYLOAD_SIZE;
  const size_t available = min((size_t)len - header_len, ESPNOW_PAYLOAD_SIZE);
  EspNowQueuedCommand queued = {};
  memcpy(queued.sender, info->src_addr, ESP_NOW_ETH_ALEN);
  queued.sequence = packet->sequence;
  size_t command_len = strnlen(packet->payload, available);
  command_len = min(command_len, ESPNOW_PAYLOAD_SIZE - 1);
  memcpy(queued.command, packet->payload, command_len);
  queued.command[command_len] = '\0';
  xQueueSend(espnow_command_queue, &queued, 0);
}

static bool init_espnow() {
  espnow_command_queue = xQueueCreate(6, sizeof(EspNowQueuedCommand));
  if (!espnow_command_queue) {
    Serial.println("ESP-NOW: queue allocation failed");
    return false;
  }
  if (WiFi.getMode() == WIFI_OFF) {
    WiFi.mode(WIFI_STA);
  }
  if (!wifi_ok) {
    esp_err_t channel_result = esp_wifi_set_channel(ESPNOW_FALLBACK_CHANNEL, WIFI_SECOND_CHAN_NONE);
    Serial.printf("ESP-NOW: WiFi offline, forcing channel=%u (%s)\n",
                  (unsigned)ESPNOW_FALLBACK_CHANNEL, esp_err_to_name(channel_result));
  }
  esp_err_t result = esp_now_init();
  if (result != ESP_OK) {
    Serial.printf("ESP-NOW: init failed: %s\n", esp_err_to_name(result));
    return false;
  }
  result = esp_now_register_recv_cb(espnow_receive);
  if (result != ESP_OK) {
    Serial.printf("ESP-NOW: receive callback failed: %s\n", esp_err_to_name(result));
    esp_now_deinit();
    return false;
  }
  uint8_t primary = 0;
  wifi_second_chan_t secondary = WIFI_SECOND_CHAN_NONE;
  esp_wifi_get_channel(&primary, &secondary);
  Serial.printf("ESP-NOW: ready mac=%s channel=%u\n", WiFi.macAddress().c_str(), primary);
  return true;
}

static void service_espnow() {
  if (!espnow_ok || !espnow_command_queue) return;
  EspNowQueuedCommand queued = {};
  while (xQueueReceive(espnow_command_queue, &queued, 0) == pdTRUE) {
    EspNowResponseStream response;
    String command(queued.command);
    if (command == "FRAME" || command.startsWith("STREAM ")) {
      send_err(response, "CAMERA_USE_WIFI_OR_UART");
    } else {
      handle_command(command, response);
    }
    response.value.trim();

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, queued.sender, ESP_NOW_ETH_ALEN);
    peer.channel = 0;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    if (!esp_now_is_peer_exist(queued.sender)) {
      esp_now_add_peer(&peer);
    }

    EspNowPacket packet = {};
    packet.magic = ESPNOW_MAGIC;
    packet.version = ESPNOW_VERSION;
    packet.type = ESPNOW_RESPONSE;
    packet.sequence = queued.sequence;
    response.value.toCharArray(packet.payload, ESPNOW_PAYLOAD_SIZE);
    const size_t length = sizeof(EspNowPacket) - ESPNOW_PAYLOAD_SIZE +
                          strlen(packet.payload) + 1;
    esp_now_send(queued.sender, reinterpret_cast<const uint8_t*>(&packet), length);
    Serial.printf("ESP-NOW cmd=%s reply=%s\n", queued.command, packet.payload);
  }
}

static void espnow_service_task(void*) {
  while (true) {
    service_espnow();
    vTaskDelay(pdMS_TO_TICKS(2));
  }
}
#endif

// ── Serial line reader ────────────────────────────────────────────────────────
static void poll_stream(Stream& in, String& buf) {
  while (in.available()) {
    char c = (char)in.read();
    if (c == '\r') continue;
    if (c == '\n') {
      String cmd = buf; buf = "";
      handle_command(cmd, in);
    } else if (buf.length() < 160) {
      buf += c;
    } else {
      buf = ""; send_err(in, "LINE_TOO_LONG");
    }
  }
}

// ── HTTP handlers ─────────────────────────────────────────────────────────────
static void handle_http_capture() {
  if (!camera_ok)         { server.send(503, "text/plain", "camera not ready"); return; }
  if (!cam_mutex_take(4000)) { server.send(503, "text/plain", "camera busy"); return; }
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) { cam_mutex_give(); server.send(500, "text/plain", "capture failed"); return; }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.setContentLength(fb->len);
  server.send(200, "image/jpeg", "");
  server.client().write(fb->buf, fb->len);
  esp_camera_fb_return(fb); cam_mutex_give();
}

static void handle_http_stream() {
  if (!camera_ok) { server.send(503, "text/plain", "camera not ready"); return; }
  WiFiClient client = server.client();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println("Access-Control-Allow-Origin: *");
  client.println("Cache-Control: no-cache"); client.println();
  while (client.connected()) {
    poll_stream(Serial, usb_line);
#if ENABLE_UART_LINK
    poll_stream(LinkSerial, link_line);
#endif
    if (!cam_mutex_take(2000)) { delay(5); continue; }
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) { cam_mutex_give(); delay(5); continue; }
    client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", (unsigned)fb->len);
    const size_t w = client.write(fb->buf, fb->len);
    client.print("\r\n");
    esp_camera_fb_return(fb); cam_mutex_give();
    if (w != fb->len) break;
    delay(1);
  }
}

static void handle_http_root() {
  const char* html =
    "<!DOCTYPE html><html><head><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>XIAO S3</title>"
    "<style>body{font-family:system-ui,sans-serif;margin:16px;background:#1a1a1a;color:#eee}"
    "button{font:inherit;padding:10px 14px;margin:0 8px 12px 0;border:0;border-radius:4px;cursor:pointer}"
    "#bRec{background:#c62828;color:#fff}#bStop{background:#333;color:#eee;border:1px solid #555}"
    "img{max-width:100%;border-radius:4px;border:1px solid #444;background:#111}"
    "#st{margin:10px 0;color:#b8d7ff;font-size:.9em}</style></head><body>"
    "<h1>XIAO ESP32-S3</h1>"
    "<p><a href='/stream'>MJPEG stream</a> &middot; <a href='/capture'>JPEG</a></p>"
    "<button id=bRec>Record</button><button id=bStop>Stop</button>"
    "<div id=st>–</div><img id=cam><br>"
    "<script>"
    "const img=document.getElementById('cam'),st=document.getElementById('st');"
    "async function api(u){try{await fetch(u,{method:'POST'})}catch(e){};await poll()}"
    "async function poll(){try{let r=await fetch('/status');st.textContent=await r.text();}catch(e){}}"
    "document.getElementById('bRec').onclick=()=>api('/record/start');"
    "document.getElementById('bStop').onclick=()=>api('/record/stop');"
    "setInterval(()=>{img.src='/capture?t='+Date.now();},700);"
    "setInterval(poll,1500);poll();"
    "</script></body></html>";
  server.send(200, "text/html", html);
}

static void handle_http_status() {
  String s = "camera="; s += camera_ok ? "1" : "0";
  s += " vl53=";
#if ENABLE_VL53
  s += vl53_ok ? "1" : "0";
#else
  s += "-1";
#endif
  s += " c1001=";
#if ENABLE_C1001
  s += c1001_ok ? "1" : "0";
#else
  s += "-1";
#endif
  s += " servos="; s += servos_ok ? "1" : "0";
  s += " led="; s += led_ring_ok ? "1" : "0";
  s += " yaw="; s += String(yaw_angle);
  s += " pitch="; s += String(pitch_angle);
  s += " wifi="; s += wifi_ok ? "1" : "0";
  server.send(200, "text/plain", s);
}

static void upload_vision_frame() {
#if ENABLE_VISION_UPLOAD
  if (!wifi_ok || !camera_ok || xiao_power_save_enabled) return;
  if (!cam_mutex_take(2000)) return;
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    cam_mutex_give();
    return;
  }

  HTTPClient http;
  http.setTimeout(3000);
  if (http.begin(VISION_UPLOAD_URL)) {
    http.addHeader("Content-Type", "image/jpeg");
    http.addHeader("X-Device-Id", VISION_DEVICE_ID);
    int status = http.POST(fb->buf, fb->len);
    if (status < 200 || status >= 300) {
      Serial.printf("VISION: upload failed status=%d bytes=%u\n", status, (unsigned)fb->len);
    } else {
      Serial.printf("VISION: frame accepted bytes=%u\n", (unsigned)fb->len);
    }
    http.end();
  }
  esp_camera_fb_return(fb);
  cam_mutex_give();
#endif
}

static void vision_upload_worker(void*) {
  while (true) {
#if ENABLE_VISION_UPLOAD
    if (wifi_ok && camera_ok && !xiao_power_save_enabled &&
        millis() - last_vision_upload_ms >= VISION_UPLOAD_INTERVAL_MS) {
      last_vision_upload_ms = millis();
      upload_vision_frame();
    }
#endif
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

static void start_wifi_and_http() {
  wifi_ok = false;
  if (WIFI_SSID[0] == '\0') { Serial.println("WiFi: skipped (empty SSID)"); return; }
  WiFi.mode(WIFI_STA); WiFi.setHostname(WIFI_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("WiFi: connecting to \"%s\"...\n", WIFI_SSID);
  const uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 25000) { delay(300); Serial.print('.'); }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi: failed; stopping background reconnect so ESP-NOW channel remains fixed");
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
    delay(100);
    return;
  }
  wifi_ok = true;
  Serial.printf("WiFi: %s  http://%s/\n", WiFi.localIP().toString().c_str(), WiFi.localIP().toString().c_str());
  if (MDNS.begin(WIFI_HOSTNAME)) { MDNS.addService("http","tcp",80); Serial.printf("mDNS: http://%s.local/\n", WIFI_HOSTNAME); }
  server.on("/",            HTTP_GET,  handle_http_root);
  server.on("/capture",     HTTP_GET,  handle_http_capture);
  server.on("/stream",      HTTP_GET,  handle_http_stream);
  server.on("/status",      HTTP_GET,  handle_http_status);
#if ENABLE_SD_RECORDING
  server.on("/record/start",  HTTP_GET,  []{ if(start_recording()) server.send(200,"text/plain","OK"); else server.send(503,"text/plain","ERR"); });
  server.on("/record/start",  HTTP_POST, []{ if(start_recording()) server.send(200,"text/plain","OK"); else server.send(503,"text/plain","ERR"); });
  server.on("/record/stop",   HTTP_GET,  []{ stop_recording(); server.send(200,"text/plain","OK"); });
  server.on("/record/stop",   HTTP_POST, []{ stop_recording(); server.send(200,"text/plain","OK"); });
#endif
  server.begin();
}

// ── setup ─────────────────────────────────────────────────────────────────────
void setup() {
  s_cam_mutex = xSemaphoreCreateMutex();

  Serial.begin(115200);
  const uint32_t t0 = millis();
  while (!Serial && millis() - t0 < 2000) delay(10);
  Serial.println();
  Serial.println("BOOT XIAO_SENSOR_BRIDGE");
  Serial.printf("Build: VL53=%d C1001=%d SERVOS=%d SD=%d AUDIO=%d\n",
                ENABLE_VL53, ENABLE_C1001, ENABLE_SERVOS, ENABLE_SD_RECORDING, ENABLE_AUDIO_RECORDING);

  // Legacy UART link to main MCU. Disabled when ESP-NOW is used because D2 is a servo pin.
#if ENABLE_UART_LINK
  LinkSerial.begin(LINK_UART_BAUD, SERIAL_8N1, LINK_UART_RX_PIN, LINK_UART_TX_PIN);
  LinkSerial.println("BOOT XIAO_SENSOR_BRIDGE");
#endif

  // Start the control link before optional peripherals: a missing sensor must
  // not prevent the AMOLED controller from reaching the bridge.
  start_wifi_and_http();
#if ENABLE_ESPNOW
  espnow_ok = init_espnow();
  if (espnow_ok &&
      xTaskCreate(espnow_service_task, "espnow_service", 4096, nullptr, 2, nullptr) != pdPASS) {
    Serial.println("ESP-NOW: service task allocation failed");
    espnow_ok = false;
  }
#endif

  // SD card (disabled by default — shares D8/D9/D10 with LED ring + servos)
  sd_ok = init_sd_card();

  // LED ring on D8
  Serial.printf("LED ring gpio=%d count=%d\n", LED_RING_PIN, LED_RING_COUNT);
  if (sd_ok && pin_uses_sd_spi(LED_RING_PIN)) {
    Serial.println("LED ring: DISABLED (gpio shared with SD SPI)");
  } else {
    pinMode(LED_RING_PIN, OUTPUT);
    ring.begin(); ring.setBrightness(180); ring.clear(); ring.show();
    led_ring_ok = true;
    // Boot color sweep: R → G → B → default
    set_ring(80, 0, 0);   delay(200);
    set_ring(0, 80, 0);   delay(200);
    set_ring(0, 0, 80);   delay(200);
    set_ring(LED_DEFAULT_R, LED_DEFAULT_G, LED_DEFAULT_B);
    Serial.println("LED ring: OK");
  }

  // Camera (SCCB on I2C1 @ GPIO40/39 — independent of VL53 Wire on D4/D5)
  Serial.println("Camera init...");
  camera_ok = init_camera();
  Serial.printf("Camera: %s\n", camera_ok ? "OK" : "FAILED");

  // VL53L0X on I2C0 (Wire) @ D4/D5 — init AFTER camera (SCCB already claimed I2C1)
#if ENABLE_VL53
  Serial.printf("VL53L0X SDA=D%d(GPIO%d) SCL=D%d(GPIO%d)\n", 4, I2C_SDA_PIN, 5, I2C_SCL_PIN);
  vl53_ok = init_vl53_sensor();
  Serial.printf("VL53L0X: %s\n", vl53_ok ? "OK" : "FAILED");
#else
  Serial.println("VL53L0X: disabled (ENABLE_VL53=0)");
#endif

  // DFRobot C1001 radar on UART1 @ D6/D7.
#if ENABLE_C1001
  Serial.printf("C1001 radar TX=D6(GPIO%d) RX=D7(GPIO%d) baud=%d; wire sensor RX<-D6 TX->D7\n",
                C1001_TX_PIN, C1001_RX_PIN, C1001_UART_BAUD);
  c1001_ok = init_c1001_radar();
  Serial.printf("C1001: %s\n", c1001_ok ? "OK" : "FAILED");
#else
  Serial.println("C1001: disabled (ENABLE_C1001=0)");
#endif

  // Servos on D1 (yaw) / D2 (pitch).
  Serial.printf("Servos yaw=D1(GPIO%d) pitch=D2(GPIO%d) range=%d..%d home=%d\n",
                SERVO_YAW_PIN, SERVO_PITCH_PIN, SERVO_MIN_ANGLE, SERVO_MAX_ANGLE, SERVO_HOME_ANGLE);
#if ENABLE_SERVOS
  if (sd_ok && (pin_uses_sd_spi(SERVO_YAW_PIN) || pin_uses_sd_spi(SERVO_PITCH_PIN))) {
    Serial.println("Servos: DISABLED (pins shared with SD SPI)");
  } else {
    servos_ok = init_servo_pwm();
    Serial.printf("Servos: %s\n", servos_ok ? "OK" : "FAILED");
  }
#else
  Serial.println("Servos: disabled (ENABLE_SERVOS=0)");
#endif

  set_ring(LED_DEFAULT_R, LED_DEFAULT_G, LED_DEFAULT_B);
  last_activity_ms = millis();

#if ENABLE_VISION_UPLOAD
  if (wifi_ok && camera_ok) {
    if (xTaskCreate(vision_upload_worker, "vision_upload", 6144, nullptr, 1,
                    &vision_upload_task) != pdPASS) {
      Serial.println("VISION: failed to start upload task");
      vision_upload_task = nullptr;
    } else {
      Serial.printf("VISION: upload enabled interval=%lums endpoint=%s\n",
                    (unsigned long)VISION_UPLOAD_INTERVAL_MS, VISION_UPLOAD_URL);
    }
  }
#endif

  Serial.println("Ready. Type HELP.");
#if ENABLE_UART_LINK
  LinkSerial.println("READY XIAO_SENSOR_BRIDGE");
#endif
}

// ── loop ──────────────────────────────────────────────────────────────────────
void loop() {
  if (wifi_ok) server.handleClient();
  poll_stream(Serial, usb_line);
#if ENABLE_UART_LINK
  poll_stream(LinkSerial, link_line);
#endif
#if ENABLE_SERVOS
  service_servo_motion();
  if (xiao_power_save_enabled && servos_ok && s_servos_attached && !s_servo_moving) {
    servo_detach();
  }
#endif

#if ENABLE_SD_RECORDING
  service_recording();
#endif

#if ENABLE_C1001
  service_radar_stream();
#endif

#if ENABLE_VL53
  if (dist_stream_enabled && vl53_ok && millis() - last_dist_stream_ms >= dist_stream_ms) {
    last_dist_stream_ms = millis();
    uint16_t mm = 0; uint8_t st = 0;
    if (vl53_read_mm(&mm, &st)) {
      if (st != 4) {
        Serial.printf("DIST %u\n", (unsigned)mm);
#if ENABLE_UART_LINK
        LinkSerial.printf("DIST %u\n", (unsigned)mm);
#endif
      } else {
#if ENABLE_UART_LINK
        LinkSerial.println("DIST OOR");
#endif
      }
    }
  }
#endif

  if (stream_enabled && millis() - last_stream_ms >= stream_interval_ms) {
    last_stream_ms = millis();
#if ENABLE_UART_LINK
    send_frame(LinkSerial);
#endif
  }

#if ENABLE_SERVOS && SERVO_RELAX_MS > 0
  if (servos_ok && s_servos_attached && !s_servo_moving &&
      (uint32_t)(millis() - s_servo_last_move_ms) >= SERVO_RELAX_MS) {
    servo_detach();
  }
#endif

  if (!xiao_power_save_enabled &&
      (uint32_t)(millis() - last_activity_ms) >= XIAO_POWER_SAVE_TIMEOUT_MS) {
    enter_power_save();
  }

  // Heartbeat every 5 s
  if (millis() - last_heartbeat_ms >= 5000) {
    last_heartbeat_ms = millis();
    uint8_t wifi_channel = 0;
    wifi_second_chan_t secondary_channel = WIFI_SECOND_CHAN_NONE;
    esp_wifi_get_channel(&wifi_channel, &secondary_channel);
    Serial.printf("ALIVE cam=%d vl53=%d c1001=%d servos=%d led=%d yaw=%d pitch=%d"
                  " wifi=%d espnow=%d channel=%u power_save=%d\n",
                  camera_ok,
#if ENABLE_VL53
                  vl53_ok,
#else
                  -1,
#endif
#if ENABLE_C1001
                  c1001_ok,
#else
                  -1,
#endif
                  servos_ok, led_ring_ok, yaw_angle, pitch_angle, wifi_ok,
#if ENABLE_ESPNOW
                  espnow_ok ? 1 : 0,
#else
                  -1,
#endif
                  (unsigned)wifi_channel,
                  xiao_power_save_enabled ? 1 : 0);
  }

  delay(2);
}
