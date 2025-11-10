
#include <stdio.h>
#include "pico/stdlib.h"
#include "ir_emitter.h"

#define TX_PIN 36

int main() {
    stdio_init_all();
    sleep_ms(3000);
    
    printf("\n=== RP2350 NEC IR Transmitter ===\n");
    
    ir_emitter_init(TX_PIN);
    
    // Send addr=1, cmd=2, repeat 5 times
    ir_emitter_start(1, 2, 5);

    while (true) {
        ir_emitter_update();
        
        // Check if done and start new sequence
        if (ir_emitter_done()) {
            sleep_ms(1000); // Wait between sequences
            ir_emitter_start(3, 4, 3); // Send different packet
        }
        
        sleep_ms(1);
    }
}