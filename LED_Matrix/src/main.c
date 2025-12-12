#include "pico/stdlib.h"
#include "ws2812.h"
#include "controller.h"

// Define Matrix Dimensions
#define WIDTH  16
#define HEIGHT 16

// Prototype for the timer function (assuming it is defined in another .c file)
void run_big_timer(int start_seconds);

int main() {
    // 1. Initialize Standard IO and LED Driver
    stdio_init_all();
    ws2812_init();

    while (true) {
        // --- Cycle through all spells defined in controller.c ---
        // 0: Blue Shield
        // 1: Firework (Red)
        // 2: Firework (Blue)
        // 3: Firework (Magenta)
        // 4: Spiral
        // 5: Circle Explosion
        // 6: Heal (Heart)
        
        for (int spell_id = 0; spell_id <= 6; spell_id++) {
            // The controller handles the params and execution
            controller(6, WIDTH, HEIGHT);
            
            // Small delay between animations
            sleep_ms(500);
        }

        // --- Test the Timer separately ---
        // (Since it's not currently in the controller switch-case)
        run_big_timer(5); 

        // Wait before restarting the loop
        sleep_ms(1000);
    }

    return 0;
}