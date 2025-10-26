#ifndef BNO085_H
#define BNO085_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdbool.h>

// --- Configuration ---
#define BNO085_I2C_ADDR_DEFAULT 0x4A
#define BNO085_I2C_BAUD_HZ      400000 // 400 KHz

// --- Constants ---
#define SHTP_HEADER_LEN 4
#define SHTP_CHANNEL_CONTROL  1
#define SHTP_CHANNEL_REPORTS  3

// --- Feature Report IDs (from SH-2 Reference Manual) ---
#define FEAT_ACCELEROMETER        0x01
#define FEAT_GAME_ROTATION_VECTOR 0x08
#define SH2_REPORT_SET_FEATURE    0xFD

// --- Data Structures ---
typedef struct {
    bool valid;
    uint32_t timestamp_us;
    float qi, qj, qk, qr;
} bno085_quat_t;

typedef struct {
    bool valid;
    uint32_t timestamp_us;
    float ax, ay, az; // m/s^2
} bno085_accel_t;

typedef struct {
    bno085_quat_t quat;
    bno085_accel_t accel;
} bno085_outputs_t;

typedef struct {
    i2c_inst_t *i2c;
    uint8_t i2c_addr;
    int8_t rst_pin;
    uint8_t seq[4];
    uint8_t tx_buf[128];
    uint8_t rx_buf[256];
    bno085_outputs_t out;
} bno085_t;

// --- Public API ---
bool bno085_init(bno085_t *dev, i2c_inst_t *i2c, int8_t rst_pin);
bool bno085_enable_reports(bno085_t *dev, uint32_t quat_interval_us, uint32_t accel_interval_us);
bool bno085_poll(bno085_t *dev);
bno085_outputs_t bno085_get_outputs(bno085_t *dev);

#endif // BNO085_H