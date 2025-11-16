/**
 * Integrated Wand Duel Main File
 * Combines: IR Recv/Send, Haptics, LED Matrix Animations, and Game Logic
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

// --- Library Includes ---
#include "ir_emitter.h"
#include "irremote.h"        // Receiver logic
#include "drv2605.h"         // Haptics
#include "ws2812.h"          // LED Driver
#include "healthbar.h"       // Health system

// --- Animation Includes ---
#include "animation_params.h" // Default structs (FIREWORK_DEFAULT_BLUE, etc.)
#include "firework.h"
#include "spiral.h"
#include "circle_explosion.h"

// --- Hardware Pin Definitions ---
#define TX_PIN      36  // IR Transmitter
#define RX_PIN      15  // IR Receiver
#define I2C_SDA     16  // Haptic SDA
#define I2C_SCL     17  // Haptic SCL
#define LED_PIN     18  // (Defined in ws2812.h, but noted here for clarity)

// --- Configuration ---
#define LED_WIDTH   16
#define LED_HEIGHT  16

// Set to true to auto-cast spells for testing
#define TEST_CAST_SPELL     true 
#define TEST_SPELL_ID       2    // Which spell to auto-cast (1, 2, or 3)

// --- Globals ---
static drv2605_t haptic; // Haptic driver instance

// Packet validation buffer
#define PACKET_BUFFER_SIZE 3
static uint8_t packet_buffer[PACKET_BUFFER_SIZE] = {0};
static int packet_index = 0;


// --- Helper Functions ---

/**
 * Validates spells by requiring redundancy. 
 * Returns true if 'spell_num' appears at least twice in the buffer.
 */
bool validate_spell_input(uint8_t spell_num) {
    int match_count = 0;
    for (int i = 0; i < PACKET_BUFFER_SIZE; i++) {
        if (packet_buffer[i] == spell_num) {
            match_count++;
        }
    }
    return (match_count >= 2);
}

void add_packet_to_buffer(uint8_t spell_num) {
    packet_buffer[packet_index] = spell_num;
    packet_index = (packet_index + 1) % PACKET_BUFFER_SIZE;
}

void clear_packet_buffer() {
    for (int i = 0; i < PACKET_BUFFER_SIZE; i++) {
        packet_buffer[i] = 0;
    }
    packet_index = 0;
}

/**
 * Handles the visual and logical reaction to a validated spell.
 */
void process_spell_effect(uint8_t spell_num) {
    int damage = 0;
    
    // 1. Determine Damage and Log
    switch(spell_num) {
        case 1:
            printf(">>> HIT: Spiral (Damage 1)\n");
            damage = 1;
            break;
        case 2:
            printf(">>> HIT: Explosion (Damage 2)\n");
            damage = 2;
            break;
        case 3:
            printf(">>> HIT: Firework (Damage 3)\n");
            damage = 3;
            break;
        default:
            printf(">>> Unknown Spell ID: %d\n", spell_num);
            return;
    }

    // 2. Apply Damage
    hb_update(-damage);
    printf("Health Remaining: %d\n", hb_current());

    // 3. Check for Death
    // If dead, play loser screen and reset IMMEDIATELY
    if (hb_current() <= 0) {
        printf("*** DEFEATED ***\n");
        loser_screen(LED_WIDTH, LED_HEIGHT);
        sleep_ms(1000); // Pause on the "L"
        hb_reset();     // Restore full health
        hb_draw();      // Draw full bar
        return;         // Skip the hit animation since we just died
    }

    // 4. Play Hit Animation (If still alive)
    // We clear the screen first so the animation plays on a black background
    ws2812_clear();

    switch(spell_num) {
        case 1:
            // Spiral
            spiral(LED_WIDTH, LED_HEIGHT, &SPIRAL_DEFAULT_RED);
            break;
        case 2:
            // Circle Explosion
            circle_explosion(LED_WIDTH, LED_HEIGHT, &EXPLOSION_DEFAULT_CYAN);
            break;
        case 3:
            // Firework
            firework(LED_WIDTH, LED_HEIGHT, &FIREWORK_DEFAULT_BLUE);
            break;
    }

    // 5. Restore Health Bar
    // The animation is done, now we redraw the surviving health
    hb_draw();
}


// --- Main ---

int main() {
    // 1. System Init
    stdio_init_all();
    sleep_ms(2000); // Wait for USB serial
    printf("\n=== WAND DUEL SYSTEM STARTING ===\n");

    // 2. Initialize Subsystems
    
    // IR
    ir_emitter_init(TX_PIN);
    ir_receiver_init(RX_PIN);
    
    // Haptics
    if (drv2605_init(&haptic, i2c0, I2C_SDA, I2C_SCL)) {
        printf("Haptics initialized.\n");
        drv2605_set_mode(&haptic, DRV2605_MODE_INTTRIG);
        drv2605_select_library(&haptic, 1); // Library 1 = Strong Click
    } else {
        printf("Haptics initialization FAILED!\n");
    }

    // LEDs
    ws2812_init();
    hb_init(LED_WIDTH, LED_HEIGHT);
    hb_draw(); // Draw initial state (Full Health)

    printf("System Ready. Listening for spells...\n");
    
    // Variables for loop
    ir_decoded_data_t received_data;
    bool casting_state = false;
    uint8_t last_processed_spell = 0;

    while (true) {
        
        // --- A. IR Receiver Logic ---
        if (ir_receiver_decode(&received_data)) {
            // We only care about NEC protocol
            if (received_data.protocol == IR_PROTOCOL_NEC) {
                uint8_t spell_cmd = received_data.command;
                
                // Add to rolling buffer
                add_packet_to_buffer(spell_cmd);

                // Validate: Do we have 2 matching packets? 
                // Also ensure we don't re-trigger on the exact same packet stream continuously
                // (Logic: if spell_cmd changes or time passes, we accept. 
                // Simple logic: just check validation. Debouncing is handled by the fact animations are blocking).
                if (validate_spell_input(spell_cmd)) {
                    
                    // 1. Haptic Feedback (Physical reaction first)
                    drv2605_play_hit_feedback(&haptic);

                    // 2. Process Logic & Visuals
                    process_spell_effect(spell_cmd);
                    
                    // 3. Clear buffer to prevent double-triggering
                    clear_packet_buffer();
                }
            }
            // Resume receiver for next frame
            ir_receiver_resume(); 
        }

        // --- B. IR Emitter Logic ---
        // Must be called frequently to handle non-blocking PWM timing
        ir_emitter_update();


        // --- C. Test Casting Logic (Optional) ---
        // Sends a spell periodically if enabled
        if (TEST_CAST_SPELL) {
            static uint32_t last_cast_time = 0;
            uint32_t now = to_ms_since_boot(get_absolute_time());

            // Cast every 5 seconds if idle
            if (!casting_state && (now - last_cast_time > 5000)) {
                printf("--- CASTING TEST SPELL %d ---\n", TEST_SPELL_ID);
                
                // Haptic "Recoil"
                drv2605_play_cast_feedback(&haptic);
                
                // Start transmission (Addr=1, Cmd=SpellID, Repeats=3)
                ir_emitter_start(1, TEST_SPELL_ID, 3);
                
                casting_state = true;
                last_cast_time = now;
            }

            // Check if transmission finished
            if (casting_state && ir_emitter_done()) {
                casting_state = false;
            }
        }

        // Small delay to prevent CPU hogging (IR logic handles timing internally)
        sleep_ms(1);
    }
}