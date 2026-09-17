#ifndef SYS_CONFIG_H
#define SYS_CONFIG_H

#include "pico/stdlib.h"

// ==========================================
// --- PLAYER IDENTITY & DEBUG ---
// ==========================================
// Change this to 2 for the second wand!
#define PLAYER_ID           1   

// Set to 1 to allow shooting yourself (for testing/single wand)
// Set to 0 for actual duel (ignore your own IR signals)
#define ALLOW_SELF_HIT      1   

// ==========================================
// --- HARDWARE PINOUT ---
// ==========================================

// I2C0: IMU (BNO08x) & Haptics (Shared Bus)
#define I2C_IMU_PORT        i2c0
#define PIN_IMU_SDA         4
#define PIN_IMU_SCL         5

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

// Buttons
#define PIN_BTN_SHIELD      21

// ==========================================
// --- GAMEPLAY & AI TUNING ---
// ==========================================

// 1. CONFIDENCE
#define AI_CONFIDENCE_THRESHOLD  0.9f  

// 2. ANOMALY THRESHOLDS
#define ANOMALY_THRESH_AGUAMENTI  2.3f  
#define ANOMALY_THRESH_STUPEFY    2.6f  

// 3. TRIGGER LOGIC ("The Bucket")
#define SPELL_TRIGGER_TARGET     7      
#define SPELL_DECAY_RATE         2      

// 4. SHIELD MECHANICS
#define SHIELD_COOLDOWN_MS       10000   // 10 Seconds cooldown

// Timings
#define SENSOR_POLL_RATE_HZ      74     
#define SENSOR_POLL_US           (1000000 / SENSOR_POLL_RATE_HZ)
#define POST_CAST_LOCKOUT_MS     2000   
#define AI_CHECK_INTERVAL_MS     100    

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

// Spell IDs
#define SPELL_LOGIC_NONE        0
#define SPELL_LOGIC_AGUAMENTI   1
#define SPELL_LOGIC_STUPEFY     2

#endif // SYS_CONFIG_H