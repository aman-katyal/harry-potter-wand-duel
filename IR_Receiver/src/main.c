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

int main() {
    // Initialize standard IO (for printf)
    stdio_init_all();

    // Wait a few seconds for the serial monitor to connect
    sleep_ms(3000);
    printf("--- Pico NEC IR Receiver ---\n");

    // Initialize the IR receiver library
    ir_receiver_init(IR_RECEIVE_GPIO_PIN);
    printf("IR Receiver initialized on GPIO %d\n", IR_RECEIVE_GPIO_PIN);

    ir_decoded_data_t decoded_data;

    // Main application loop
    while (true) {
        // Check if the receiver has a complete frame to decode
        if (ir_receiver_decode(&decoded_data)) {

            // A decode attempt was made. Check the results.
            if (decoded_data.protocol == IR_PROTOCOL_NEC) {
                
                // --- Print the decoded data ---
                printf("NEC Frame Received:\n");
                printf("  Address: 0x%04X\n", decoded_data.address);
                printf("  Command: 0x%04X\n", decoded_data.command);

                if (decoded_data.flags & IRDATA_FLAGS_IS_REPEAT) {
                    printf("  Type:    REPEAT\n");
                } else {
                    printf("  Type:    Frame\n");
                }

            } else if (decoded_data.flags & IRDATA_FLAGS_WAS_OVERFLOW) {
                printf("Warning: IR buffer overflow. Frame discarded.\n");
            } else {
                printf("Received unknown protocol or noise. Frame discarded.\n");
            }

            // --- IMPORTANT ---
            // We must call resume() to tell the receiver to start
            // listening for the next frame.
            ir_receiver_resume();
        
        } else {
            // No frame ready for decoding.
            // We can do other work here.
            sleep_ms(10); // Don't burn CPU cycles
        }
    }
}