// #ifndef FIREWORK_ANIMATION_H
// #define FIREWORK_ANIMATION_H

// #include <stdint.h>

// // Defines frame update timing (can be adjusted per call)
// #define FRAME_INTERVAL_US 9000

// // Function prototype
// // width, height: matrix size
// // color_mode: 0 = blue, 1 = red, 2 = magenta
// // intensity: integer multiplier for brightness (1–255)
// void run_firework_animation_with_color(int width, int height, int color_mode, int intensity);

// #endif

#ifndef FIREWORK_ANIMATION_H
#define FIREWORK_ANIMATION_H

#include <stdint.h>

#define FRAME_INTERVAL_US 9000 

void firework(int width, int height, int color_mode, int intensity);

#endif

