#include "pico/stdlib.h"
#include "ws2812.h"
#include "firework.h"

#define WIDTH  16
#define HEIGHT 16

int main() {
    stdio_init_all();
    ws2812_init();

    while (true) {
        spiral(WIDTH, HEIGHT, 0, 255); // blue spiral
        spiral(WIDTH, HEIGHT, 1, 255); // red spiral
        spiral(WIDTH, HEIGHT, 2, 255); // magenta spiral
    }
}