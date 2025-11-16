#include "controller.h"
#include "firework.h"
#include "spiral.h"
#include "circle_explosion.h"
#include "ws2812.h"

// ---------------------------------------------------------
// INTERNAL HELPERS
// ---------------------------------------------------------

// Map spell number → color mode
static int pick_color(uint8_t spell) {
    switch (spell) {
        case 0: return 0;   // firework blue
        case 1: return 1;   // firework red
        case 2: return 2;   // firework magenta
        case 3: return 1;   // spiral red
        case 4: return 2;   // circle explosion cyan-ish
        default: return 0;
    }
}

// Map spell number → LED intensity
static int pick_intensity(uint8_t spell) {
    switch (spell) {
        case 0: return 255;
        case 1: return 255;
        case 2: return 255;
        case 3: return 255;
        case 4: return 255;
        default: return 200;
    }
}

// ---------------------------------------------------------
// ANIMATION CONTROLLER
// ---------------------------------------------------------

void controller(uint8_t spell_num, int width, int height) 
{
    int color     = pick_color(spell_num);
    int intensity = pick_intensity(spell_num);

    switch (spell_num) {

        // FIREWORKS (spells 0, 1, 2)
        case 0:
        case 1:
        case 2:
            firework(width, height, color, intensity);
            break;

        // SPIRAL (spell 3)
        case 3:
            spiral(width, height, color, intensity);
            break;

        // CIRCLE EXPLOSION (spell 4)
        case 4:
            circle_explosion(width, height, color, intensity);
            break;

        // DEFAULT — clear LEDs
        default:
            ws2812_clear();
            break;
    }
}