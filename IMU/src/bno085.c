#include "bno085.h"
#include <string.h>
#include <stdio.h>

// --- Internal Function Prototypes ---
static void parse_reports(bno085_t *dev, const uint8_t *p, uint16_t len);
static bool shtp_send(bno085_t *dev, uint8_t channel, const uint8_t *payload, uint16_t plen);
static bool set_feature(bno085_t *dev, uint8_t feature_id, uint32_t report_interval_us);

// --- Public API ---

bool bno085_init(bno085_t *dev, i2c_inst_t *i2c, int8_t rst_pin) {
    memset(dev, 0, sizeof(*dev));
    dev->i2c = i2c;
    dev->i2c_addr = BNO085_I2C_ADDR_DEFAULT;
    dev->rst_pin = rst_pin;

    if (dev->rst_pin != -1) {
        gpio_init(dev->rst_pin);
        gpio_set_dir(dev->rst_pin, GPIO_OUT);
        gpio_put(dev->rst_pin, 0);
        sleep_ms(10);
        gpio_put(dev->rst_pin, 1);
        sleep_ms(50);
    } else {
        sleep_ms(100);
    }

    uint8_t dummy;
    int result = i2c_read_blocking(dev->i2c, dev->i2c_addr, &dummy, 1, false);
    return (result >= 0);
}

bool bno085_enable_reports(bno085_t *dev, uint32_t quat_interval_us, uint32_t accel_interval_us) {
    bool ok1 = set_feature(dev, FEAT_GAME_ROTATION_VECTOR, quat_interval_us);
    sleep_ms(5);
    bool ok2 = set_feature(dev, FEAT_ACCELEROMETER, accel_interval_us);
    return ok1 && ok2;
}

bno085_outputs_t bno085_get_outputs(bno085_t *dev) {
    return dev->out;
}

// --- Internal Implementation ---

static float q_to_float(int16_t fixed_point_value, uint8_t q_point) {
    float q_point_divisor = (1 << q_point);
    return (float)fixed_point_value / q_point_divisor;
}

static void parse_reports(bno085_t *dev, const uint8_t *p, uint16_t len) {
    if (len < 1) return;
    uint8_t report_id = p[0];
    uint32_t ts = 0;
    if (len >= 5) {
        ts = (uint32_t)p[1] | ((uint32_t)p[2] << 8) | ((uint32_t)p[3] << 16) | ((uint32_t)p[4] << 24);
    }

    if (report_id == FEAT_GAME_ROTATION_VECTOR) {
        if (len < 13) return;
        int16_t qi = (int16_t)(p[5] | (p[6] << 8));
        int16_t qj = (int16_t)(p[7] | (p[8] << 8));
        int16_t qk = (int16_t)(p[9] | (p[10] << 8));
        int16_t qr = (int16_t)(p[11] | (p[12] << 8));
        dev->out.quat.qi = q_to_float(qi, 14);
        dev->out.quat.qj = q_to_float(qj, 14);
        dev->out.quat.qk = q_to_float(qk, 14);
        dev->out.quat.qr = q_to_float(qr, 14);
        dev->out.quat.timestamp_us = ts;
        dev->out.quat.valid = true;
    } else if (report_id == FEAT_ACCELEROMETER) {
        if (len < 11) return;
        int16_t ax = (int16_t)(p[5] | (p[6] << 8));
        int16_t ay = (int16_t)(p[7] | (p[8] << 8));
        int16_t az = (int16_t)(p[9] | (p[10] << 8));
        dev->out.accel.ax = q_to_float(ax, 8);
        dev->out.accel.ay = q_to_float(ay, 8);
        dev->out.accel.az = q_to_float(az, 8);
        dev->out.accel.timestamp_us = ts;
        dev->out.accel.valid = true;
    }
}

static bool set_feature(bno085_t *dev, uint8_t feature_id, uint32_t report_interval_us) {
    uint8_t payload[17] = {0};
    payload[0] = SH2_REPORT_SET_FEATURE;
    payload[1] = feature_id;
    memcpy(&payload[5], &report_interval_us, 4);
    return shtp_send(dev, SHTP_CHANNEL_CONTROL, payload, sizeof(payload));
}

static bool shtp_send(bno085_t *dev, uint8_t channel, const uint8_t *payload, uint16_t plen) {
    uint16_t frame_len = plen + SHTP_HEADER_LEN;
    if (frame_len > sizeof(dev->tx_buf)) return false;
    uint8_t *buf = dev->tx_buf;
    buf[0] = (uint8_t)(frame_len & 0xFF);
    buf[1] = (uint8_t)(frame_len >> 8);
    buf[2] = channel;
    buf[3] = dev->seq[channel]++;
    memcpy(&buf[4], payload, plen);
    return (i2c_write_blocking(dev->i2c, dev->i2c_addr, buf, frame_len, false) == frame_len);
}

bool bno085_poll(bno085_t *dev) {
    uint8_t hdr[SHTP_HEADER_LEN];
    if (i2c_read_blocking(dev->i2c, dev->i2c_addr, hdr, SHTP_HEADER_LEN, false) != SHTP_HEADER_LEN) return false;

    uint16_t frame_len = (uint16_t)(hdr[0] | (hdr[1] << 8));
    frame_len &= ~0x8000;

    if (frame_len == 0) return false;
    if (frame_len > sizeof(dev->rx_buf)) {
        printf("BNO085 Error: Packet too large (%d bytes)\n", frame_len);
        // We must still read the packet to clear the sensor's buffer
        uint8_t dummy_buf[frame_len];
        i2c_read_blocking(dev->i2c, dev->i2c_addr, dummy_buf, frame_len, false);
        return false;
    }

    if (i2c_read_blocking(dev->i2c, dev->i2c_addr, dev->rx_buf, frame_len, false) != frame_len) return false;

    uint8_t channel = dev->rx_buf[2];
    uint16_t payload_len = (frame_len > SHTP_HEADER_LEN) ? (frame_len - SHTP_HEADER_LEN) : 0;
    
    if (channel == SHTP_CHANNEL_REPORTS && payload_len > 0) {
        parse_reports(dev, &dev->rx_buf[SHTP_HEADER_LEN], payload_len);
    }
    return true;
}