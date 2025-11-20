/**
 * @file adafruit_i2cdev.c
 * @brief Implementation of the C-based I2C device driver for the RP2350B.
 */

#include "adafruit_i2cdev.h"

void adafruit_i2cdev_init(adafruit_i2cdev_t *dev, i2c_inst_t *i2c, uint8_t address) {
    dev->i2c_inst = i2c;
    dev->addr = address;
}

bool adafruit_i2cdev_detected(adafruit_i2cdev_t *dev) {
    if (!dev || !dev->i2c_inst) {
        return false;
    }
    // A "ping" is performed by attempting to write 0 bytes.
    // The i2c_write_blocking function returns PICO_ERROR_GENERIC on a NACK.
    uint8_t dummy;
    int ret = i2c_read_blocking(dev->i2c_inst, dev->addr, &dummy, 1, false);
    return ret >= 0;
}

bool adafruit_i2cdev_read(adafruit_i2cdev_t *dev, uint8_t *buffer, size_t len, bool stop) {
    if (!dev || !dev->i2c_inst) {
        return false;
    }
    int result = i2c_read_blocking(dev->i2c_inst, dev->addr, buffer, len, !stop);
    return (result == (int)len);
}

bool adafruit_i2cdev_write(adafruit_i2cdev_t *dev, const uint8_t *buffer, size_t len, bool stop,
                           const uint8_t *prefix_buffer, size_t prefix_len) {
    if (!dev || !dev->i2c_inst) {
        return false;
    }

    // Write prefix first, if it exists.
    if (prefix_buffer && prefix_len > 0) {
        // If there's a main buffer to write, don't send a STOP after the prefix.
        // Otherwise, the 'stop' parameter determines the behavior.
        bool nostop = (buffer && len > 0) || !stop;
        int result = i2c_write_blocking(dev->i2c_inst, dev->addr, prefix_buffer, prefix_len, nostop);
        if (result != (int)prefix_len) {
            return false;
        }
    }

    // Write main buffer, if it exists.
    if (buffer && len > 0) {
        int result = i2c_write_blocking(dev->i2c_inst, dev->addr, buffer, len, !stop);
        if (result != (int)len) {
            return false;
        }
    }

    return true;
}

bool adafruit_i2cdev_write_then_read(adafruit_i2cdev_t *dev, const uint8_t *write_buffer, size_t write_len,
                                     uint8_t *read_buffer, size_t read_len) {
    if (!dev || !dev->i2c_inst) {
        return false;
    }

    // Write the register address or command, but keep control of the bus (nostop = true).
    int write_result = i2c_write_blocking(dev->i2c_inst, dev->addr, write_buffer, write_len, true);
    if (write_result != (int)write_len) {
        return false;
    }

    // Perform the read, releasing the bus at the end (nostop = false).
    int read_result = i2c_read_blocking(dev->i2c_inst, dev->addr, read_buffer, read_len, false);
    return (read_result == (int)read_len);
}

// Add this to the end of adafruit_i2cdev.c

bool adafruit_i2cdev_read_with_timeout(adafruit_i2cdev_t *dev, uint8_t *buffer, size_t len, bool stop, uint32_t timeout_us) {
    if (!dev || !dev->i2c_inst) {
        return false;
    }
    
    // The SDK provides a static inline helper for this
    int result = i2c_read_timeout_us(dev->i2c_inst, dev->addr, buffer, len, !stop, timeout_us);

    return (result == (int)len);
}