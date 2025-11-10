/*
 * main.c
 *
 * Example application for the Pico C IR Receiver Library (NEC Only).
 * Initializes the receiver, polls for decoded data in the main loop,
 * and prints the result to standard IO (USB Serial).
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "irremote.h" // Our library's main header

// --- Configuration ---
// Define the GPIO pin for the IR receiver.
// Pin 15 is used based on the PinDefinitionsAndMore.h context.
#define IR_RECEIVE_GPIO_PIN 15

// In main.c

int main() {
    stdio_init_all();
    
    // This sleep is only for waiting on a serial monitor.
    // In a larger project, you would remove it.
    // sleep_ms(3000); 
    printf("--- Pico NEC IR Receiver ---\n");

    ir_receiver_init(IR_RECEIVE_GPIO_PIN);
    printf("IR Receiver initialized on GPIO %d\n", IR_RECEIVE_GPIO_PIN);

    ir_decoded_data_t decoded_data;

    // Main application loop
    while (true) {
        
        // --- This is now your high-level, non-blocking function ---
        // It's very fast and just checks a flag.
        if (ir_receiver_decode(&decoded_data)) {

            // A frame was successfully decoded and the receiver is
            // already listening for the next one.
            
            if (decoded_data.protocol == IR_PROTOCOL_NEC) {
                printf("NEC Frame Received:\n");
                printf("  Address: 0x%04X\n", decoded_data.address);
                printf("  Command: 0x%04X\n", decoded_data.command);
                // ... etc ...
            } else {
                // ... handle overflow or unknown protocol ...
            }

            // --- NO MORE ir_receiver_resume() NEEDED ---
        
        } 
        
        // --- NO MORE 'else' BLOCK OR sleep_ms(10) NEEDED ---
        
        // --- DO OTHER WORK HERE ---
        // Your larger project's code can run here without
        // being blocked.
        // e.g., update_display();
        //       check_network_packets();
        //       read_other_sensors();
    }
}