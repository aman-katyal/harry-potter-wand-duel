#ifndef SYS_CONFIG_H
#define SYS_CONFIG_H

#include "pico/stdlib.h"

// ==========================================
// --- HARDWARE PINOUT ---
// ==========================================

// I2C0: IMU (BNO08x)
#define I2C_IMU_PORT        i2c0
#define PIN_IMU_SDA         4
#define PIN_IMU_SCL         5

// I2C1: Haptics (DRV2605)
#define I2C_HAPTIC_PORT     i2c0
#define PIN_HAPTIC_SDA      4
#define PIN_HAPTIC_SCL      5

// LED Matrix (WS2812)
#define PIN_LED             20  
#define NUM_LEDS            256
#define LED_DMA_CHANNEL     0
#define MATRIX_WIDTH        16
#define MATRIX_HEIGHT       16

// IR Comms
#define PIN_IR_TX           16
#define PIN_IR_RX           6

// ==========================================
// --- GAMEPLAY & AI TUNING ---
// ==========================================

// 1. CONFIDENCE (How sure is the AI?)
// Keep this high (70-80%) to stop "Unknown" noise from winning.
#define AI_CONFIDENCE_THRESHOLD  0.8f  

// 2. ANOMALY THRESHOLDS (How "weird" is the motion?)
// Lower = Stricter (Must look exactly like training data)
// Higher = Looser (Allows sloppy casting)
#define ANOMALY_THRESH_AGUAMENTI  2.8f  // Tune this for Spell 2
#define ANOMALY_THRESH_STUPEFY    3.0f  // Tune this for Spell 1

// 3. TRIGGER LOGIC ("The Bucket")
// How many valid frames in a row to fire?
#define SPELL_TRIGGER_TARGET     6      // Increased to 5 for stability
#define SPELL_DECAY_RATE         2      // Penalty for bad frames

// Timings
#define SENSOR_POLL_RATE_HZ      74     
#define SENSOR_POLL_US           (1000000 / SENSOR_POLL_RATE_HZ)
#define POST_CAST_LOCKOUT_MS     2000   
#define AI_CHECK_INTERVAL_MS     100    // Faster checks to fill bucket

// ==========================================
// --- INTER-CORE COMMUNICATION ---
// ==========================================
typedef union {
    struct {
        uint8_t reserved;
        uint8_t anomaly;    // Scaled 0-100 
        uint8_t confidence; // Scaled 0-100
        uint8_t spell_id;   // 0-255
    } parts;
    uint32_t packed;
} ai_result_t;

// Spell IDs (Internal Logic)
#define SPELL_LOGIC_NONE        0
#define SPELL_LOGIC_AGUAMENTI   1
#define SPELL_LOGIC_STUPEFY     2

#endif // SYS_CONFIG_H