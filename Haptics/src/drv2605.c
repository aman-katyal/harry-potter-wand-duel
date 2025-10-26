#include "drv2605.h"

// Internal helper functions
static void write_register(drv2605_t *drv, uint8_t reg, uint8_t val) {
    uint8_t buffer[2] = {reg, val};
    i2c_write_blocking(drv->i2c_port, DRV2605_ADDR, buffer, 2, false);
}

static uint8_t read_register(drv2605_t *drv, uint8_t reg) {
    uint8_t val;
    i2c_write_blocking(drv->i2c_port, DRV2605_ADDR, &reg, 1, true);
    i2c_read_blocking(drv->i2c_port, DRV2605_ADDR, &val, 1, false);
    return val;
}

// Public functions
bool drv2605_init(drv2605_t *drv, i2c_inst_t *i2c_port, uint sda_pin, uint scl_pin) {
    drv->i2c_port = i2c_port;

    // Initialize I2C at 100kHz
    i2c_init(i2c_port, 100 * 1000);
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);

    // Check device status
    uint8_t id = read_register(drv, DRV2605_REG_STATUS);
    if ((id & 0xE0) != 0xE0) { // Top 3 bits should be 111 for DRV2605
        return false; // Device not found
    }

    write_register(drv, DRV2605_REG_MODE, 0x00); // Out of standby
    write_register(drv, DRV2605_REG_RTPIN, 0x00); // No real-time-playback
    write_register(drv, DRV2605_REG_WAVESEQ1, 1); // A strong click
    write_register(drv, DRV2605_REG_WAVESEQ2, 0); // End of sequence
    write_register(drv, DRV2605_REG_OVERDRIVE, 0);
    write_register(drv, DRV2605_REG_SUSTAINPOS, 0);
    write_register(drv, DRV2605_REG_SUSTAINNEG, 0);
    write_register(drv, DRV2605_REG_BREAK, 0);
    write_register(drv, DRV2605_REG_AUDIOMAX, 0x64);

    // Set ERM open loop mode
    uint8_t feedback = read_register(drv, DRV2605_REG_FEEDBACK);
    write_register(drv, DRV2605_REG_FEEDBACK, feedback & 0x7F); // Clear LRA bit
    uint8_t control3 = read_register(drv, DRV2605_REG_CONTROL3);
    write_register(drv, DRV2605_REG_CONTROL3, control3 | 0x20); // Set ERM_OPEN_LOOP

    return true;
}

void drv2605_set_waveform(drv2605_t *drv, uint8_t slot, uint8_t w) {
    if (slot > 7) return;
    write_register(drv, DRV2605_REG_WAVESEQ1 + slot, w);
}

void drv2605_select_library(drv2605_t *drv, uint8_t lib) {
    write_register(drv, DRV2605_REG_LIBRARY, lib);
}

void drv2605_go(drv2605_t *drv) {
    write_register(drv, DRV2605_REG_GO, 1);
}

void drv2605_set_mode(drv2605_t *drv, uint8_t mode) {
    write_register(drv, DRV2605_REG_MODE, mode);
}

void drv2605_set_realtime_value(drv2605_t *drv, uint8_t rtp) {
    write_register(drv, DRV2605_REG_RTPIN, rtp);
}