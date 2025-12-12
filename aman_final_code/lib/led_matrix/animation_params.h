#ifndef ANIMATION_PARAMS_H
#define ANIMATION_PARAMS_H

// Include all the animation headers that define the structs
#include "firework.h"
#include "spiral.h" // <-- THIS IS THE MISSING INCLUDE
#include "circle_explosion.h"

// --- Declare the "default" parameter structs ---

// Firework Defaults
extern const firework_params_t FIREWORK_DEFAULT_BLUE;
extern const firework_params_t FIREWORK_DEFAULT_RED;
extern const firework_params_t FIREWORK_DEFAULT_MAGENTA;

// Spiral Defaults
extern const spiral_params_t SPIRAL_DEFAULT_RED; // This will now be recognized

// Circle Explosion Defaults
extern const circle_explosion_params_t EXPLOSION_DEFAULT_CYAN;

#endif