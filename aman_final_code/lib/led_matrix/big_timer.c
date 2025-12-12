#include "big_timer.h"
#include "pico/stdlib.h"
#include "ws2812.h"
#include <stdint.h>

#define LED_WIDTH   16
#define LED_HEIGHT  16

// pure red in your RBG packing (R in high byte)
static inline uint32_t make_red(uint8_t r) {
    return ((uint32_t)r << 16);
}

// 3x8 digit font
// bit2 = left, bit0 = right
// Top and bottom rows are blank to make digits 2 LEDs taller
static const uint8_t DIGITS[10][8] = {
    // 0
    {
        0b000,
        0b111,
        0b101,
        0b101,
        0b101,
        0b101,
        0b111,
        0b000
    },
    // 1
    {
        0b000,
        0b010,
        0b110,
        0b010,
        0b010,
        0b010,
        0b111,
        0b000
    },
    // 2
    {
        0b000,
        0b111,
        0b001,
        0b011,
        0b110,
        0b100,
        0b111,
        0b000
    },
    // 3
    {
        0b000,
        0b111,
        0b001,
        0b010,
        0b001,
        0b001,
        0b111,
        0b000
    },
    // 4
    {
        0b000,
        0b101,
        0b101,
        0b111,
        0b001,
        0b001,
        0b001,
        0b000
    },
    // 5
    {
        0b000,
        0b111,
        0b100,
        0b111,
        0b001,
        0b001,
        0b111,
        0b000
    },
    // 6
    {
        0b000,
        0b011,
        0b100,
        0b111,
        0b101,
        0b101,
        0b111,
        0b000
    },
    // 7
    {
        0b000,
        0b111,
        0b001,
        0b010,
        0b010,
        0b100,
        0b100,
        0b000
    },
    // 8
    {
        0b000,
        0b111,
        0b101,
        0b111,
        0b101,
        0b101,
        0b111,
        0b000
    },
    // 9
    {
        0b000,
        0b111,
        0b101,
        0b101,
        0b111,
        0b001,
        0b110,
        0b000
    }
};

// Standard zigzag (serpentine) index
static int zigzag_index(int row, int col) {
    if (row % 2 == 0) {
        return row * LED_WIDTH + col;
    } else {
        return row * LED_WIDTH + (LED_WIDTH - 1 - col);
    }
}

static void clear_matrix(uint32_t *buf) {
    for (int i = 0; i < LED_WIDTH * LED_HEIGHT; i++) {
        buf[i] = 0;
    }
}

// GLOBAL HORIZONTAL FLIP:
// We flip all columns here so logical column 0 becomes physical right side.
static void set_pixel(uint32_t *buf, int r, int c, uint32_t color) {
    if (r < 0 || r >= LED_HEIGHT || c < 0 || c >= LED_WIDTH) return;

    int flipped_col = LED_WIDTH - 1 - c;  // global horizontal flip
    int idx = zigzag_index(r, flipped_col);
    buf[idx] = color;
}

// Draw 3x8 digit with NORMAL (non-mirrored) mapping.
// bit2 = left, bit1 = middle, bit0 = right.
static void draw_digit(uint32_t *buf, int digit, int top, int left, uint32_t color) {
    if (digit < 0 || digit > 9) return;

    for (int r = 0; r < 8; r++) {
        uint8_t row_bits = DIGITS[digit][r];
        for (int c = 0; c < 3; c++) {
            if (row_bits & (1 << (2 - c))) {
                set_pixel(buf, top + r, left + c, color);
            }
        }
    }
}

// Colon with a clear gap between minutes and seconds digits
static void draw_colon(uint32_t *buf, int top, int col, uint32_t color) {
    set_pixel(buf, top + 2, col, color);
    set_pixel(buf, top + 5, col, color);
}

// One frame of MM:SS
static void draw_timer_frame(uint32_t *buf, int t, uint32_t color) {
    int minutes = t / 60;
    int seconds = t % 60;

    int m1 = minutes / 10;
    int m0 = minutes % 10;
    int s1 = seconds / 10;
    int s0 = seconds % 10;

    clear_matrix(buf);

    // 8 tall, start at row 4 → rows 4..11
    int top = 4;

    // Layout (logical, before global flip):
    //
    // M1 _ M0 : _ S1 _ S0
    // 0..2 3 4..6 7 8 9..11 12 13..15
    int col_m1    = 0;   // 0,1,2
    int col_m0    = 4;   // 4,5,6
    int col_colon = 7;   // colon (gaps at 3 and 8)
    int col_s1    = 9;   // 9,10,11
    int col_s0    = 13;  // 13,14,15 (gap at 12)

    draw_digit(buf, m1, top, col_m1,    color);  // minutes tens
    draw_digit(buf, m0, top, col_m0,    color);  // minutes ones
    draw_colon(buf, top, col_colon,     color);
    draw_digit(buf, s1, top, col_s1,    color);  // seconds tens
    draw_digit(buf, s0, top, col_s0,    color);  // seconds ones
}



void run_big_timer(int start_seconds) {
    uint32_t *buf = ws2812_get_buffer();
    uint32_t red = make_red(40);

    for (int t = start_seconds; t >= 0; t--) {
        draw_timer_frame(buf, t, red);
        ws2812_update();
        sleep_ms(1000);
    }

    clear_matrix(buf);
    ws2812_update();
}
