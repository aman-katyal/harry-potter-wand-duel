/**
 * @file bno08x_driver.h
 * @brief C driver for the BNO08x 9-DOF IMU for the RP2350B SDK.
 *
 * This driver is a C conversion of the Adafruit BNO08x Arduino library.
 * It uses the platform-agnostic SH2/SHTP library from Hillcrest Labs
 * and provides the necessary Hardware Abstraction Layer (HAL) for the
 * RP2350B platform.
 */

#ifndef BNO08X_DRIVER_H
#define BNO08X_DRIVER_H
#define BNO08x_I2CADDR_DEFAULT 0x4A
#include "pico/stdlib.h"
#include "adafruit_i2cdev.h" // Our I2C device library
#include "sh2.h"             // Hillcrest SH2 main header
#include "sh2_SensorValue.h" // Hillcrest sensor value decoding

/**
 * @brief Main driver structure for the BNO08x sensor.
 * This holds all the state for a single BNO08x instance.
 */
typedef struct {
    adafruit_i2cdev_t i2c_dev;    // The underlying I2C device
    int8_t            reset_pin;  // The GPIO pin number for hardware reset

    sh2_Hal_t         hal;        // The SH2 HAL instance
    sh2_ProductIds_t  prod_ids;   // To store product IDs from the device

    // Internal state, managed by the driver
    volatile bool     reset_occurred; // Flag set by the HAL callback on reset
    sh2_SensorValue_t *sensor_value_ptr; // Pointer to user's struct for sensor data
} bno08x_driver_t;


/**
 * @brief Initializes the BNO08x driver over I2C.
 *
 * @param driver Pointer to the driver instance structure to initialize.
 * @param i2c_inst The I2C peripheral to use (e.g., i2c0).
 * @param i2c_addr The 7-bit I2C address of the BNO08x.
 * @param reset_pin The GPIO pin connected to the BNO08x RST pin.
 * @return true on successful initialization, false on failure.
 */
bool bno08x_begin_i2c(bno08x_driver_t *driver, i2c_inst_t *i2c_inst, uint8_t i2c_addr, int8_t reset_pin);

/**
 * @brief Performs a hardware reset of the BNO08x using the reset pin.
 *
 * @param driver Pointer to the initialized driver instance.
 */
void bno08x_hardware_reset(bno08x_driver_t *driver);

/**
 * @brief Checks if a hardware reset has occurred since the last check.
 * This flag is set by the SH2 event callback.
 *
 * @param driver Pointer to the initialized driver instance.
 * @return true if a reset was detected, false otherwise.
 */
bool bno08x_was_reset(bno08x_driver_t *driver);

/**
 * @brief Enables a specific sensor report from the BNO08x.
 *
 * @param driver Pointer to the initialized driver instance.
 * @param sensor_id The ID of the sensor report to enable (from sh2.h).
 * @param interval_us The desired interval between reports, in microseconds.
 * @return true on success, false on failure.
 */
bool bno08x_enable_report(bno08x_driver_t *driver, sh2_SensorId_t sensor_id, uint32_t interval_us);

/**
 * @brief Polls for and retrieves the latest sensor event.
 *
 * This function services the underlying SH2 protocol. If a new sensor report is
 * available, it is decoded and placed into the `value` structure.
 *
 * @param driver Pointer to the initialized driver instance.
 * @param value Pointer to a `sh2_SensorValue_t` structure to be filled.
 * @return true if a new event was successfully retrieved, false if no new event was available.
 */
bool bno08x_get_sensor_event(bno08x_driver_t *driver, sh2_SensorValue_t *value);

#endif // BNO08X_DRIVER_H