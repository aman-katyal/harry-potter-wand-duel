#ifndef BLUE_SHIELD_H
#define BLUE_SHIELD_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int num_frames;       // Duration (e.g., 500)
    int brightness;       // Overall brightness scaler (0-255)
} blue_shield_params_t;

// Run the shield animation
void blue_shield_run(int width, int height, const blue_shield_params_t* params);

#endif