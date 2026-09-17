#ifndef CIRCLE_EXPLOSION_H
#define CIRCLE_EXPLOSION_H
#include <stdint.h>

typedef struct {
    int color_mode;
    int intensity;
    int num_frames;
    float growth_speed;
    float ring_thickness;
} circle_explosion_params_t;

void circle_explosion(int width, int height, const circle_explosion_params_t* params);

#endif