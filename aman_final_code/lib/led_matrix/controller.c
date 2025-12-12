#include "controller.h"
#include "ws2812.h"
#include "pico/time.h"

// Include your animation headers
// (You will need to update these .h files to expose _start and _update functions)
#include "firework.h"
#include "spiral.h"
#include "circle_explosion.h"

// Configuration
#define FRAME_DELAY_MS 33  // ~30 FPS

// State
static uint8_t current_anim = 0;
static bool is_anim_running = false;
static uint64_t last_frame_time = 0;

void controller_init() {
    ws2812_clear();
}

void controller_start_spell(uint8_t spell_num) {
    current_anim = spell_num;
    is_anim_running = true;
    
    // Reset/Start the specific animation
    switch (spell_num) {
        case 1: // Aguamenti
            // firework_start(BLUE); // Example of what you need in firework.c
            break;
        case 2: // Stupefy
            // firework_start(RED);
            break;
        default:
            is_anim_running = false;
            ws2812_clear();
            break;
    }
}

void controller_update() {
    if (!is_anim_running) return;

    // Non-blocking timer check
    uint64_t now = to_ms_since_boot(get_absolute_time());
    if (now - last_frame_time < FRAME_DELAY_MS) return;
    last_frame_time = now;

    // Dispatch to the active animation
    bool finished = false;
    
    switch (current_anim) {
        case 1: 
        case 2:
            // finished = firework_update(); // Should return true when done
            break;
            
        // Add other cases...
    }

    // If animation reports it's done, stop updating
    if (finished) {
        is_anim_running = false;
        ws2812_clear();
    }
}