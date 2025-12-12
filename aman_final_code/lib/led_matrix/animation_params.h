#ifndef ANIMATION_PARAMS_H
#define ANIMATION_PARAMS_H

// Include all the animation headers that define the structs
#include "firework.h"
#include "spiral.h"
#include "circle_explosion.h"
#include "blue_shield.h"
#include "heal.h"  // <--- Added for Heal animation

// --- Declare the "default" parameter structs ---

// Firework Defaults
extern const firework_params_t FIREWORK_DEFAULT_BLUE;
extern const firework_params_t FIREWORK_DEFAULT_RED;
extern const firework_params_t FIREWORK_DEFAULT_MAGENTA;

// Spiral Defaults
extern const spiral_params_t SPIRAL_DEFAULT_RED;

// Circle Explosion Defaults
extern const circle_explosion_params_t EXPLOSION_DEFAULT_CYAN;

// Shield Defaults
extern const blue_shield_params_t SHIELD_DEFAULT_CONFIG; // <--- Fixed syntax error here

// Heal Defaults
extern const heal_params_t HEAL_DEFAULT_CONFIG; // <--- Added this

#endif