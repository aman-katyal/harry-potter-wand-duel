#ifndef HEAL_H
#define HEAL_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int num_frames;       // Duration (e.g., 200)
    int brightness;       // Max brightness (0-255)
} heal_params_t;

// Run the healing heart animation
void heal_run(int width, int height, const heal_params_t* params);

#endif