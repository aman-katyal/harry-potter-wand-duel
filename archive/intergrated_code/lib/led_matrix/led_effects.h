#ifndef LED_EFFECTS_H
#define LED_EFFECTS_H

#include "pico/stdlib.h"

// A simple way to pack colors
#define COLOR_GRB(g, r, b) ((uint32_t)(g << 16) | (uint32_t)(r << 8) | (b))

// An enum to define our effects in a readable way
typedef enum {
    EFFECT_NONE,
    EFFECT_STATIC,
    EFFECT_BREATHING,
    EFFECT_BURST
} effect_t;

void effects_init();
void effects_set_mode(effect_t mode, uint32_t color);
void effects_update();

#endif // LED_EFFECTS_H