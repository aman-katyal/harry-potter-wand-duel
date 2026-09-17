#define _USE_MATH_DEFINES
#include <math.h> // For the sine wave in the breathing effect
#include "led_effects.h"
#include "ws2812.h"

// --- Animation Parameters ---
#define FRAME_RATE 60
#define FRAME_INTERVAL_MS (1000 / FRAME_RATE)
#define BREATHING_SPEED 0.02f // Controls how fast the breathing happens
#define BURST_SPEED_MS 10     // How many ms between pixel moves

// --- Module State Variables ---
static effect_t current_effect = EFFECT_NONE;
static uint32_t current_color = 0;
static uint8_t current_r, current_g, current_b;
static absolute_time_t next_frame_time;

// --- Effect-specific State ---
static float breath_phase = 0.0f;
static int burst_position = -1; // -1 means the burst is "off-screen"
static absolute_time_t next_burst_move_time;

// --- Private Animation Functions ---

static void update_breathing() {
    // Use a sine wave for a smooth up/down brightness curve
    float brightness = (sinf(breath_phase) + 1.0f) / 2.0f;

    uint8_t r = (uint8_t)(current_r * brightness);
    uint8_t g = (uint8_t)(current_g * brightness);
    uint8_t b = (uint8_t)(current_b * brightness);

    ws2812_fill(r, g, b);

    breath_phase += BREATHING_SPEED;
    // Keep phase from growing infinitely large
    if (breath_phase > 2 * M_PI) {
        breath_phase -= 2 * M_PI;
    }
}

static void update_burst() {
    // Check if it's time to move the burst pixel
    if (time_reached(next_burst_move_time)) {
        burst_position++;
        if (burst_position >= NUM_LEDS) {
            burst_position = 0; // Loop the burst
        }
        next_burst_move_time = make_timeout_time_ms(BURST_SPEED_MS);

        // Fill the buffer with black first
        ws2812_fill(0, 0, 0);

        // Draw the main pixel and a fading tail
        for (int i = 0; i < 5; ++i) {
            int pos = burst_position - i;
            if (pos >= 0) {
                // Dim the color for the tail
                uint8_t tail_r = current_r / (i + 1);
                uint8_t tail_g = current_g / (i + 1);
                uint8_t tail_b = current_b / (i + 1);
                ws2812_set_pixel_color(pos, tail_r, tail_g, tail_b);
            }
        }
    }
}

// --- Public API Functions ---

void effects_init() {
    next_frame_time = get_absolute_time();
}

void effects_set_mode(effect_t mode, uint32_t color) {
    current_effect = mode;
    current_color = color;
    // Unpack color for easier use in animations
    current_g = (current_color >> 16) & 0xFF;
    current_r = (current_color >> 8) & 0xFF;
    current_b = (current_color) & 0xFF;

    // Reset state when changing effects
    breath_phase = 0.0f;
    burst_position = -1; // Start burst off-screen
    next_burst_move_time = get_absolute_time();
}

void effects_update() {
    // Check if it's time to render the next frame
    if (!time_reached(next_frame_time)) {
        return;
    }
    next_frame_time = make_timeout_time_ms(FRAME_INTERVAL_MS);

    // Run the logic for the currently selected effect
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
            ws2812_fill(0, 0, 0); // Off
            break;
    }

    // After the buffer has been prepared by the effect, push it to the LEDs
    ws2812_update();
}