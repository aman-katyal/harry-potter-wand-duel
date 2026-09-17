#include "controller.h"
#include "ws2812.h"

// Animation Headers
#include "firework.h"
#include "spiral.h"
#include "circle_explosion.h"
#include "blue_shield.h"
#include "heal.h" 
#include "animation_params.h"

void controller(uint8_t spell_num, int width, int height) 
{
    // Ensure we are in rotation mode 1 (90 degrees) for your matrix setup
    ws2812_set_rotation(1); 

    switch (spell_num) {
        // --- Spell 0: Shield (Reserved for Button) ---
        case 0:
            blue_shield_run(width, height, &SHIELD_DEFAULT_CONFIG);
            break;

        // --- Spell 1: Stupefy (Red Firework) ---
        case 1:
            firework(width, height, &FIREWORK_DEFAULT_RED);
            break;

        // --- Spell 2: Aguamenti (Blue Firework) ---
        case 2:
            firework(width, height, &FIREWORK_DEFAULT_BLUE);
            break;

        // --- Spell 3: Misc (Magenta Firework) ---
        case 3:
            firework(width, height, &FIREWORK_DEFAULT_MAGENTA);
            break;

        // --- Spell 4: Spiral ---
        case 4:
            spiral(width, height, &SPIRAL_DEFAULT_RED);
            break;

        // --- Spell 5: Explosion ---
        case 5:
            circle_explosion(width, height, &EXPLOSION_DEFAULT_CYAN);
            break;

        // --- Spell 6: Heal ---
        case 6:
            heal_run(width, height, &HEAL_DEFAULT_CONFIG);
            break;

        default:
            ws2812_clear();
            break;
    }
}