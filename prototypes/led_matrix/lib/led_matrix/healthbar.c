#include "healthbar.h"
#include "ws2812.h"
#include "pico/stdlib.h"      // REQUIRED for sleep_ms()
#include <stdint.h>

// from ws2812.c (must exist)
extern uint32_t led_buffer[];

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
    HB_WIDTH  = width;
    HB_HEIGHT = height;

    max_health = HB_WIDTH;
    health     = max_health;
}

int hb_current(void)
{
    return health;
}

void hb_reset(void)
{
    health = max_health;
}

// --------------------------------------------
// LOSER SCREEN (big "L" pattern)
// --------------------------------------------
void loser_screen(int width, int height) 
{
    // Clear entire display
    ws2812_fill(0, 0, 0); // Changed this to use the fill helper

    int pattern[16][16] = {0};

    // ----------------------------
    // LEFT EYE "X"
    // ----------------------------
    pattern[5][3] = 1;
    pattern[5][4] = 1; 
    pattern[6][2] = 1;
    pattern[6][4] = 1;
    pattern[4][2] = 1;
    pattern[4][5] = 1;
    pattern[7][1] = 1;
    pattern[7][6] = 1;
    pattern[3][1] = 1;
    pattern[3][6] = 1;

    // ----------------------------
    // RIGHT EYE "X"
    // ----------------------------
    pattern[5][10] = 1;
    pattern[5][11] = 1;
    pattern[6][9]  = 1;
    pattern[6][12] = 1;
    pattern[4][9]  = 1;
    pattern[4][12] = 1;
    pattern[7][8]  = 1;
    pattern[7][13] = 1;
    pattern[3][8]  = 1;
    pattern[3][13] = 1;

    // ----------------------------
    // MOUTH
    // ----------------------------
    pattern[12][4]  = 1;
    pattern[12][5]  = 1;
    pattern[12][6]  = 1;
    pattern[12][7]  = 1;
    pattern[12][8]  = 1;
    pattern[12][9]  = 1;
    pattern[12][10] = 1;
    pattern[12][11] = 1;

    // ----------------------------
    // DRAW PATTERN
    // ----------------------------
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {

            if (!pattern[y][x])
                continue;

            int index =
                (y % 2 == 0)
                ? (y * width + x)
                : (y * width + (width - 1 - x));

            ws2812_set_pixel_color(index, 255, 0, 0);
        }
    }

    ws2812_update();
}


// --------------------------------------------
// PRIMARY HEALTH UPDATE LOGIC
// --------------------------------------------
void hb_update(int delta)
{
    health += delta;

    if (health < 0)
        health = 0;
    
    if (health > max_health)
        health = max_health;
}

// --------------------------------------------
// DRAW HEALTH BAR
// --------------------------------------------
void hb_draw(void)
{
    // --- THIS IS THE FIX ---
    // First, clear the entire buffer
    ws2812_fill(0, 0, 0);
    // -----------------------

    int row0 = 0;
    int row1 = 1;

    for (int x = 0; x < HB_WIDTH; x++)
    {
        int idx0 = zigzag_index(row0, x);
        int idx1 = zigzag_index(row1, x);

        if (x < health)
        {
            ws2812_set_pixel_color(idx0, 255, 0, 0);
            ws2812_set_pixel_color(idx1, 255, 0, 0);
        }
        else
        {
            // The fill function already set these to 0,
            // so we don't strictly need this 'else' block,
            // but it's good practice.
            ws2812_set_pixel_color(idx0, 0, 0, 0);
            ws2812_set_pixel_color(idx1, 0, 0, 0);
        }
    }

    ws2812_update();
}