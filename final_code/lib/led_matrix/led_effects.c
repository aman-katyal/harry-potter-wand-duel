#define _USE_MATH_DEFINES
#include <math.h> 
#include "led_effects.h"
#include "ws2812.h"

// These control how fast the animations run
#define FRAME_RATE 60
#define FRAME_INTERVAL_MS (1000 / FRAME_RATE)
#define BREATHING_SPEED 0.02f  
#define BURST_SPEED_MS 10       

// Tracks which effect is active and what color it uses
static effect_t current_effect = EFFECT_NONE;
static uint32_t current_color = 0;
static uint8_t current_r, current_g, current_b;
static absolute_time_t next_frame_time;

// Each effect keeps a little internal state so it can animate properly
static float breath_phase = 0.0f;
static int burst_position = -1; 
static absolute_time_t next_burst_move_time;

// Breathing Effect
// This fades the chosen color in and out using a smooth sine wave
static void update_breathing() {

    float brightness = (sinf(breath_phase) + 1.0f) / 2.0f;

    // Apply the brightness to the base color
    uint8_t r = (uint8_t)(current_r * brightness);
    uint8_t g = (uint8_t)(current_g * brightness);
    uint8_t b = (uint8_t)(current_b * brightness);

    ws2812_fill(r, g, b);

    breath_phase += BREATHING_SPEED;

    // Keep the value from growing forever
    if (breath_phase > 2 * M_PI) {
        breath_phase -= 2 * M_PI;
    }
}

// Burst Effect
// A single bright pixel moves across the LEDs with a fading tail behind it
static void update_burst() {

    // Only move the burst when the timer says it is time
    if (time_reached(next_burst_move_time)) {

        burst_position++;

        // Loop when we reach the end of the strip
        if (burst_position >= NUM_LEDS) {
            burst_position = 0;
        }

        next_burst_move_time = make_timeout_time_ms(BURST_SPEED_MS);

        // Clear everything first
        ws2812_fill(0, 0, 0);

        // Draw the burst pixel and a short tail behind it
        for (int i = 0; i < 5; ++i) {
            int pos = burst_position - i;

            if (pos >= 0) {
                // The tail gets dimmer as it gets older
                uint8_t tail_r = current_r / (i + 1);
                uint8_t tail_g = current_g / (i + 1);
                uint8_t tail_b = current_b / (i + 1);

                ws2812_set_pixel_color(pos, tail_r, tail_g, tail_b);
            }
        }
    }
}

// Sets up the timing for animations
void effects_init() {
    next_frame_time = get_absolute_time();
}

// Choose an effect and a color
void effects_set_mode(effect_t mode, uint32_t color) {
    current_effect = mode;
    current_color = color;

    current_g = (current_color >> 16) & 0xFF;
    current_r = (current_color >> 8) & 0xFF;
    current_b = (current_color) & 0xFF;

    // Reset any animation progress
    breath_phase = 0.0f;
    burst_position = -1;
    next_burst_move_time = get_absolute_time();
}

// Called repeatedly to animate the currently selected effect
void effects_update() {

    // Only update when it is time for a new frame
    if (!time_reached(next_frame_time)) {
        return;
    }
    next_frame_time = make_timeout_time_ms(FRAME_INTERVAL_MS);

    // Run whichever animation mode is active
    switch (current_effect) {

        case EFFECT_STATIC:
            ws2812_fill(current_r, current_g, current_b);
            break;

        case EFFECT_BREATHING:
            update_breathing();
            break;

        case EFFECT_BURST:
            update_burst();
            break;

        case EFFECT_NONE:
        default:
            ws2812_fill(0, 0, 0);
            break;
    }

    // Push the updated frame to the LEDs
    ws2812_update();
}
