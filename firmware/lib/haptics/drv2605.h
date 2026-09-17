#ifndef DRV2605_H
#define DRV2605_H

#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// The default I2C address for the DRV2605L motor driver
#define DRV2605_ADDR 0x5A

// --- Register Definitions ---
// These are the memory locations inside the chip where we control settings.
// See the TI DRV2605 datasheet for deep details on each bit.
#define DRV2605_REG_STATUS      0x00
#define DRV2605_REG_MODE        0x01
#define DRV2605_MODE_INTTRIG    0x00 // Internal Trigger: plays waveforms from ROM
#define DRV2605_MODE_REALTIME   0x05 // Real-time Playback: you directly control vibration strength
#define DRV2605_REG_RTPIN       0x02
#define DRV2605_REG_LIBRARY     0x03
#define DRV2605_REG_WAVESEQ1    0x04 // The start of the waveform sequence queue
#define DRV2605_REG_WAVESEQ2    0x05
#define DRV2605_REG_GO          0x0C
#define DRV2605_REG_OVERDRIVE   0x0D  
#define DRV2605_REG_SUSTAINPOS  0x0E
#define DRV2605_REG_SUSTAINNEG  0x0F
#define DRV2605_REG_BREAK       0x10
#define DRV2605_REG_AUDIOMAX    0x13
#define DRV2605_REG_FEEDBACK    0x1A
#define DRV2605_REG_CONTROL3    0x1D

// Object representing a specific DRV2605 device.
// We store the i2c_port here so we can have multiple devices on different buses if needed.
typedef struct {
    i2c_inst_t *i2c_port;
} drv2605_t;

// --- Core Functions ---

// Starts the I2C bus and configuring the chip settings.
// Returns true if the device was found and responded correctly.
bool drv2605_init(drv2605_t *drv, i2c_inst_t *i2c_port, uint sda_pin, uint scl_pin);

// Queues up a specific effect ID into a slot (1-8).
// slot: 0 to 7 (the chip processes these in order).
// w: The effect ID (1-123) from the TI ROM library.
void drv2605_set_waveform(drv2605_t *drv, uint8_t slot, uint8_t w);

// Selects which library of effects to use.
// 1-5 are for LRA motors, 6 is for ERM motors.
void drv2605_select_library(drv2605_t *drv, uint8_t lib);

// "Fire!" - tells the chip to process the waveform queue immediately.
void drv2605_go(drv2605_t *drv);

// Switches between internal triggers (ROM presets) and real-time control.
void drv2605_set_mode(drv2605_t *drv, uint8_t mode);

// Directly sets vibration intensity (0-255) when in Real-Time Playback mode.
void drv2605_set_realtime_value(drv2605_t *drv, uint8_t rtp);

// Checks if the motor is currently vibrating.
// Useful so you don't interrupt an effect that is already happening.
bool drv2605_is_playing(drv2605_t *drv);

// --- Game Logic Presets ---

// Plays a "magical" triple-click sensation.
void drv2605_play_cast_feedback(drv2605_t *drv);    

// Plays a heavy buzz/thud sensation indicating damage.
void drv2605_play_hit_feedback(drv2605_t *drv);     

#endif // DRV2605_H