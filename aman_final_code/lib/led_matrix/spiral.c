#include "spiral.h"
#include "ws2812.h"
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265f
#endif

static spiral_params_t p;
static int frame_idx = 0;
static float angle_offset = 0.0f;
static enum { SP_RUN, SP_FADE } state;

void spiral_start(const spiral_params_t* params) {
    memcpy(&p, params, sizeof(spiral_params_t));
    frame_idx = 0;
    angle_offset = 0.0f;
    state = SP_RUN;
}

static void apply_fade(int factor) {
    uint32_t* buf = ws2812_get_buffer();
    for (int i = 0; i < NUM_LEDS; ++i) {
        uint32_t c = buf[i];
        uint8_t r = (c >> 16) & 0xFF;
        uint8_t b = (c >> 8)  & 0xFF;
        uint8_t g =  c        & 0xFF;
        r = (uint8_t)((r * factor) / 255);
        g = (uint8_t)((g * factor) / 255);
        b = (uint8_t)((b * factor) / 255);
        buf[i] = ((uint32_t)r << 16) | ((uint32_t)b << 8) | g;
    }
}

bool spiral_update(void) {
    if (state == SP_FADE) {
        apply_fade(220); // Fixed fade for spiral
        ws2812_update();
        frame_idx++;
        return (frame_idx >= 30);
    }

    // RUN state
    ws2812_clear(); // Spiral clears frame every time unlike firework

    int width = 16, height = 16;
    float cx = (width - 1) / 2.0f;
    float cy = (height - 1) / 2.0f;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float dx = x - cx;
            float dy = y - cy;
            float dist = sqrtf(dx * dx + dy * dy);
            float angle = atan2f(dy, dx);
            if (angle < 0) angle += 2.0f * M_PI;

            float spiral_pos = angle + dist / p.spiral_gap + angle_offset;
            float wave = fmodf(spiral_pos, 2.0f * M_PI);
            float brightness = cosf(wave) * 0.5f + 0.5f;

            if (brightness > 0.6f) {
                uint8_t r=0, g=0, b=0;
                int imode = p.color_mode % 3;
                if (imode == 0) b = (uint8_t)(brightness * p.intensity);
                else if (imode == 1) r = (uint8_t)(brightness * p.intensity);
                else { r = (uint8_t)(brightness * p.intensity); b = (uint8_t)(brightness * p.intensity * 0.6f); }

                int index = (y % 2 == 0) ? y * width + x : y * width + (width - 1 - x);
                ws2812_set_pixel_color(index, r, g, b);
            }
        }
    }

    ws2812_update();

    angle_offset += p.speed;
    if (angle_offset > 2.0f * M_PI) angle_offset -= 2.0f * M_PI;

    frame_idx++;
    if (frame_idx >= p.num_frames) {
        state = SP_FADE;
        frame_idx = 0;
    }

    return false;
}