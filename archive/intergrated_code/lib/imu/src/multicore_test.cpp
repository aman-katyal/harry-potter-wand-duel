#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"

extern "C" {
    #include "ir_emitter.h"
    #include "irremote.h"
    #include "drv2605.h"
    #include "ws2812.h"
    #include "controller.h"
    #include "healthbar.h"
}

extern "C" void imu_core1_entry();   // from testmodelv2.cpp




// ===================================
// PIN & HARDWARE CONFIGURATION
// ===================================
#define TX_PIN 36
#define RX_PIN 15
#define I2C_SDA 16
#define I2C_SCL 17

#define BTN_A 21  // Spell 1
#define BTN_B 26  // Spell 2
                  // Combo (A+B) = Spell 3

#define LED_WIDTH  16
#define LED_HEIGHT 16

// ===================================
// GAME CONFIGURATION
// ===================================
#define MY_PLAYER_ID 1        // Change to 2 for the second wand
#define OPPONENT_PLAYER_ID 2  // Change to 1 for the second wand
#define PACKET_REPEATS 3 

// Time to stay "dead" after the loser screen before you can cast again
#define RESPAWN_DELAY_MS 3000 

// ===================================
// INTERNAL GLOBALS
// ===================================
#define PACKET_BUFFER_SIZE 3
static uint8_t packet_buffer[PACKET_BUFFER_SIZE] = {0};
static int packet_index = 0;

// Track when we died to handle respawn delay
static uint32_t death_timestamp = 0;
static bool is_dead = false;

// --- Helper Functions ---

bool validate_spell(uint8_t spell_num) {
    int match_count = 0;
    for (int i = 0; i < PACKET_BUFFER_SIZE; i++) {
        if (packet_buffer[i] == spell_num) match_count++;
    }
    return (match_count >= 2);
}

void add_packet(uint8_t spell_num) {
    packet_buffer[packet_index] = spell_num;
    packet_index = (packet_index + 1) % PACKET_BUFFER_SIZE;
}

void clear_packets() {
    for (int i = 0; i < PACKET_BUFFER_SIZE; i++) packet_buffer[i] = 0;
    packet_index = 0;
}

void init_buttons() {
    // Active High (Press = 1) - Connect buttons to 3.3V
    gpio_init(BTN_A); gpio_set_dir(BTN_A, GPIO_IN); gpio_pull_down(BTN_A);
    gpio_init(BTN_B); gpio_set_dir(BTN_B, GPIO_IN); gpio_pull_down(BTN_B);
}

// Maps Spell ID to Animation ID and Damage
void process_hit_effects(uint8_t spell_num) {
    int anim_id = 0;
    int damage = 0;

    switch(spell_num) {
        case 1: 
            printf(">>> Hit by Spell 1 (Spiral)\n");
            anim_id = 3; // Spiral in your controller.c
            damage = 1;
            break;
        case 2: 
            printf(">>> Hit by Spell 2 (Firework)\n");
            anim_id = 0; // Firework in your controller.c
            damage = 2;
            break;
        case 3: 
            printf(">>> Hit by Spell 3 (Explosion)\n");
            anim_id = 4; // Explosion in your controller.c
            damage = 3;
            break;
        default:
            return; // Unknown spell
    }

    // 1. Apply Damage to HealthBar system
    hb_update(-damage);

    // 2. Clear screen for animation
    ws2812_clear();

    // 3. Play Animation (Blocking)
    // This acts as a natural "stun" duration where you can't shoot back
    controller(anim_id, LED_WIDTH, LED_HEIGHT);

    // 4. Animation done: Redraw the Health Bar
    hb_draw();
}

int main() {
    stdio_init_all();
    sleep_ms(2000); // Power-up safety delay

    printf("\n=== Wand Duel (LED Integrated, IMU on Core 1) ===\n");

    // --- Launch IMU classifier on Core 1 ---
    multicore_launch_core1(imu_core1_entry);

    // --- Initialize Hardware ---
    init_buttons();
    ir_emitter_init(TX_PIN);
    ir_receiver_init(RX_PIN);
    ws2812_init();
    
    // Haptics
    drv2605_t haptic;
    drv2605_init(&haptic, i2c0, I2C_SDA, I2C_SCL);
    drv2605_set_mode(&haptic, DRV2605_MODE_INTTRIG);
    drv2605_select_library(&haptic, 1);
    
    // --- Initialize Game Graphics ---
    hb_init(LED_WIDTH, LED_HEIGHT);
    hb_draw(); // Draw initial full health

    printf("System Ready.\n");
    printf("My ID: %d | Target: %d\n", MY_PLAYER_ID, OPPONENT_PLAYER_ID);
    
    ir_decoded_data_t received_spell;
    bool currently_casting = false;
    uint8_t last_spell = 0;
    
    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        // ==========================================
        // 1. RESPAWN CHECK
        // ==========================================
        if (is_dead) {
            if (now - death_timestamp > RESPAWN_DELAY_MS) {
                // Respawn logic
                is_dead = false;
                hb_reset(); // Reset health in library
                hb_draw();  // Draw full health
                printf("*** RESPAWNED ***\n");
                
                // Flash white to indicate respawn
                ws2812_clear();
                sleep_ms(50);
                hb_draw();
            }
        }

        // ==========================================
        // 2. RECEIVE LOGIC (getting hit via IR)
        // ==========================================
        if (!is_dead && ir_receiver_decode(&received_spell)) {
            if (received_spell.protocol == IR_PROTOCOL_NEC) {
                
                // Filter: Must be from opponent
                if (received_spell.address == OPPONENT_PLAYER_ID) {
                    uint8_t spell_num = received_spell.command;
                    add_packet(spell_num);
                
                    if (validate_spell(spell_num) && spell_num != last_spell) {
                        
                        // --- HIT CONFIRMED ---
                        printf("*** HIT CONFIRMED! ***\n");
                        
                        // A. Haptic Feedback
                        drv2605_play_hit_feedback(&haptic);
                        
                        // B. Play LED Animation & Update Health
                        process_hit_effects(spell_num);
                        
                        // C. Reset Packet Buffer
                        last_spell = spell_num;
                        clear_packets();
                        
                        // D. Check Death using Healthbar Library
                        if (hb_current() <= 0) {
                            printf("*** YOU DIED ***\n");
                            is_dead = true;
                            death_timestamp = to_ms_since_boot(get_absolute_time());
                            
                            // Play Loser Animation
                            loser_screen(LED_WIDTH, LED_HEIGHT);
                            
                            // Clear screen while dead
                            ws2812_clear(); 
                        }
                    }
                }
            }
        }
        
        // ==========================================
        // 3. TRANSMIT UPDATE
        // ==========================================
        ir_emitter_update();
        
        if (currently_casting && ir_emitter_done()) {
            currently_casting = false;
            printf("Spell sent.\n");
        }

        // ==========================================
        // 4a. IMU-BASED CASTING (from core 1)
        // ==========================================
        if (!is_dead && !currently_casting && ir_emitter_done()) {
            if (multicore_fifo_rvalid()) {
                uint32_t class_idx = multicore_fifo_pop_blocking();

                // TODO: adjust mapping based on your actual model labels.
                // Example for labels: [0] = "aguamenti", [1] = "idle", [2] = "stupefy"
                int spell_to_cast = 0;
                switch (class_idx) {
                    case 0: // e.g. aguamenti gesture
                        spell_to_cast = 1;
                        break;
                    case 1: // idle -> no spell
                        spell_to_cast = 0;
                        break;
                    case 2: // e.g. stupefy gesture
                        spell_to_cast = 2;
                        break;
                    default:
                        spell_to_cast = 0;
                        break;
                }

                if (spell_to_cast > 0) {
                    printf("IMU CASTING Spell %d (class_idx=%lu)\n",
                           spell_to_cast, (unsigned long)class_idx);

                    drv2605_play_cast_feedback(&haptic);
                    ir_emitter_start(MY_PLAYER_ID, spell_to_cast, PACKET_REPEATS);
                    currently_casting = true;

                    sleep_ms(400); // cooldown / debounce
                }
            }
        }

        // ==========================================
        // 4b. BUTTONS (CASTING) – keep for debugging or fallback
        // ==========================================
        if (!is_dead && !currently_casting && ir_emitter_done()) {
            
            bool a_pressed = gpio_get(BTN_A);
            bool b_pressed = gpio_get(BTN_B);

            if (a_pressed || b_pressed) {
                sleep_ms(50); // Combo window
                a_pressed = gpio_get(BTN_A);
                b_pressed = gpio_get(BTN_B);
                
                int spell_to_cast = 0;

                if (a_pressed && b_pressed) spell_to_cast = 3;
                else if (a_pressed) spell_to_cast = 1;
                else if (b_pressed) spell_to_cast = 2;

                if (spell_to_cast > 0) {
                    printf("BUTTON CASTING Spell %d\n", spell_to_cast);
                    
                    drv2605_play_cast_feedback(&haptic);
                    ir_emitter_start(MY_PLAYER_ID, spell_to_cast, PACKET_REPEATS);
                    currently_casting = true;
                    
                    sleep_ms(400); // Debounce
                }
            }
        }
        
        sleep_ms(1);
    }
}
