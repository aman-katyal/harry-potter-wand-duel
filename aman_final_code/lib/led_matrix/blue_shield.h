#ifndef BLUE_SHIELD_H
#define BLUE_SHIELD_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int num_frames;       // How long the animation runs
    int brightness;       // Overall brightness (0-255)
} blue_shield_params_t;

// Function prototype
void blue_shield_run(int width, int height, const blue_shield_params_t* params);

#endif