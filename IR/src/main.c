// #include <stdio.h>
// #include "pico/stdlib.h"
// #include "ir_emitter.h"
// //#include "ir_receiver_lib/irremote.h"
// #include "irremote.h"

// // Pin definitions
// #define TX_PIN 36  //transmitting
// #define RX_PIN 15  //receiving


// #define TEST_CAST_SPELL true 

// int main() {
//     stdio_init_all();
//     sleep_ms(3000);
    
//     printf("\n---Wand Duel---\n");
    
//     // Initialize both transmitter and receiver
//     ir_emitter_init(TX_PIN);
//     ir_receiver_init(RX_PIN);
    
//     printf("Wand ready! TX: GPIO%d, RX: GPIO%d\n", TX_PIN, RX_PIN);
//     printf("Test mode: %s\n", TEST_CAST_SPELL ? "CASTING" : "RECEIVING ONLY");
    
//     ir_decoded_data_t received_spell;
//     bool spell_cast = false;
//     bool cast_complete_printed = false;
    
//     while (true) {
//         if (ir_receiver_decode(&received_spell)) {
//             if (received_spell.protocol == IR_PROTOCOL_NEC) {
//                 printf("\nHIT! Received spell:\n");
//                 printf("From Player: %d\n", received_spell.address);
//                 printf("Spell Type: %d\n", received_spell.command);
//                 printf("Flags: 0x%02X\n\n", received_spell.flags);
//             }
//         }
        
//         ir_emitter_update();

//         if (TEST_CAST_SPELL && !spell_cast) {
//             printf("Casting spell:\n");
//             //addr = player ID, cmd = spell type, repeats
//             ir_emitter_start(1, 2, 3);
//             spell_cast = true;
//         }
        
//         if (spell_cast && ir_emitter_done() && !cast_complete_printed) {
//             printf("Spell complete!\n");
//             cast_complete_printed = true;
//         }
        
//         sleep_ms(1);
//     }
// }










// #include <stdio.h>
// #include "pico/stdlib.h"
// #include "ir_emitter.h"
// #include "irremote.h"
// #include "drv2605.h"

// // Pin definitions
// #define TX_PIN 36
// #define RX_PIN 15
// #define I2C_SDA 16
// #define I2C_SCL 17

// #define TEST_CAST_SPELL true 

// int main() {
//     stdio_init_all();
//     sleep_ms(3000);
    
//     printf("\n---Wand Duel---\n");
    
//     // Initialize all systems
//     ir_emitter_init(TX_PIN);
//     ir_receiver_init(RX_PIN);
    
//     drv2605_t haptic;
//     drv2605_init(&haptic, i2c0, I2C_SDA, I2C_SCL);
//     drv2605_set_mode(&haptic, DRV2605_MODE_INTTRIG);
//     drv2605_select_library(&haptic, 1);
    
//     printf("Wand ready! TX:%d RX:%d\n", TX_PIN, RX_PIN);
//     printf("Test mode: %s\n", TEST_CAST_SPELL ? "CASTING" : "RECEIVING");
    
//     ir_decoded_data_t received_spell;
//     bool spell_cast = false;
//     bool cast_complete_printed = false;
    
//     while (true) {
//         if (ir_receiver_decode(&received_spell)) {
//             if (received_spell.protocol == IR_PROTOCOL_NEC) {
//                 printf("\nHIT! Player:%d Spell:%d\n", 
//                        received_spell.address, received_spell.command);
//                 drv2605_play_hit_feedback(&haptic);  // Strong haptic
//             }
//         }
        
//         ir_emitter_update();

//         if (TEST_CAST_SPELL && !spell_cast) {
//             printf("Casting spell:\n");
//             drv2605_play_cast_feedback(&haptic);  // Light haptic
//             ir_emitter_start(2, 1, 4);
//             spell_cast = true;
//         }
        
//         if (spell_cast && ir_emitter_done() && !cast_complete_printed) {
//             printf("Spell complete!\n");
//             cast_complete_printed = true;
//         }
        
//         sleep_ms(1);
//     }
// }

#include <stdio.h>
#include "pico/stdlib.h"
#include "ir_emitter.h"
#include "irremote.h"
#include "drv2605.h"
#include "ws2812.h"
#include "firework.h"
#include "spiral.h"
#include "circle_explosion.h"
#include "healthbar.h"

// Pin definitions
#define TX_PIN 36
#define RX_PIN 15
#define I2C_SDA 16
#define I2C_SCL 17
#define LED_WIDTH 16
#define LED_HEIGHT 16

// Test configuration
#define TEST_CAST_SPELL true 
#define TEST_SPELL_NUMBER 2  // Change to 1, 2, or 3 to test different spells

// Packet validation
#define PACKET_BUFFER_SIZE 3
static uint8_t packet_buffer[PACKET_BUFFER_SIZE] = {0};
static int packet_index = 0;

// Validate: need at least 2 matching packets
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

void display_spell(uint8_t spell_num) {
    int health_cost = 0;
    
    switch(spell_num) {
        case 1:  // Spiral (-1 HP)
            printf(">>> Spiral Spell!\n");
            spiral(LED_WIDTH, LED_HEIGHT, 0, 255);
            health_cost = -1;
            break;
            
        case 2:  // Circle Explosion (-2 HP)
            printf(">>> Circle Explosion Spell!\n");
            circle_explosion(LED_WIDTH, LED_HEIGHT, 1, 255);
            health_cost = -2;
            break;
            
        case 3:  // Firework (-3 HP)
            printf(">>> Firework Spell!\n");
            firework(LED_WIDTH, LED_HEIGHT, 2, 255);
            health_cost = -3;
            break;
            
        default:
            printf("Unknown spell: %d\n", spell_num);
            return;
    }
    
    // Update health and redraw
    hb_update(health_cost);
    hb_draw();
    printf("Health: %d/%d\n", hb_get_health(), LED_WIDTH);
    
    // Clear packet buffer after successful spell
    clear_packets();
}

int main() {
    stdio_init_all();
    sleep_ms(3000);
    
    printf("\n=== Wand Duel ===\n");
    
    // Initialize all systems
    ir_emitter_init(TX_PIN);
    ir_receiver_init(RX_PIN);
    
    drv2605_t haptic;
    drv2605_init(&haptic, i2c0, I2C_SDA, I2C_SCL);
    drv2605_set_mode(&haptic, DRV2605_MODE_INTTRIG);
    drv2605_select_library(&haptic, 1);
    
    ws2812_init();
    hb_init(LED_WIDTH, LED_HEIGHT);
    hb_draw();  // Draw initial full health
    
    printf("Systems ready! TX:%d RX:%d LED:18\n", TX_PIN, RX_PIN);
    printf("Test: %s Spell %d\n", 
           TEST_CAST_SPELL ? "CASTING" : "RECEIVING", TEST_SPELL_NUMBER);
    printf("Health: %d/%d\n\n", hb_get_health(), LED_WIDTH);
    
    ir_decoded_data_t received_spell;
    bool spell_cast = false;
    bool cast_printed = false;
    uint8_t last_spell = 0;
    
    while (true) {
        // === RECEIVE SPELLS ===
        if (ir_receiver_decode(&received_spell)) {
            if (received_spell.protocol == IR_PROTOCOL_NEC) {
                uint8_t spell_num = received_spell.command;
                
                printf("Packet: spell=%d (%d/3)\n", spell_num, 
                       (packet_index % PACKET_BUFFER_SIZE) + 1);
                
                add_packet(spell_num);
                
                // Validate after receiving each packet
                if (validate_spell(spell_num) && spell_num != last_spell) {
                    printf("\n*** VALIDATED! ***\n");
                    
                    drv2605_play_hit_feedback(&haptic);
                    display_spell(spell_num);
                    last_spell = spell_num;
                    
                    if (hb_get_health() <= 0) {
                        printf("\n*** DEFEATED! Resetting... ***\n");
                        sleep_ms(2000);
                        hb_init(LED_WIDTH, LED_HEIGHT);
                        hb_draw();
                        last_spell = 0;
                    }
                }
            }
        }
        
        // === UPDATE EMITTER ===
        ir_emitter_update();
        
        // === CAST SPELL (TEST) ===
        if (TEST_CAST_SPELL && !spell_cast) {
            printf("\n=== CASTING Spell %d ===\n", TEST_SPELL_NUMBER);
            drv2605_play_cast_feedback(&haptic);
            ir_emitter_start(2, TEST_SPELL_NUMBER, 3);  // Send 3 packets
            spell_cast = true;
        }
        
        if (spell_cast && ir_emitter_done() && !cast_printed) {
            printf("Cast complete!\n\n");
            cast_printed = true;
        }
        
        sleep_ms(1);
    }
}