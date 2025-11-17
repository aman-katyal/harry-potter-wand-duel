#include <stdio.h>
#include "pico/stdlib.h"
#include "ir_emitter.h"
#include "irremote.h"
#include "drv2605.h"

// Pin definitions
#define TX_PIN 36
#define RX_PIN 15
#define I2C_SDA 16
#define I2C_SCL 17

// Button Definitions
#define BTN_A 21
#define BTN_B 26

// ===================================
// GAME CONFIGURATION
// ===================================
#define MY_PLAYER_ID 1        // Change this for Wand 2 (e.g., to 2)
#define OPPONENT_PLAYER_ID 2  // Change this for Wand 2 (e.g., to 1)

#define PACKET_REPEATS 3 
#define MAX_HEALTH 16    
#define RESPAWN_TIME_MS 10000  // Time to stay dead before respawning
// ===================================

// Packet validation
#define PACKET_BUFFER_SIZE 3
static uint8_t packet_buffer[PACKET_BUFFER_SIZE] = {0};
static int packet_index = 0;

// Global Game State
static int current_health = MAX_HEALTH;
static uint32_t death_timestamp = 0;

// --- Helper Functions ---

bool validate_spell(uint8_t spell_num) {
    int match_count = 0;
    for (int i = 0; i < PACKET_BUFFER_SIZE; i++) {
        if (packet_buffer[i] == spell_num) {
            match_count++;
        }
    }
    return (match_count >= 2);
}

void add_packet(uint8_t spell_num) {
    packet_buffer[packet_index] = spell_num;
    packet_index = (packet_index + 1) % PACKET_BUFFER_SIZE;
}

void clear_packets() {
    for (int i = 0; i < PACKET_BUFFER_SIZE; i++) {
        packet_buffer[i] = 0;
    }
    packet_index = 0;
}

void init_buttons() {
    // Active High (Press = 1)
    gpio_init(BTN_A); gpio_set_dir(BTN_A, GPIO_IN); gpio_pull_down(BTN_A);
    gpio_init(BTN_B); gpio_set_dir(BTN_B, GPIO_IN); gpio_pull_down(BTN_B);
}

int get_spell_damage(uint8_t spell_num) {
    switch(spell_num) {
        case 1: return 1;
        case 2: return 2;
        case 3: return 3;
        default: return 0;
    }
}

int main() {
    stdio_init_all();
    sleep_ms(3000);
    
    printf("\n=== Wand Duel ===\n");
    
    init_buttons();
    ir_emitter_init(TX_PIN);
    ir_receiver_init(RX_PIN);
    
    drv2605_t haptic;
    drv2605_init(&haptic, i2c0, I2C_SDA, I2C_SCL);
    drv2605_set_mode(&haptic, DRV2605_MODE_INTTRIG);
    drv2605_select_library(&haptic, 1);
    
    printf("Wand Ready!\n");
    printf("--> My ID: %d\n", MY_PLAYER_ID);
    printf("--> Target ID: %d\n", OPPONENT_PLAYER_ID);
    printf("Health: %d/%d\n\n", current_health, MAX_HEALTH);
    
    ir_decoded_data_t received_spell;
    bool currently_casting = false;
    uint8_t last_spell = 0;
    
    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        // ==========================================
        // 1. RESPAWN LOGIC
        // ==========================================
        if (current_health <= 0) {
            // If enough time has passed since death, revive
            if (now - death_timestamp > RESPAWN_TIME_MS) {
                current_health = MAX_HEALTH;
                last_spell = 0;
                printf("\n*** RESPAWNED! READY TO DUEL ***\n");
                printf("Health: %d/%d\n\n", current_health, MAX_HEALTH);
            }
        }

        // ==========================================
        // 2. RECEIVE SPELLS LOGIC
        // ==========================================
        if (ir_receiver_decode(&received_spell)) {
            if (received_spell.protocol == IR_PROTOCOL_NEC) {
                
                // Only accept spells if we are ALIVE and matching ID
                if (current_health > 0 && received_spell.address == OPPONENT_PLAYER_ID) {
                    
                    uint8_t spell_num = received_spell.command;
                    add_packet(spell_num);
                
                    if (validate_spell(spell_num) && spell_num != last_spell) {
                        printf("\n*** HIT CONFIRMED! ***\n");
                        drv2605_play_hit_feedback(&haptic);
                        current_health -= get_spell_damage(spell_num);
                        
                        last_spell = spell_num;
                        clear_packets();
                        
                        if (current_health <= 0) {
                            current_health = 0; // Clamp to 0
                            death_timestamp = now; // Record time of death
                            printf("*** DEFEATED! Respawning in 5s... ***\n");
                        } else {
                             printf("Health: %d/%d\n", current_health, MAX_HEALTH);
                        }
                    }
                } 
            }
        }
        
        // ==========================================
        // 3. EMITTER UPDATE LOOP
        // ==========================================
        ir_emitter_update();
        
        if (currently_casting && ir_emitter_done()) {
            printf("Spell sent.\n");
            currently_casting = false;
        }

        // ==========================================
        // 4. BUTTON INPUT LOGIC (CASTING)
        // ==========================================
        
        // ONLY allow casting if Emitter is free AND WE ARE ALIVE
        if (!currently_casting && ir_emitter_done()) {
            
            bool a_pressed = gpio_get(BTN_A);
            bool b_pressed = gpio_get(BTN_B);

            if (a_pressed || b_pressed) {
                
                // --- CHECK DEAD STATE ---
                if (current_health <= 0) {
                    printf("(X) Cannot cast! You are defeated.\n");
                    sleep_ms(200); // Small delay so it doesn't spam the console
                } 
                // --- ALIVE: PROCEED TO CAST ---
                else {
                    sleep_ms(50); // Combo delay
                    a_pressed = gpio_get(BTN_A);
                    b_pressed = gpio_get(BTN_B);
                    
                    int spell_to_cast = 0;

                    if (a_pressed && b_pressed) spell_to_cast = 3;
                    else if (a_pressed) spell_to_cast = 1;
                    else if (b_pressed) spell_to_cast = 2;

                    if (spell_to_cast > 0) {
                        printf("CASTING Spell %d\n", spell_to_cast);
                        drv2605_play_cast_feedback(&haptic);
                        ir_emitter_start(MY_PLAYER_ID, spell_to_cast, PACKET_REPEATS);
                        currently_casting = true;
                        sleep_ms(400); 
                    }
                }
            }
        }
        sleep_ms(1);
    }
}