#include "healthbar.h"
#include "ws2812.h"
#include "pico/stdlib.h"     
#include <stdint.h>

extern uint32_t led_buffer[];

static int HB_WIDTH;
static int HB_HEIGHT;

static int health;
static int max_health;

// Convert row and col into the correct LED index
// The LED matrix goes from left right on one row, then right to left on the next.
// This helper hides that zigzag behavior so nobody has to re-think it every time.
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


// LOSER SCREEN Shows a big angry face when HP hits zero
void loser_screen(int width, int height) 
{
    // WS2812 LEDs need a short pause to “reset” their protocol timing.
    // Without this, effects bleed into each other and look glitchy.
    sleep_ms(50); 

    ws2812_fill(0, 0, 0); 

    // Store a simple 16×16 pattern for the face.
    // 0 = off, 1 = turn that pixel red.
    uint8_t pattern[16][16] = {0};

    // LEFT EYE "X"
    pattern[5][3] = 1; pattern[5][4] = 1; 
    pattern[6][2] = 1; pattern[6][4] = 1;
    pattern[4][2] = 1; pattern[4][5] = 1;
    pattern[7][1] = 1; pattern[7][6] = 1;
    pattern[3][1] = 1; pattern[3][6] = 1;

    // RIGHT EYE "X"
    pattern[5][10] = 1; pattern[5][11] = 1;
    pattern[6][9]  = 1; pattern[6][12] = 1;
    pattern[4][9]  = 1; pattern[4][12] = 1;
    pattern[7][8]  = 1; pattern[7][13] = 1;
    pattern[3][8]  = 1; pattern[3][13] = 1;

    // MOUTH 
    pattern[12][4]  = 1; pattern[12][5]  = 1;
    pattern[12][6]  = 1; pattern[12][7]  = 1;
    pattern[12][8]  = 1; pattern[12][9]  = 1;
    pattern[12][10] = 1; pattern[12][11] = 1;

    // Draw the face on the LED matrix
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {

            // Skip pixels that should stay black.
            if (!pattern[y][x])
                continue;

            // Convert row and col to physical LED index with zigzag layout.
            int index = zigzag_index(y, x);

            // Draw this pixel in red.
            ws2812_set_pixel_color(index, 255, 0, 0);
        }
    }

    // Push the updated frame out to the LEDs.
    ws2812_update();
}

// Adjust the player's health by the given amount.
// Keeps health clamped between 0 and max.
void hb_update(int delta)
{
    health += delta;

    if (health < 0)
        health = 0;
    
    if (health > max_health)
        health = max_health;
}

// Draw the actual red health bar across the top two rows.
void hb_draw(void)
{
    // Start fresh every frame so the bar doesn’t leave old leds on.
    ws2812_fill(0, 0, 0);

    int row0 = 0;
    int row1 = 1;

    for (int x = 0; x < HB_WIDTH; x++)
    {
        int idx0 = zigzag_index(row0, x);
        int idx1 = zigzag_index(row1, x);

        if (x < health)
        {
            // This part of the bar is still up
            ws2812_set_pixel_color(idx0, 255, 0, 0);
            ws2812_set_pixel_color(idx1, 255, 0, 0);
        }
        else
        {
            // Past the current health → keep it dark
            ws2812_set_pixel_color(idx0, 0, 0, 0);
            ws2812_set_pixel_color(idx1, 0, 0, 0);
        }
    }

    // Send the finished health bar to the LEDs.
    ws2812_update();
}
