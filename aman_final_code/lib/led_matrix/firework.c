#include "firework.h"
#include "ws2812.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define MAX_SPIKES 64
#ifndef M_PI
#define M_PI 3.14159265f
#endif

// Internal State
static firework_params_t p;
static int frame_idx = 0;
static float radius = 0.0f;
static bool expanding = true;
static float ray_brightness[MAX_SPIKES];
static enum { FW_RUN, FW_FADE } state;

void firework_start(const firework_params_t* params) {
    // Copy params locally so we don't depend on the pointer
    memcpy(&p, params, sizeof(firework_params_t));
    
    frame_idx = 0;
    radius = 0.5f;
    expanding = true;
    state = FW_RUN;

    // Generate random spikes once
    int count = (p.spikes > MAX_SPIKES) ? MAX_SPIKES : p.spikes;
    for (int i = 0; i < count; i++) {
        ray_brightness[i] = 0.7f + (rand() % 30) / 100.0f;
    }
}

static void apply_fade(int factor) {
    uint32_t* buf = ws2812_get_buffer();
    for (int i = 0; i < NUM_LEDS; ++i) {
        uint32_t color = buf[i];
        uint8_t r = (color >> 16) & 0xFF;
        uint8_t b = (color >> 8)  & 0xFF;
        uint8_t g =  color        & 0xFF;

        r = (uint8_t)((r * factor) / 255);
        g = (uint8_t)((g * factor) / 255);
        b = (uint8_t)((b * factor) / 255);

        buf[i] = ((uint32_t)r << 16) | ((uint32_t)b << 8) | g;
    }
}

bool firework_update(void) {
    // 1. FADE PHASE
    if (state == FW_FADE) {
        apply_fade(p.fade_factor);
        ws2812_update();
        frame_idx++;
        return (frame_idx >= 30); // Done after 30 fade frames
    }

    // 2. RUN PHASE
    // Apply trail fade
    apply_fade(p.fade_factor);

    int width = 16; // Assuming 16x16, or pass this in if dynamic
    int height = 16;
    float cx = (width - 1) / 2.0f;
    float cy = (height - 1) / 2.0f;

    // Draw Rays
    int spike_count = (p.spikes > MAX_SPIKES) ? MAX_SPIKES : p.spikes;
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float dx = x - cx; 
            float dy = y - cy;
            float dist  = sqrtf(dx * dx + dy * dy);
            float angle = atan2f(dy, dx);
            if (angle < 0) angle += 2.0f * M_PI;

            float segment = (2.0f * M_PI) / spike_count;
            int ray_idx = (int)(angle / segment);
            float ray_angle = ray_idx * segment + segment * 0.5f;
            float delta = fabsf(angle - ray_angle);
            if (delta > M_PI) delta = fabsf(delta - 2.0f * M_PI);

            if (delta < p.thinness && fabsf(dist - radius) < 0.6f) {
                float fade = 1.0f - fabsf(dist - radius);
                float bright = fade * ray_brightness[ray_idx] * (p.intensity / 255.0f);
                
                uint8_t r=0, g=0, b=0;
                if (p.color_mode == 0) b = (uint8_t)(bright * 220.0f);
                else if (p.color_mode == 1) r = (uint8_t)(bright * 255.0f);
                else { r = (uint8_t)(bright * 230.0f); b = (uint8_t)(bright * 160.0f); }

                int index = (y % 2 == 0) ? y * width + x : y * width + (width - 1 - x);
                ws2812_set_pixel_color(index, r, g, b);
            }
        }
    }
    
    // Draw Core
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float dx = x - cx, dy = y - cy;
            float dist = sqrtf(dx*dx + dy*dy);
            if (dist <= p.core_radius) {
                float fade = 1.0f - (dist / p.core_radius);
                float bright = fade * (p.intensity / 255.0f);
                uint8_t r=0, g=0, b=0;
                
                if (p.color_mode == 0) b = (uint8_t)(bright * 120.0f);
                else if (p.color_mode == 1) r = (uint8_t)(bright * 150.0f);
                else { r = (uint8_t)(bright * 160.0f); b = (uint8_t)(bright * 100.0f); }

                int index = (y % 2 == 0) ? y * width + x : y * width + (width - 1 - x);
                ws2812_set_pixel_color(index, r, g, b);
            }
        }
    }

    ws2812_update();

    // Logic
    radius += (expanding ? p.growth_speed : -p.growth_speed);
    if (radius >= 7.0f) expanding = false;
    if (radius <= 0.5f && !expanding) expanding = true;

    frame_idx++;
    if (frame_idx >= p.num_frames) {
        state = FW_FADE; // Switch to fade out
        frame_idx = 0;
    }
    
    return false; // Not done yet
}