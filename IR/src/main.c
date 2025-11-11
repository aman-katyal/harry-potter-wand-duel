#include <stdio.h>
#include "pico/stdlib.h"
#include "ir_emitter.h"
#include "ir_receiver_lib/irremote.h"

// Pin definitions
#define TX_PIN 36  // IR LED for transmitting
#define RX_PIN 15  // IR receiver module

// ========================================
// TEST FLAG - Change this for testing!
// Player 1: Set to true
// Player 2: Set to false
// ========================================
#define TEST_CAST_SPELL true  // <-- CHANGE THIS PER COMPUTER

int main() {
    stdio_init_all();
    sleep_ms(3000);
    
    printf("\n=== Harry Potter Wand Duel ===\n");
    
    // Initialize both transmitter and receiver
    ir_emitter_init(TX_PIN);
    ir_receiver_init(RX_PIN);
    
    printf("Wand ready! TX: GPIO%d, RX: GPIO%d\n", TX_PIN, RX_PIN);
    printf("Test mode: %s\n", TEST_CAST_SPELL ? "CASTING" : "RECEIVING ONLY");
    
    ir_decoded_data_t received_spell;
    bool spell_cast = false;
    bool cast_complete_printed = false;
    
    while (true) {
        // ===== ALWAYS CHECK FOR INCOMING SPELLS =====
        if (ir_receiver_decode(&received_spell)) {
            if (received_spell.protocol == IR_PROTOCOL_NEC) {
                printf("\n🎯 HIT! Received spell:\n");
                printf("  From Player: %d\n", received_spell.address);
                printf("  Spell Type: %d\n", received_spell.command);
                printf("  Flags: 0x%02X\n\n", received_spell.flags);
                
                // TODO: Add haptic feedback for being hit
                // TODO: Add LED animation for being hit
            }
        }
        
        // ===== UPDATE EMITTER (non-blocking) =====
        ir_emitter_update();
        
        // ===== TEST SPELL CASTING =====
        // Replace this entire block with IMU gesture detection later
        if (TEST_CAST_SPELL && !spell_cast) {
            printf("⚡ Casting spell!\n");
            // addr = player ID, cmd = spell type, repeats = 3 for reliability
            ir_emitter_start(1, 2, 3);
            spell_cast = true;
        }
        
        // Print confirmation when cast completes
        if (spell_cast && ir_emitter_done() && !cast_complete_printed) {
            printf("✅ Spell transmission complete!\n");
            cast_complete_printed = true;
        }
        
        sleep_ms(1);
    }
}