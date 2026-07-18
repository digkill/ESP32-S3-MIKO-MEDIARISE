#include "vl53l0x.h"
#include "config.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "VL53L0X";

#define VL53_ADDR 0x29

// Register addresses
#define REG_IDENTIFICATION_MODEL_ID         0xC0
#define REG_VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV 0x89
#define REG_MSRC_CONFIG_CONTROL             0x60
#define REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT 0x44
#define REG_SYSTEM_SEQUENCE_CONFIG          0x01
#define REG_DYNAMIC_SPAD_REF_EN_START_OFFSET 0x4F
#define REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD 0x4E
#define REG_GLOBAL_CONFIG_REF_EN_START_SELECT 0xB6
#define REG_SYSTEM_INTERRUPT_CONFIG_GPIO    0x0A
#define REG_GPIO_HV_MUX_ACTIVE_HIGH         0x84
#define REG_SYSTEM_INTERRUPT_CLEAR          0x0B
#define REG_RESULT_INTERRUPT_STATUS         0x13
#define REG_SYSRANGE_START                  0x00
#define REG_RESULT_RANGE_STATUS             0x14
#define REG_OSC_CALIBRATE_VAL               0xF8
#define REG_GLOBAL_CONFIG_VCSEL_WIDTH       0x32
#define REG_ALGO_PHASECAL_LIM               0x30
#define REG_ALGO_PHASECAL_CONFIG_TIMEOUT    0x30

static i2c_master_bus_handle_t s_bus = nullptr;
static i2c_master_dev_handle_t s_dev = nullptr;
static bool s_ok = false;

static esp_err_t write_reg(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_dev, buf, 2, pdMS_TO_TICKS(50));
}

static esp_err_t read_reg(uint8_t reg, uint8_t* out) {
    return i2c_master_transmit_receive(s_dev, &reg, 1, out, 1, pdMS_TO_TICKS(50));
}

static esp_err_t read_regs(uint8_t reg, uint8_t* buf, size_t len) {
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, len, pdMS_TO_TICKS(100));
}

#if ENABLE_VL53_SCAN_ON_FAIL
static void i2c_scan(void) {
    ESP_LOGI(TAG, "I2C scan (SDA=GPIO%d SCL=GPIO%d):", I2C_SDA_GPIO, I2C_SCL_GPIO);
    for (uint8_t addr = 1; addr < 127; addr++) {
        esp_err_t probe = i2c_master_probe(s_bus, addr, pdMS_TO_TICKS(20));
        if (probe == ESP_OK) ESP_LOGI(TAG, "  found 0x%02x", addr);
    }
}
#endif

bool vl53_init(void) {
    i2c_master_bus_config_t bus_cfg = {};
    bus_cfg.i2c_port            = (i2c_port_num_t)I2C_PORT;
    bus_cfg.sda_io_num          = (gpio_num_t)I2C_SDA_GPIO;
    bus_cfg.scl_io_num          = (gpio_num_t)I2C_SCL_GPIO;
    bus_cfg.clk_source          = I2C_CLK_SRC_DEFAULT;
    bus_cfg.glitch_ignore_cnt   = 7;
    bus_cfg.flags.enable_internal_pullup = true;

    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "bus init failed: %s", esp_err_to_name(err));
        return false;
    }

    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address  = VL53_ADDR;
    dev_cfg.scl_speed_hz    = I2C_FREQ_HZ;

    // Try 100 kHz, 50 kHz, 400 kHz
    const uint32_t speeds[] = {100000, 50000, 400000};
    for (uint32_t hz : speeds) {
        dev_cfg.scl_speed_hz = hz;
        if (i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev) != ESP_OK) continue;

        uint8_t model_id = 0;
        if (read_reg(REG_IDENTIFICATION_MODEL_ID, &model_id) == ESP_OK && model_id == 0xEE) {
            ESP_LOGI(TAG, "found at 0x29 (model=0xEE) speed=%lu Hz", (unsigned long)hz);
            goto init_device;
        }
        i2c_master_bus_rm_device(s_dev);
        s_dev = nullptr;
        vTaskDelay(pdMS_TO_TICKS(30));
    }

    ESP_LOGW(TAG, "device not found at 0x29 (SDA=GPIO%d SCL=GPIO%d)",
             I2C_SDA_GPIO, I2C_SCL_GPIO);
#if ENABLE_VL53_SCAN_ON_FAIL
    ESP_LOGI(TAG, "scan:");
    i2c_scan();
#endif
    i2c_del_master_bus(s_bus);
    s_bus = nullptr;
    return false;

init_device:
    // Enable 2.8V mode
    uint8_t v = 0;
    read_reg(REG_VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV, &v);
    write_reg(REG_VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV, v | 0x01);

    // Standard init sequence from ST reference driver
    write_reg(0x88, 0x00);
    write_reg(0x80, 0x01);
    write_reg(0xFF, 0x01);
    write_reg(0x00, 0x00);
    uint8_t stop_variable = 0;
    read_reg(0x91, &stop_variable);
    write_reg(0x00, 0x01);
    write_reg(0xFF, 0x00);
    write_reg(0x80, 0x00);

    // Disable SIGNAL_RATE_MSRC and SIGNAL_RATE_PRE_RANGE limit checks
    uint8_t msrc = 0;
    read_reg(REG_MSRC_CONFIG_CONTROL, &msrc);
    write_reg(REG_MSRC_CONFIG_CONTROL, msrc | 0x12);

    // Set signal rate limit to 0.1 MCPS (unit: 9.7 fixed point)
    uint8_t limit_buf[2] = {0x00, 0x33}; // ~0.1 MCPS
    uint8_t limit_reg = REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT;
    i2c_master_transmit(s_dev, (uint8_t[]){limit_reg, limit_buf[0], limit_buf[1]}, 3, pdMS_TO_TICKS(50));

    write_reg(REG_SYSTEM_SEQUENCE_CONFIG, 0xFF);

    // Configure GPIO interrupt: level low when new data ready
    read_reg(REG_GPIO_HV_MUX_ACTIVE_HIGH, &v);
    write_reg(REG_GPIO_HV_MUX_ACTIVE_HIGH, v & ~0x10);
    write_reg(REG_SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04); // new sample ready

    write_reg(REG_SYSTEM_INTERRUPT_CLEAR, 0x01);

    // Set sequence steps for default accuracy
    write_reg(REG_SYSTEM_SEQUENCE_CONFIG, 0xE8);

    // Start continuous ranging (period 0 = back-to-back)
    write_reg(0xFF, 0x01);
    write_reg(0x00, 0x00);
    write_reg(0x91, stop_variable);
    write_reg(0x00, 0x01);
    write_reg(0xFF, 0x00);
    write_reg(REG_SYSRANGE_START, 0x02); // continuous mode

    s_ok = true;
    ESP_LOGI(TAG, "OK SDA=GPIO%d SCL=GPIO%d", I2C_SDA_GPIO, I2C_SCL_GPIO);
    return true;
}

bool vl53_is_ok(void) { return s_ok; }

bool vl53_read(uint16_t* mm_out, uint8_t* status_out) {
    if (!s_ok) return false;

    // Poll until measurement ready (bit 0 of interrupt status)
    uint8_t int_status = 0;
    int retries = 0;
    do {
        if (read_reg(REG_RESULT_INTERRUPT_STATUS, &int_status) != ESP_OK) return false;
        if (++retries > 50) return false;
        vTaskDelay(pdMS_TO_TICKS(5));
    } while ((int_status & 0x07) == 0);

    // Read 12 bytes from RESULT_RANGE_STATUS
    uint8_t buf[12] = {};
    if (read_regs(REG_RESULT_RANGE_STATUS, buf, 12) != ESP_OK) return false;

    // Clear interrupt
    write_reg(REG_SYSTEM_INTERRUPT_CLEAR, 0x01);

    if (status_out) *status_out = (buf[0] & 0x38) >> 3;
    if (mm_out)     *mm_out     = (uint16_t)((buf[10] << 8) | buf[11]);
    return true;
}
