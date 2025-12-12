#include "circle_explosion.h"
#include "ws2812.h"
#include <math.h>
#include <string.h>

static circle_explosion_params_t p;
static int frame_idx = 0;
static float radius = 0.5f;
static bool expanding = true;
static enum { CE_RUN, CE_FADE } state;

void circle_explosion_start(const circle_explosion_params_t* params) {
    memcpy(&p, params, sizeof(circle_explosion_params_t));
    frame_idx = 0;
    radius = 0.5f;
    expanding = true;
    state = CE_RUN;
}

static void apply_fade(int factor) {
    uint32_t* buf = ws2812_get_buffer();
    for (int i = 0; i < NUM_LEDS; i++) {
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

bool circle_explosion_update(void) {
    if (state == CE_FADE) {
        apply_fade(225);
        ws2812_update();
        frame_idx++;
        return (frame_idx >= 30);
    }

    // RUN PHASE
    apply_fade(225); // Trail fade

    int width = 16, height = 16;
    float cx = (width - 1) / 2.0f;
    float cy = (height - 1) / 2.0f;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy);

            if (fabsf(dist - radius) < p.ring_thickness) {
                float fade = 1.0f - fabsf(dist - radius);
                float bright = fade * (p.intensity / 255.0f);
                uint8_t r=0, g=0, b=0;

                int imode = p.color_mode % 3;
                if (imode == 0) b = (uint8_t)(bright * 255.0f);
                else if (imode == 1) r = (uint8_t)(bright * 255.0f);
                else { r = (uint8_t)(bright * 180.0f); b = (uint8_t)(bright * 200.0f); }

                int index = (y % 2 == 0) ? y * width + x : y * width + (width - 1 - x);
                ws2812_set_pixel_color(index, r, g, b);
            }
        }
    }

    ws2812_update();

    radius += (expanding ? p.growth_speed : -p.growth_speed);
    if (radius >= 7.0f) expanding = false;
    if (radius <= 0.5f && !expanding) expanding = true;

    frame_idx++;
    if (frame_idx >= p.num_frames) {
        state = CE_FADE;
        frame_idx = 0;
    }
    
    return false;
}