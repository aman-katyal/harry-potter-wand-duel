#include "pico/stdlib.h"
#include "ws2812.h"
#include "led_effects.h"

int main() {
    stdio_init_all();

    // Initialize the hardware and effects engine
    ws2812_init();
    effects_init();

    // Set an initial effect
    effects_set_mode(EFFECT_BREATHING, COLOR_GRB(0, 150, 150)); // Breathing Cyan

    absolute_time_t next_effect_change_time = make_timeout_time_ms(5000);
    int effect_index = 0;

    while (true) {
        // This one function runs all the animation logic
        effects_update();

        // Example of how to cycle through effects automatically
        if (time_reached(next_effect_change_time)) {
            effect_index = (effect_index + 1) % 3;
            switch (effect_index) {
                case 0:
                    effects_set_mode(EFFECT_BREATHING, COLOR_GRB(0, 150, 150)); // Breathing Cyan
                    break;
                case 1:
                    effects_set_mode(EFFECT_BURST, COLOR_GRB(150, 0, 150)); // Burst Magenta
                    break;
                case 2:
                    effects_set_mode(EFFECT_STATIC, COLOR_GRB(150, 100, 0)); // Static Orange
                    break;
            }
            next_effect_change_time = make_timeout_time_ms(5000); // Change again in 5 seconds
        }

        // You could do other non-blocking work here, like checking buttons or sensors
    }
}