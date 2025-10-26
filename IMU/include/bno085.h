#ifndef BNO085_H
#define BNO085_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// I2C address (default 0x4A; becomes 0x4B if DI pulled high)
#define BNO085_I2C_ADDR_DEFAULT 0x4A

// Recommended I2C frequency for BNO08x
#define BNO085_I2C_BAUD_HZ 400000

// SHTP constants
#define SHTP_HEADER_LEN 4
// Channels (per SH-2 / SHTP; minimal set we need)
#define SHTP_CHANNEL_COMMAND   0
#define SHTP_CHANNEL_CONTROL   2
#define SHTP_CHANNEL_REPORTS   3

// SH-2 Control report IDs (host->sensor)
#define SH2_REPORT_SET_FEATURE    0xFD
#define SH2_REPORT_GET_FEATURE    0xFE

// Common feature report IDs (sensor->host) — confirm with datasheet
// We enable: Accel + Game Rotation Vector (low-latency quaternion)
// If you prefer Absolute Rotation Vector, swap the ID below accordingly.
#define FEAT_ACCELEROMETER            0x01
#define FEAT_GAME_ROTATION_VECTOR     0x28   // Optimized for low latency (quaternion)

// Parsed outputs
typedef struct {
    bool valid;
    // quaternion: (i, j, k, real)
    float qi, qj, qk, qr;
    uint32_t timestamp_us;
} bno085_quat_t;

typedef struct {
    bool valid;
    float ax, ay, az; // m/s^2
    uint32_t timestamp_us;
} bno085_accel_t;

typedef struct {
    // last-known outputs
    bno085_quat_t quat;
    bno085_accel_t accel;
} bno085_outputs_t;

typedef struct {
    i2c_inst_t *i2c;
    uint8_t i2c_addr;      // 0x4A or 0x4B
    uint8_t seq[6];        // sequence counters per channel (we use a few)
    // scratch buffers (big enough for typical SHTP frames; tune as needed)
    uint8_t rx_buf[256];
    uint8_t tx_buf[64];
    bno085_outputs_t out;
} bno085_t;

// Public API
bool bno085_init(bno085_t *dev, i2c_inst_t *i2c, uint8_t i2c_addr);
bool bno085_enable_reports(bno085_t *dev, uint32_t quat_interval_us,
                           uint32_t accel_interval_us);
bool bno085_poll(bno085_t *dev); // non-blocking poll: parses any pending report

// Helpers to fetch the latest decoded values
static inline bno085_outputs_t bno085_get_outputs(const bno085_t *dev) { return dev->out; }

#endif // BNO085_H
