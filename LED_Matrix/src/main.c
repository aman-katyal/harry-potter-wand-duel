#include "pico/stdlib.h"
#include "ws2812.h"
#include "firework_animation.h"

#define WIDTH  16
#define HEIGHT 16

int main() {
    stdio_init_all();
    ws2812_init();

    while (true) {
        run_firework_animation_with_color(WIDTH, HEIGHT, 0, 255); // Blue
        run_firework_animation_with_color(WIDTH, HEIGHT, 1, 180); // Red (medium brightness)
        run_firework_animation_with_color(WIDTH, HEIGHT, 2, 255);
    }
}