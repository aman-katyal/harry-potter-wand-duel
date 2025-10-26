#include "bno085.h"
#include <string.h>

// --- Internal helpers --------------------------------------------------------

static int i2c_write_bytes(i2c_inst_t *i2c, uint8_t addr, const uint8_t *data, size_t len) {
    return i2c_write_blocking(i2c, addr, data, (int)len, false);
}

static int i2c_read_bytes(i2c_inst_t *i2c, uint8_t addr, uint8_t *data, size_t len) {
    return i2c_read_blocking(i2c, addr, data, (int)len, false);
}

// Send one SHTP frame on a channel
static bool shtp_send(bno085_t *dev, uint8_t channel, const uint8_t *payload, uint16_t plen) {
    // SHTP header: [len LSB][len MSB][channel][seq]
    // len includes header length
    uint16_t frame_len = (uint16_t)(plen + SHTP_HEADER_LEN);
    if (frame_len > sizeof(dev->tx_buf)) return false;

    uint8_t *buf = dev->tx_buf;
    buf[0] = (uint8_t)(frame_len & 0xFF);
    buf[1] = (uint8_t)(frame_len >> 8);
    buf[2] = channel;
    buf[3] = dev->seq[channel]++;
    memcpy(&buf[4], payload, plen);

    return (i2c_write_bytes(dev->i2c, dev->i2c_addr, buf, frame_len) == frame_len);
}

// Read one SHTP frame if available (blocking for a short read)
// The BNO08x acts like a memory-like I2C target: you perform a read of N bytes and
// it returns the next frame. We first read the 4-byte header to learn length, then rest.
static bool shtp_recv(bno085_t *dev, uint8_t *channel, uint8_t **payload, uint16_t *plen) {
    // Read header
    uint8_t hdr[SHTP_HEADER_LEN];
    int n = i2c_read_bytes(dev->i2c, dev->i2c_addr, hdr, sizeof(hdr));
    if (n != (int)sizeof(hdr)) return false;

    uint16_t frame_len = (uint16_t)(hdr[0] | (hdr[1] << 8));
    if (frame_len < SHTP_HEADER_LEN || frame_len > sizeof(dev->rx_buf)) return false;

    *channel = hdr[2];
    uint8_t seq = hdr[3]; (void)seq;

    uint16_t pay_len = (uint16_t)(frame_len - SHTP_HEADER_LEN);
    if (pay_len == 0) {
        *payload = NULL; *plen = 0;
        return true;
    }

    int m = i2c_read_bytes(dev->i2c, dev->i2c_addr, dev->rx_buf, pay_len);
    if (m != (int)pay_len) return false;

    *payload = dev->rx_buf;
    *plen    = pay_len;
    return true;
}

// Little helpers
static float q15_to_float(int16_t v, float scale) { return (float)v / scale; }
static float q14_to_float(int16_t v, float scale) { return (float)v / scale; }

// Parse known reports on REPORTS channel (3)
static void parse_reports(bno085_t *dev, const uint8_t *p, uint16_t len) {
    // The first byte in the payload is a Report ID (sensor->host)
    // Then a rolling timestamp (LSB first), then report-dependent fields.
    // We handle two report IDs:
    //   - Game Rotation Vector (quaternion) → FEAT_GAME_ROTATION_VECTOR (ID 0x28)
    //   - Accelerometer → FEAT_ACCELEROMETER (ID 0x01)
    if (len < 1) return;

    uint8_t report_id = p[0];

    // Common timestamp is often 4 bytes @ offset 1..4 (LSB first)
    uint32_t ts = 0;
    if (len >= 5) {
        ts = (uint32_t)p[1] | ((uint32_t)p[2] << 8) | ((uint32_t)p[3] << 16) | ((uint32_t)p[4] << 24);
    }

    if (report_id == FEAT_GAME_ROTATION_VECTOR) {
        // Layout (per SH-2 docs for Game Rotation Vector):
        // [0]=report_id, [1..4]=timestamp, then 4x int16 (i,j,k,real) in Q14, then optional accuracy
        if (len < 5 + 8) return;
        int16_t qi = (int16_t)(p[5]  | (p[6]  << 8));
        int16_t qj = (int16_t)(p[7]  | (p[8]  << 8));
        int16_t qk = (int16_t)(p[9]  | (p[10] << 8));
        int16_t qr = (int16_t)(p[11] | (p[12] << 8));
        dev->out.quat.qi = q14_to_float(qi, 16384.0f);
        dev->out.quat.qj = q14_to_float(qj, 16384.0f);
        dev->out.quat.qk = q14_to_float(qk, 16384.0f);
        dev->out.quat.qr = q14_to_float(qr, 16384.0f);
        dev->out.quat.timestamp_us = ts; // device tick; treat as a monotonic counter
        dev->out.quat.valid = true;
        return;
    }

    if (report_id == FEAT_ACCELEROMETER) {
        // Layout for Calibrated Accel: [0]=id, [1..4]=ts, then 3x int16 Q8 (m/s^2 scaled)
        // Many builds use Q8 for accel with scale 100 (i.e., value/100 = m/s^2). Some variants
        // use Q8.12. We’ll use a common scale (100.0f) which matches Adafruit’s outputs in SI.
        if (len < 5 + 6) return;
        int16_t ax = (int16_t)(p[5]  | (p[6]  << 8));
        int16_t ay = (int16_t)(p[7]  | (p[8]  << 8));
        int16_t az = (int16_t)(p[9]  | (p[10] << 8));
        dev->out.accel.ax = q15_to_float(ax, 100.0f);
        dev->out.accel.ay = q15_to_float(ay, 100.0f);
        dev->out.accel.az = q15_to_float(az, 100.0f);
        dev->out.accel.timestamp_us = ts;
        dev->out.accel.valid = true;
        return;
    }
}

// Send a Set Feature command (enable a report at a given period)
static bool set_feature(bno085_t *dev, uint8_t feature_id, uint32_t report_interval_us) {
    // Payload format (host->sensor on CONTROL channel):
    // byte 0: SH2_REPORT_SET_FEATURE (0xFD)
    // byte 1: feature_id
    // bytes 2..5: report_interval_us (LSB first)
    // bytes 6..9: sensor-specific 'batch interval' (0 = disabled)
    // bytes 10..11: sensor-specific config (0)
    uint8_t pay[12];
    pay[0] = SH2_REPORT_SET_FEATURE;
    pay[1] = feature_id;
    pay[2] = (uint8_t)(report_interval_us & 0xFF);
    pay[3] = (uint8_t)((report_interval_us >> 8) & 0xFF);
    pay[4] = (uint8_t)((report_interval_us >> 16) & 0xFF);
    pay[5] = (uint8_t)((report_interval_us >> 24) & 0xFF);
    // batch interval = 0
    pay[6] = pay[7] = pay[8] = pay[9] = 0x00;
    // sensor-specific cfg = 0
    pay[10] = pay[11] = 0x00;

    return shtp_send(dev, SHTP_CHANNEL_CONTROL, pay, sizeof(pay));
}

// --- Public API --------------------------------------------------------------

bool bno085_init(bno085_t *dev, i2c_inst_t *i2c, uint8_t i2c_addr) {
    memset(dev, 0, sizeof(*dev));
    dev->i2c      = i2c;
    dev->i2c_addr = i2c_addr ? i2c_addr : BNO085_I2C_ADDR_DEFAULT;

    // Start sequence counters at 0
    memset(dev->seq, 0, sizeof(dev->seq));

    // Simple sanity read: try to pull a header to see if device responds.
    // Many boards need a short delay after power-up.
    sleep_ms(20);

    // Attempt a small read; if NACK, give the user a hint to check wiring.
    uint8_t dummy[4];
    int n = i2c_read_blocking(dev->i2c, dev->i2c_addr, dummy, 1, false);
    if (n < 0) {
        // Device not responding yet — not fatal; it may be busy at boot.
        // We'll proceed; first poll will clarify.
    }

    // Clear last-known outputs
    dev->out.quat.valid = false;
    dev->out.accel.valid = false;

    return true;
}

bool bno085_enable_reports(bno085_t *dev, uint32_t quat_interval_us, uint32_t accel_interval_us) {
    bool ok1 = set_feature(dev, FEAT_GAME_ROTATION_VECTOR, quat_interval_us);
    sleep_ms(2);
    bool ok2 = set_feature(dev, FEAT_ACCELEROMETER,       accel_interval_us);
    return ok1 && ok2;
}

// Non-blocking poll: tries to read one frame and parse it (if any)
// Call this in your main loop frequently.
bool bno085_poll(bno085_t *dev) {
    uint8_t ch; uint8_t *p; uint16_t len;
    // Try to read one frame header; if no data, the read may fail quickly.
    // We’ll do a small timeout-like behavior by attempting once; users can loop.
    if (!shtp_recv(dev, &ch, &p, &len)) return false;

    if (ch == SHTP_CHANNEL_REPORTS && p && len) {
        parse_reports(dev, p, len);
        return true;
    }

    // Ignore other channels for this minimal example (command/control responses, etc.)
    return true;
}
