#include "drv2605.h"
#include <stdio.h>

// Timeout for I2C transactions (10ms is plenty for 400kHz)
#define I2C_TIMEOUT_US 10000

// --- Helper Functions ---

// UPDATED: Uses timeout to prevent hanging on missing devices
static int write_register(drv2605_t *drv, uint8_t reg, uint8_t val) {
    uint8_t buffer[2] = {reg, val};
    // Returns number of bytes written or PICO_ERROR_TIMEOUT
    return i2c_write_timeout_us(drv->i2c_port, DRV2605_ADDR, buffer, 2, false, I2C_TIMEOUT_US);
}

// UPDATED: Uses timeout
static int read_register(drv2605_t *drv, uint8_t reg, uint8_t *val) {
    // 1. Write register address (nostop=true to hold bus)
    int ret = i2c_write_timeout_us(drv->i2c_port, DRV2605_ADDR, &reg, 1, true, I2C_TIMEOUT_US);
    if (ret < 0) return ret;

    // 2. Read value
    return i2c_read_timeout_us(drv->i2c_port, DRV2605_ADDR, val, 1, false, I2C_TIMEOUT_US);
}

// --- Implementation ---

bool drv2605_init(drv2605_t *drv, i2c_inst_t *i2c_port, uint sda_pin, uint scl_pin) {
    drv->i2c_port = i2c_port;

    // Initialize I2C
    i2c_init(i2c_port, 400 * 1000);
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);
    
    // --- CRITICAL FIX: Connectivity Check with TIMEOUT ---
    // Try to write the STATUS register address. 
    // If device is missing, this will return PICO_ERROR_TIMEOUT (negative) after 10ms.
    uint8_t reg = DRV2605_REG_STATUS;
    int ret = i2c_write_timeout_us(i2c_port, DRV2605_ADDR, &reg, 1, true, I2C_TIMEOUT_US);
    
    if (ret < 0) {
        return false; // Device not found, return immediately without hanging
    }

    // Check Device ID
    uint8_t id = 0;
    if (read_register(drv, DRV2605_REG_STATUS, &id) < 0) return false;
    
    // 0xE0 is the expected ID for DRV2605L
    if ((id & 0xE0) != 0xE0) {
        return false; 
    }

    // --- Chip Configuration ---
    // If any write fails (ret < 0), we technically could abort, 
    // but usually if ID check passed, these will work.
    
    write_register(drv, DRV2605_REG_MODE, 0x00);      // Internal Trigger
    write_register(drv, DRV2605_REG_RTPIN, 0x00);
    write_register(drv, DRV2605_REG_WAVESEQ1, 1);
    write_register(drv, DRV2605_REG_WAVESEQ2, 0);
    write_register(drv, DRV2605_REG_OVERDRIVE, 0);
    write_register(drv, DRV2605_REG_SUSTAINPOS, 0);
    write_register(drv, DRV2605_REG_SUSTAINNEG, 0);
    write_register(drv, DRV2605_REG_BREAK, 0);
    write_register(drv, DRV2605_REG_AUDIOMAX, 0x64);

    // LRA Setup
    uint8_t feedback = 0;
    read_register(drv, DRV2605_REG_FEEDBACK, &feedback);
    write_register(drv, DRV2605_REG_FEEDBACK, feedback & 0x7F); 
    
    uint8_t control3 = 0;
    read_register(drv, DRV2605_REG_CONTROL3, &control3);
    write_register(drv, DRV2605_REG_CONTROL3, control3 | 0x20); 

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

bool drv2605_is_playing(drv2605_t *drv) {
    uint8_t status = 0;
    if (read_register(drv, DRV2605_REG_GO, &status) < 0) return false;
    return (status & 0x01) != 0; 
}

void drv2605_play_cast_feedback(drv2605_t *drv) {
    drv2605_set_waveform(drv, 0, 14);  
    drv2605_set_waveform(drv, 1, 14);  
    drv2605_set_waveform(drv, 2, 14);  
    drv2605_set_waveform(drv, 3, 0);   
    drv2605_go(drv);
}

void drv2605_play_hit_feedback(drv2605_t *drv) {
    drv2605_set_waveform(drv, 0, 84);  
    drv2605_set_waveform(drv, 1, 47);  
    drv2605_set_waveform(drv, 2, 0);   
    drv2605_go(drv);
}