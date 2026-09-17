#include "controller.h"
#include "ws2812.h"

// NEW: Include all animation headers
#include "firework.h"
#include "spiral.h"
#include "circle_explosion.h"

// NEW: Include your new config file
#include "animation_params.h"

// DELETED: We no longer need pick_color or pick_intensity here.
// They are now part of the animation_params.c file.

// ---------------------------------------------------------
// ANIMATION CONTROLLER
// ---------------------------------------------------------

void controller(uint8_t spell_num, int width, int height) 
{
    // UPDATED: This switch now calls the animation function
    // with the corresponding "default params" struct.
    switch (spell_num) {

        // FIREWORKS (spells 0, 1, 2)
        case 0:
            firework(width, height, &FIREWORK_DEFAULT_BLUE);
            break;
        case 1:
            firework(width, height, &FIREWORK_DEFAULT_RED);
            break;
        case 2:
            firework(width, height, &FIREWORK_DEFAULT_MAGENTA);
            break;

        // SPIRAL (spell 3)
        case 3:
            spiral(width, height, &SPIRAL_DEFAULT_RED);
            break;

        // CIRCLE EXPLOSION (spell 4)
        case 4:
            circle_explosion(width, height, &EXPLOSION_DEFAULT_CYAN);
            break;

        // DEFAULT — clear LEDs
        default:
            ws2812_clear();
            break;
    }
}