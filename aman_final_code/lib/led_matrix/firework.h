#ifndef FIREWORK_ANIMATION_H
#define FIREWORK_ANIMATION_H

#include <stdint.h>

#define FRAME_INTERVAL_US 9000 

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

void firework(int width, int height, const firework_params_t* params);

#endif