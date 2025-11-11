#ifndef DRV2605_H
#define DRV2605_H

#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// Device I2C address
#define DRV2605_ADDR 0x5A

// Register addresses
#define DRV2605_REG_STATUS 0x00
#define DRV2605_REG_MODE 0x01
#define DRV2605_MODE_INTTRIG 0x00
#define DRV2605_MODE_REALTIME 0x05
#define DRV2605_REG_RTPIN 0x02
#define DRV2605_REG_LIBRARY 0x03
#define DRV2605_REG_WAVESEQ1 0x04
#define DRV2605_REG_WAVESEQ2 0x05
#define DRV2605_REG_GO 0x0C
#define DRV2605_REG_OVERDRIVE 0x0D  
#define DRV2605_REG_SUSTAINPOS 0x0E
#define DRV2605_REG_SUSTAINNEG 0x0F
#define DRV2605_REG_BREAK 0x10
#define DRV2605_REG_AUDIOMAX 0x13
#define DRV2605_REG_FEEDBACK 0x1A
#define DRV2605_REG_CONTROL3 0x1D

// C-style object representing a DRV2605 device.
typedef struct {
    i2c_inst_t *i2c_port;
} drv2605_t;

// Function Prototypes
bool drv2605_init(drv2605_t *drv, i2c_inst_t *i2c_port, uint sda_pin, uint scl_pin);
void drv2605_set_waveform(drv2605_t *drv, uint8_t slot, uint8_t w);
void drv2605_select_library(drv2605_t *drv, uint8_t lib);
void drv2605_go(drv2605_t *drv);
void drv2605_set_mode(drv2605_t *drv, uint8_t mode);
void drv2605_set_realtime_value(drv2605_t *drv, uint8_t rtp);

// Non-blocking status check
bool drv2605_is_playing(drv2605_t *drv);

// Quick haptic presets for wand actions
void drv2605_play_cast_feedback(drv2605_t *drv);    // Light buzz for casting
void drv2605_play_hit_feedback(drv2605_t *drv);     // Strong pulse for being hit

#endif // DRV2605_H