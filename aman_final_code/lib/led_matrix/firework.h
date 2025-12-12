#ifndef FIREWORK_ANIMATION_H
#define FIREWORK_ANIMATION_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int color_mode;
    int intensity;
    int num_frames;
    float growth_speed;
    int spikes;
    int fade_factor;
    float thinness;
    float core_radius;
} firework_params_t;

// Setup the animation state (non-blocking)
void firework_start(const firework_params_t* params);

// Draw one frame. Returns true when animation is complete.
bool firework_update(void);

#endif