#include "healthbar.h"
#include "ws2812.h"    // <-- REQUIRED for ws2812_set_pixel_color()
#include <stdint.h>

extern uint32_t led_buffer[];   // your main LED buffer

static int HB_WIDTH;
static int HB_HEIGHT;

static int health;
static int max_health;

static int zigzag_index(int row, int col)
{
    if (row % 2 == 0)
        return row * HB_WIDTH + col;
    return row * HB_WIDTH + (HB_WIDTH - 1 - col);
}

void hb_init(int width, int height)
{
    HB_WIDTH = width;
    HB_HEIGHT = height;

    max_health = HB_WIDTH;   
    health = max_health;
}

void hb_update(int delta)
{
    health += delta;

    if (health < 0) health = 0;
    if (health > max_health) health = max_health;
}

void hb_draw(void)
{
    int row0 = 0;
    int row1 = 1;

    for (int x = 0; x < HB_WIDTH; x++) {

        int idx0 = zigzag_index(row0, x);
        int idx1 = zigzag_index(row1, x);

        if (x < health) {
            ws2812_set_pixel_color(idx0, 255, 0, 0);
            ws2812_set_pixel_color(idx1, 255, 0, 0);
        } 
        else {
            ws2812_set_pixel_color(idx0, 0, 0, 0);
            ws2812_set_pixel_color(idx1, 0, 0, 0);
        }
    }

    // OPTIONAL: push to LEDs immediately
    // ws2812_update();
}

int hb_get_health(void)
{
    return health;
}
