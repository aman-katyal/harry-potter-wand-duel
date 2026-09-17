/**
 * @file bno08x_driver.c
 * @brief C implementation of the BNO08x driver for RP2350B.
 * This version corrects the HAL I2C read logic to be a single transaction.
 */

#include "bno08x_driver.h"
#include "hardware/gpio.h"
#include <string.h>
#include <stdio.h>
#include "sh2_err.h"

// --- Forward declarations ---
static int      hal_open_rp2350b(sh2_Hal_t *self);
static void     hal_close_rp2350b(sh2_Hal_t *self);
static int      hal_read_rp2350b(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us);
static int      hal_write_rp2350b(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len);
static uint32_t hal_getTimeUs_rp2350b(sh2_Hal_t *self);
static void _hal_callback(void *cookie, sh2_AsyncEvent_t *pEvent);
static void _sensor_handler_callback(void *cookie, sh2_SensorEvent_t *pEvent);

static bno08x_driver_t* _current_driver_instance = NULL;

// -------------------------------------------------------------------------
// PUBLIC C API FUNCTIONS
// -------------------------------------------------------------------------

bool bno08x_begin_i2c(bno08x_driver_t *driver, i2c_inst_t *i2c_inst, uint8_t i2c_addr, int8_t reset_pin) {
    if (driver == NULL) return false;

    adafruit_i2cdev_init(&driver->i2c_dev, i2c_inst, i2c_addr);

    driver->reset_pin = reset_pin;
    if (driver->reset_pin != -1) {
        gpio_init(driver->reset_pin);
        gpio_set_dir(driver->reset_pin, GPIO_OUT);
    }

    driver->hal.open = hal_open_rp2350b;
    driver->hal.close = hal_close_rp2350b;
    driver->hal.read = hal_read_rp2350b;
    driver->hal.write = hal_write_rp2350b;
    driver->hal.getTimeUs = hal_getTimeUs_rp2350b;

    _current_driver_instance = driver;

    bno08x_hardware_reset(driver);
    sleep_ms(500);

    int status = sh2_open(&driver->hal, _hal_callback, driver);
    if (status != SH2_OK) {
        printf("ERROR: sh2_open() failed with status: %d.\n", status);
        return false;
    }

    memset(&driver->prod_ids, 0, sizeof(sh2_ProductIds_t));
    status = sh2_getProdIds(&driver->prod_ids);
    if (status != SH2_OK) {
        printf("ERROR: sh2_getProdIds() failed with status: %d.\n", status);
        return false;
    }

    sh2_setSensorCallback(_sensor_handler_callback, driver);
    return true;
}

void bno08x_hardware_reset(bno08x_driver_t *driver) {
    if (driver->reset_pin != -1) {
        gpio_put(driver->reset_pin, true); sleep_ms(10);
        gpio_put(driver->reset_pin, false); sleep_ms(10);
        gpio_put(driver->reset_pin, true); sleep_ms(10);
    }
}

// ... (Other public functions are unchanged) ...
bool bno08x_was_reset(bno08x_driver_t *driver) {
    bool state = driver->reset_occurred; driver->reset_occurred = false; return state;
}

bool bno08x_enable_report(bno08x_driver_t *driver, sh2_SensorId_t sensor_id, uint32_t interval_us) {
    sh2_SensorConfig_t config;
    config.changeSensitivityEnabled = false; config.wakeupEnabled = false; config.changeSensitivityRelative = false;
    config.alwaysOnEnabled = false; config.changeSensitivity = 0; config.batchInterval_us = 0;
    config.sensorSpecific = 0; config.reportInterval_us = interval_us;
    int status = sh2_setSensorConfig(sensor_id, &config);
    return (status == SH2_OK);
}

bool bno08x_get_sensor_event(bno08x_driver_t *driver, sh2_SensorValue_t *value) {
    driver->sensor_value_ptr = value; value->timestamp = 0; 
    sh2_service();
    if (value->timestamp == 0 && value->sensorId != SH2_GYRO_INTEGRATED_RV) {
        driver->sensor_value_ptr = NULL; return false;
    }
    driver->sensor_value_ptr = NULL; return true;
}

// -------------------------------------------------------------------------
// SH2 HAL IMPLEMENTATION
// -------------------------------------------------------------------------

static int hal_open_rp2350b(sh2_Hal_t *self) {
    uint8_t softreset_pkt[] = {5, 0, 1, 0, 1};
    bool success = false;
    for (int i = 0; i < 5; i++) {
        if (adafruit_i2cdev_write(&_current_driver_instance->i2c_dev, softreset_pkt, 5, true, NULL, 0)) {
            success = true; break;
        }
        sleep_ms(30);
    }
    if (!success) return -1;
    sleep_ms(300);
    return 0;
}

static void hal_close_rp2350b(sh2_Hal_t *self) { /* Nothing to do */ }

static int hal_read_rp2350b(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us) {
    // --- THIS IS THE CORRECTED LOGIC ---
    uint8_t header[4];
    
    // 1. Perform a single I2C read to get just the header.
    //    Use a timeout to avoid hanging if the sensor isn't ready.
    if (!adafruit_i2cdev_read_with_timeout(&_current_driver_instance->i2c_dev, header, 4, true, 100000)) {
        return 0; // No data or timeout
    }

    // 2. Decode the total packet length from the header.
    uint16_t packet_size = (uint16_t)header[0] | ((uint16_t)header[1] << 8);
    packet_size &= ~0x8000; // Clear the continuation bit

    if (packet_size == 0) return 0; // No cargo available
    if (packet_size > len) return 0; // Packet won't fit in the user's buffer

    // 3. Perform a SECOND, SEPARATE I2C read for the ENTIRE packet.
    //    This is how the BNO08x protocol over I2C works. Each read transaction
    //    re-starts the packet from the beginning. We read the header first
    //    only to know how many bytes to ask for in this second, full read.
    if (!adafruit_i2cdev_read_with_timeout(&_current_driver_instance->i2c_dev, pBuffer, packet_size, true, 100000)) {
        return 0; // Failed to read the full packet
    }
    
    // 4. Return the full packet size that we successfully read.
    return packet_size;
}

static int hal_write_rp2350b(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len) {
    if (adafruit_i2cdev_write(&_current_driver_instance->i2c_dev, pBuffer, len, true, NULL, 0)) {
        return len; 
    } else {
        return 0; 
    }
}

static uint32_t hal_getTimeUs_rp2350b(sh2_Hal_t *self) {
    return time_us_32();
}

// -------------------------------------------------------------------------
// SH2 CALLBACKS
// -------------------------------------------------------------------------

static void _hal_callback(void *cookie, sh2_AsyncEvent_t *pEvent) {
    bno08x_driver_t *driver = (bno08x_driver_t *)cookie;
    if (pEvent->eventId == SH2_RESET) {
        driver->reset_occurred = true;
    }
}

static void _sensor_handler_callback(void *cookie, sh2_SensorEvent_t *pEvent) {
    bno08x_driver_t *driver = (bno08x_driver_t *)cookie;
    if (driver->sensor_value_ptr != NULL) {
        sh2_decodeSensorEvent(driver->sensor_value_ptr, pEvent);
    }
}