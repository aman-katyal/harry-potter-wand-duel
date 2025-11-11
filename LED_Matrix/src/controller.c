#include "controller.h"
#include "firework.h"
#include "ripple.h"
#include "ws2812.h"
#include "pico/stdlib.h"

void controller(int code) {
    // Example code: 2315 → color=2, anim=3, damage=15
    int color_mode = (code / 1000) % 10;  // 0–9 color
    int anim_id    = (code / 100) % 10;   // 0–9 animation
    int damage     = code % 100;          // 00–99 damage/intensity

    int intensity = damage * 10;
    if (intensity > 255) intensity = 255;

    const int WIDTH  = 16;
    const int HEIGHT = 16;

    switch (anim_id) {
        case 0:
            firework(WIDTH, HEIGHT, color_mode, intensity);
            break;

        case 1:
            ripple(color_mode, intensity);
            break;

        default:
            firework(WIDTH, HEIGHT, color_mode, intensity);
            break;
    }
}
