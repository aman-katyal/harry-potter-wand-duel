#ifndef CIRCLE_EXPLOSION_H
#define CIRCLE_EXPLOSION_H
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int color_mode;
    int intensity;
    int num_frames;
    float growth_speed;
    float ring_thickness;
} circle_explosion_params_t;

void circle_explosion_start(const circle_explosion_params_t* params);
bool circle_explosion_update(void);

#endif