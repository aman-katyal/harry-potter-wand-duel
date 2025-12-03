#pragma once
#include <stdint.h> 

typedef struct {
    int color_mode;
    int intensity;
    int num_frames;
    float speed;
    float spiral_gap;
} spiral_params_t;

void spiral(int width, int height, const spiral_params_t* params);