#include "pico/stdlib.h"
#include "ws2812.h"
#include "firework.h"
#include "healthbar.h"

#define WIDTH  16
#define HEIGHT 16

int main() {
    stdio_init_all();
    ws2812_init();
    hb_init(WIDTH, HEIGHT);


    while (true) {

        spiral(WIDTH, HEIGHT, 0, 255);
        hb_update(-3);
        hb_draw();
        circle_explosion(WIDTH, HEIGHT, 1, 255);
        hb_update(-1);
        hb_draw(); 
        firework(WIDTH, HEIGHT, 2, 255); 
        hb_update(-1);
        hb_draw(); 
    }
}