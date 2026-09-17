/**
 * @file adafruit_i2cdev.h
 * @brief C-based I2C device driver for the RP2350B SDK.
 *
 * This is a C translation of the Adafruit_I2CDevice class from the
 * Adafruit_BusIO library, designed to use the RP2350B's hardware I2C API.
 */

#ifndef _ADAFRUIT_I2CDEV_H
#define _ADAFRUIT_I2CDEV_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include <stdbool.h>

/**
 * @brief Structure to hold the state of an I2C device instance.
 * Replaces the Adafruit_I2CDevice C++ class.
 */
typedef struct {
    i2c_inst_t *i2c_inst; ///< The RP2350B SDK I2C peripheral instance (e.g., i2c0).
    uint8_t    addr;      ///< The 7-bit I2C address of the device.
} adafruit_i2cdev_t;


/**
 * @brief Initializes an I2C device structure.
 *
 * @param dev Pointer to the device structure to initialize.
 * @param i2c The I2C peripheral instance to use (e.g., i2c0).
 * @param address The 7-bit I2C address of the device.
 */
void adafruit_i2cdev_init(adafruit_i2cdev_t *dev, i2c_inst_t *i2c, uint8_t address);

/**
 * @brief Checks if a device is present on the I2C bus.
 *
 * @param dev Pointer to the initialized I2C device.
 * @return true if the device acknowledged its address, false otherwise.
 */
bool adafruit_i2cdev_detected(adafruit_i2cdev_t *dev);

/**
 * @brief Reads a sequence of bytes from the I2C device.
 *
 * @param dev Pointer to the initialized I2C device.
 * @param buffer Pointer to the buffer where data will be stored.
 * @param len The number of bytes to read.
 * @param stop If true, send a STOP condition after the read.
 * @return true on success, false on failure.
 */
bool adafruit_i2cdev_read(adafruit_i2cdev_t *dev, uint8_t *buffer, size_t len, bool stop);

/**
 * @brief Writes a sequence of bytes to the I2C device.
 *
 * @param dev Pointer to the initialized I2C device.
 * @param buffer Pointer to the data to write.
 * @param len The number of bytes to write.
 * @param stop If true, send a STOP condition after the write.
 * @param prefix_buffer Optional buffer to write before the main buffer (e.g., a register address).
 * @param prefix_len The length of the prefix buffer.
 * @return true on success, false on failure.
 */
bool adafruit_i2cdev_write(adafruit_i2cdev_t *dev, const uint8_t *buffer, size_t len, bool stop,
                           const uint8_t *prefix_buffer, size_t prefix_len);

/**
 * @brief Writes data and then immediately reads data from the device.
 * This is typically used to read from a specific register.
 *
 * @param dev Pointer to the initialized I2C device.
 * @param write_buffer Pointer to the data to write (e.g., register address).
 * @param write_len The number of bytes to write.
 * @param read_buffer Pointer to the buffer where read data will be stored.
 * @param read_len The number of bytes to read.
 * @return true on success, false on failure.
 */
bool adafruit_i2cdev_write_then_read(adafruit_i2cdev_t *dev, const uint8_t *write_buffer, size_t write_len,
                                     uint8_t *read_buffer, size_t read_len);

// Add this declaration to the end of adafruit_i2cdev.h, before the #endif

/**
 * @brief Reads a sequence of bytes from the I2C device with a timeout.
 *
 * @param dev Pointer to the initialized I2C device.
 * @param buffer Pointer to the buffer where data will be stored.
 * @param len The number of bytes to read.
 * @param stop If true, send a STOP condition after the read.
 * @param timeout_us The timeout for the read operation in microseconds.
 * @return true on success, false on failure or timeout.
 */
bool adafruit_i2cdev_read_with_timeout(adafruit_i2cdev_t *dev, uint8_t *buffer, size_t len, bool stop, uint32_t timeout_us);

#endif // _ADAFRUIT_I2CDEV_H