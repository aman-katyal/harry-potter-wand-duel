#include "circle_explosion.h"
#include "ws2812.h"
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include <math.h>
#include <stdlib.h>

#define CIRCLE_FRAME_INTERVAL_US 15000

void circle_explosion(int width, int height, const circle_explosion_params_t* params)
{
    // Read parameters from struct
    int color_mode = params->color_mode % 3;
    int intensity = params->intensity;
    const float growth_speed = params->growth_speed;
    const float ring_thickness = params->ring_thickness;

    const float cx = (width  - 1) / 2.0f;
    const float cy = (height - 1) / 2.0f;

    float radius = 0.5f;
    const float max_radius   = 7.0f;
    bool expanding = true;

    // This fade_factor is for the per-frame fade, not the final fade-out
    const int fade_factor = 225;

    for (int frame = 0; frame < params->num_frames; frame++)
    {
        // --- Step 1: Fade previous pixels ---
        // We act directly on the buffer here for speed. 
        // Rotation doesn't matter for fading (it just dims whatever led is at index i).
        uint32_t* buf = ws2812_get_buffer();

        for (int i = 0; i < NUM_LEDS; i++) {
            uint32_t c = buf[i];
            uint8_t r = (c >> 16) & 0xFF;
            uint8_t b = (c >> 8)  & 0xFF;
            uint8_t g =  c        & 0xFF;

            r = (uint8_t)((r * fade_factor) / 255);
            g = (uint8_t)((g * fade_factor) / 255);
            b = (uint8_t)((b * fade_factor) / 255);

            buf[i] = ((uint32_t)r << 16) | ((uint32_t)b << 8) | g;
        }

        // --- Step 2: Draw expanding circle ring ---
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {

                float dx = x - cx;
                float dy = y - cy;
                float dist = sqrtf(dx*dx + dy*dy);

                if (fabsf(dist - radius) < ring_thickness)
                {
                    float fade = 1.0f - fabsf(dist - radius);
                    float brightness = fade * (intensity / 255.0f);

                    uint8_t r = 0, g = 0, b = 0;

                    switch (color_mode) {
                        case 0:  // Blue ring
                            b = (uint8_t)(brightness * 255.0f);
                            break;
                        case 1:  // Red ring
                            r = (uint8_t)(brightness * 255.0f);
                            break;
                        case 2:  // Cyan-ish/Magenta hybrid
                        default:
                            r = (uint8_t)(brightness * 180.0f);
                            b = (uint8_t)(brightness * 200.0f);
                            break;
                    }

                    // UPDATED: Use centralized driver logic
                    // This handles ZigZag and Rotation automatically
                    ws2812_draw_pixel(x, y, width, height, r, g, b);
                }
            }
        }

        // --- Step 3: Send to LEDs via DMA ---
        ws2812_update();
        sleep_us(CIRCLE_FRAME_INTERVAL_US);

        // --- Step 4: Animate radius ---
        radius += (expanding ? growth_speed : -growth_speed);

        if (radius >= max_radius)
            expanding = false;

        if (radius <= 0.5f && !expanding)
            expanding = true;
    }

    // --- Fade-out loop ---
    for (int frame = 0; frame < 30; frame++) {
        uint32_t* buf = ws2812_get_buffer();
        for (int i = 0; i < NUM_LEDS; ++i) {
            uint32_t color = buf[i];
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t b = (color >> 8)  & 0xFF;
            uint8_t g =  color        & 0xFF;
            r = (uint8_t)((r * fade_factor) / 255);
            g = (uint8_t)((g * fade_factor) / 255);
            b = (uint8_t)((b * fade_factor) / 255);
            buf[i] = ((uint32_t)r << 16) | ((uint32_t)b << 8) | g;
        }
        ws2812_update();
        sleep_us(CIRCLE_FRAME_INTERVAL_US);
    }
}