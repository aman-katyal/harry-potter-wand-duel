#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "ws2812.h"
#include "controller.h"
#include "healthbar.h"

#define WIDTH  16
#define HEIGHT 16

int main() {
    stdio_init_all();
    ws2812_init();

    hb_init(WIDTH, HEIGHT);
    sleep_ms(300);

    uint8_t spell = 0;

    while (true) {

        // 1. Apply damage BEFORE animation (only once per spell)
        switch (spell) {
            case 0:
            case 1:
            case 2:
                hb_update(-2);   // fireworks
                break;
            case 3:
                hb_update(-1);   // spiral
                break;
            case 4:
                hb_update(-3);   // explosion
                break;
        }

        // 2. If dead → show loser screen + reset
        if (hb_current() == 0) {
            loser_screen(WIDTH, HEIGHT);
            sleep_ms(800);
            hb_reset();
        }

        hb_draw();

        // 3. Now run the animation
        controller(spell, WIDTH, HEIGHT);

        // 4. Next spell
        spell++;
        if (spell > 4) spell = 0;

        sleep_ms(500);
    }
}
