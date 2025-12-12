#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int color_mode;
    int intensity;
    int num_frames;
    float speed;
    float spiral_gap;
} spiral_params_t;

void spiral_start(const spiral_params_t* params);
bool spiral_update(void);