#ifndef BNO08X_H
#define BNO08X_H

#include <stdbool.h>
#include <stdint.h>
#include "hardware/spi.h"
#include "sh2.h"
#include "sh2_SensorValue.h"

// Define the pins for your RP2350 connection
typedef struct {
    spi_inst_t *spi_port;
    uint sck_pin;
    uint mosi_pin;
    uint miso_pin;
    
    uint cs_pin;
    uint int_pin;
    int8_t rst_pin;
} bno08x_pins_t;

typedef struct {
    bno08x_pins_t pins;
    sh2_Hal_t hal;
    sh2_ProductIds_t prodIds;
    bool reset_occurred;
} bno08x_t;

// Public Functions
bool bno08x_begin_spi(bno08x_t *self, bno08x_pins_t pins_config);
bool bno08x_enable_report(sh2_SensorId_t sensorId, uint32_t interval_us);
bool bno08x_get_sensor_event(sh2_SensorValue_t *value);
bool bno08x_was_reset(bno08x_t *self);
void bno08x_hardware_reset(bno08x_t *self);

#endif // BNO08X_H