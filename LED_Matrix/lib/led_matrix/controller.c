#include "controller.h"
#include "ws2812.h"

// Animation Headers
#include "firework.h"
#include "spiral.h"
#include "circle_explosion.h"
#include "blue_shield.h"
#include "heal.h" // Include the Heart/Heal animation

// Configuration Header
#include "animation_params.h"

// ---------------------------------------------------------
// ANIMATION CONTROLLER
// ---------------------------------------------------------

void controller(uint8_t spell_num, int width, int height) 
{
    // 1. Set Global Rotation
    // 0 = Normal
    // 1 = 90 Degree Rotation (Corrected logic)
    ws2812_set_rotation(1); 

    // 2. Select Animation
    switch (spell_num) {

        // --- DEFENSE SPELLS ---
        case 0:
            // Blue Shield (Protective Spell)
            blue_shield_run(width, height, &SHIELD_DEFAULT_CONFIG); 
            break;
            
        // --- ATTACK SPELLS (Fireworks) ---
        case 1:
            firework(width, height, &FIREWORK_DEFAULT_RED);
            break;
        case 2:
            firework(width, height, &FIREWORK_DEFAULT_BLUE);
            break;
        case 3:
            firework(width, height, &FIREWORK_DEFAULT_MAGENTA);
            break;

        // --- SPECIALTY SPELLS ---
        case 4:
            // Spiral
            spiral(width, height, &SPIRAL_DEFAULT_RED);
            break;

        case 5:
            // Circle Explosion (Cyan)
            circle_explosion(width, height, &EXPLOSION_DEFAULT_CYAN);
            break;

        case 6:
            // Healing Heart
            heal_run(width, height, &HEAL_DEFAULT_CONFIG);
            break;

        // --- DEFAULT ---
        default:
            ws2812_clear();
            break;
    }
}