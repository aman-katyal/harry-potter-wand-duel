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

#define TEST_CAST_SPELL true 

int main() {
    stdio_init_all();
    sleep_ms(3000);
    
    printf("\n---Wand Duel---\n");
    
    // Initialize all systems
    ir_emitter_init(TX_PIN);
    ir_receiver_init(RX_PIN);
    
    drv2605_t haptic;
    drv2605_init(&haptic, i2c0, I2C_SDA, I2C_SCL);
    drv2605_set_mode(&haptic, DRV2605_MODE_INTTRIG);
    drv2605_select_library(&haptic, 1);
    
    printf("Wand ready! TX:%d RX:%d\n", TX_PIN, RX_PIN);
    printf("Test mode: %s\n", TEST_CAST_SPELL ? "CASTING" : "RECEIVING");
    
    ir_decoded_data_t received_spell;
    bool spell_cast = false;
    bool cast_complete_printed = false;
    
    while (true) {
        if (ir_receiver_decode(&received_spell)) {
            if (received_spell.protocol == IR_PROTOCOL_NEC) {
                printf("\nHIT! Player:%d Spell:%d\n", 
                       received_spell.address, received_spell.command);
                drv2605_play_hit_feedback(&haptic);  // Strong haptic
            }
        }
        
        ir_emitter_update();

        if (TEST_CAST_SPELL && !spell_cast) {
            printf("Casting spell:\n");
            drv2605_play_cast_feedback(&haptic);  // Light haptic
            ir_emitter_start(2, 1, 4);
            spell_cast = true;
        }
        
        if (spell_cast && ir_emitter_done() && !cast_complete_printed) {
            printf("Spell complete!\n");
            cast_complete_printed = true;
        }
        
        sleep_ms(1);
    }
}