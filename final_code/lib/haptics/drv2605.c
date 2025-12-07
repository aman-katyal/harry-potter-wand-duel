#include "drv2605.h"

// --- Helper Functions ---

// A wrapper to send a single byte to a register.
// Simplifies the I2C calls in the main logic.
static void write_register(drv2605_t *drv, uint8_t reg, uint8_t val) {
    uint8_t buffer[2] = {reg, val};
    i2c_write_blocking(drv->i2c_port, DRV2605_ADDR, buffer, 2, false);
}

// A wrapper to read a single byte.
// 1. Writes the register address we want to read.
// 2. Reads the data back from that address.
static uint8_t read_register(drv2605_t *drv, uint8_t reg) {
    uint8_t val;
    // 'true' keeps master control of bus (repeated start) to prevent another device grabbing it
    i2c_write_blocking(drv->i2c_port, DRV2605_ADDR, &reg, 1, true);
    i2c_read_blocking(drv->i2c_port, DRV2605_ADDR, &val, 1, false);
    return val;
}

// --- Implementation ---

bool drv2605_init(drv2605_t *drv, i2c_inst_t *i2c_port, uint sda_pin, uint scl_pin) {
    drv->i2c_port = i2c_port;

    // Initialize I2C at 400kHz.
    // 400kHz is preferred over 100kHz for haptics to ensure low latency.
    i2c_init(i2c_port, 400 * 1000);
    
    // Configure the GPIO pins for I2C use
    gpio_set_function(sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin, GPIO_FUNC_I2C);
    
    // Pull-ups are needed for I2C lines to return to logic high when idle
    gpio_pull_up(sda_pin);
    gpio_pull_up(scl_pin);
    
    // Sanity Check: Read the status register.
    // The top 3 bits (Device ID) usually return 111 (0xE0) on a valid chip.
    // If this fails, the wiring is likely wrong or the chip is dead.
    uint8_t id = read_register(drv, DRV2605_REG_STATUS);
    if ((id & 0xE0) != 0xE0) {
        return false; 
    }

    // --- Chip Configuration ---
    
    // 1. Take device out of standby (Mode 0x00 is Internal Trigger)
    write_register(drv, DRV2605_REG_MODE, 0x00);
    
    // 2. Clear real-time playback input
    write_register(drv, DRV2605_REG_RTPIN, 0x00);
    
    // 3. Set a default "do nothing" state for the wave sequencer
    write_register(drv, DRV2605_REG_WAVESEQ1, 1);
    write_register(drv, DRV2605_REG_WAVESEQ2, 0);
    
    // 4. Configure overdrive and sustain. 
    // These values control how "crisp" the motor start/stop feels.
    // 0 is usually a safe default for autocalibration.
    write_register(drv, DRV2605_REG_OVERDRIVE, 0);
    write_register(drv, DRV2605_REG_SUSTAINPOS, 0);
    write_register(drv, DRV2605_REG_SUSTAINNEG, 0);
    write_register(drv, DRV2605_REG_BREAK, 0);
    write_register(drv, DRV2605_REG_AUDIOMAX, 0x64);

    // 5. Configure Feedback and Control registers.
    // This logic specifically sets up the driver for LRA (Linear Resonant Actuator) motors.
    // It enables analog input and optimized looping gain.
    uint8_t feedback = read_register(drv, DRV2605_REG_FEEDBACK);
    write_register(drv, DRV2605_REG_FEEDBACK, feedback & 0x7F); // Masking magic per datasheet
    
    uint8_t control3 = read_register(drv, DRV2605_REG_CONTROL3);
    write_register(drv, DRV2605_REG_CONTROL3, control3 | 0x20); // Enable analog open loop

    return true;
}

void drv2605_set_waveform(drv2605_t *drv, uint8_t slot, uint8_t w) {
    // There are only 8 slots (0-7) in the hardware queue
    if (slot > 7) return;
    
    // Offset from the first sequencer register to the desired slot
    write_register(drv, DRV2605_REG_WAVESEQ1 + slot, w);
}

void drv2605_select_library(drv2605_t *drv, uint8_t lib) {
    // Selects the ROM library.
    // Lib 1-5: LRA Motors (Most common for haptic clickers)
    // Lib 6: ERM Motors (Spinning eccentric weights)
    // Lib 7: LRA specific
    write_register(drv, DRV2605_REG_LIBRARY, lib);
}

void drv2605_go(drv2605_t *drv) {
    // Writing '1' to the GO register triggers the sequence we just programmed.
    write_register(drv, DRV2605_REG_GO, 1);
}

void drv2605_set_mode(drv2605_t *drv, uint8_t mode) {
    write_register(drv, DRV2605_REG_MODE, mode);
}

void drv2605_set_realtime_value(drv2605_t *drv, uint8_t rtp) {
    // In Real-Time mode, this register directly controls voltage amplitude.
    write_register(drv, DRV2605_REG_RTPIN, rtp);
}

bool drv2605_is_playing(drv2605_t *drv) {
    // Read the GO register. 
    // Bit 0 stays High (1) while the sequencer is running and drops to 0 when finished.
    uint8_t status = read_register(drv, DRV2605_REG_GO);
    return (status & 0x01) != 0; 
}

// --- Wand Haptic Effects ---

void drv2605_play_cast_feedback(drv2605_t *drv) {
    // Sequence: Three sharp clicks followed by silence.
    // 14 = "Strong Click - 100%"
    // 0 = End of sequence (Stop)
    drv2605_set_waveform(drv, 0, 14);  
    drv2605_set_waveform(drv, 1, 14);  
    drv2605_set_waveform(drv, 2, 14);  
    drv2605_set_waveform(drv, 3, 0);   
    drv2605_go(drv);
}

void drv2605_play_hit_feedback(drv2605_t *drv) {
    // Sequence: A long ramp up buzz, followed by a heavy pulse.
    // 84 = "Ramp Up Long"
    // 47 = "Pulsing Strong 1"
    drv2605_set_waveform(drv, 0, 84);  
    drv2605_set_waveform(drv, 1, 47);  
    drv2605_set_waveform(drv, 2, 0);   
    drv2605_go(drv);
}