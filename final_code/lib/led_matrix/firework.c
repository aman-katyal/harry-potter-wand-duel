#include "firework.h"
#include "ws2812.h"
#include "pico/stdlib.h"
#include <math.h>
#include <stdlib.h>
#include "hardware/dma.h"

void firework(int width, int height, const firework_params_t* params) {
    
    int color_mode = params->color_mode % 3;
    int intensity  = params->intensity;

    const float cx = (width  - 1) / 2.0f;
    const float cy = (height - 1) / 2.0f;

    float radius         = 0.5f;
    const float max_radius = 7.0f;

    const float growth_speed = params->growth_speed;
    bool        expanding    = true;
    const int   spikes       = params->spikes;
    const int   fade_factor  = params->fade_factor;
    const float thinness     = params->thinness;
    const float core_radius  = params->core_radius;

    // Each spike can have slightly different brightness
    float ray_brightness[spikes];
    for (int i = 0; i < spikes; i++)
        ray_brightness[i] = 0.7f + (rand() % 30) / 100.0f;

    // Main animation loop
    for (int frame = 0; frame < params->num_frames; frame++) {

        // Fade the previous frame
        uint32_t* buf = ws2812_get_buffer();
        for (int i = 0; i < NUM_LEDS; ++i) {
            uint32_t color = buf[i];

            uint8_t r = (color >> 16) & 0xFF;
            uint8_t b = (color >> 8)  & 0xFF;
            uint8_t g =  color        & 0xFF;

            g = (uint8_t)((g * fade_factor) / 255);
            r = (uint8_t)((r * fade_factor) / 255);
            b = (uint8_t)((b * fade_factor) / 255);

            buf[i] = ((uint32_t)r << 16) | ((uint32_t)b << 8) | g;
        }

        // Draw the outer ring of spikes
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float dx = x - cx, dy = y - cy;
                float dist  = sqrtf(dx * dx + dy * dy);
                float angle = atan2f(dy, dx);
                if (angle < 0) angle += 2.0f * (float)M_PI;

                float segment   = (2.0f * (float)M_PI) / spikes;
                int   ray_idx   = (int)(angle / segment);
                float ray_angle = ray_idx * segment + segment * 0.5f;
                float delta     = fabsf(angle - ray_angle);
                if (delta > (float)M_PI) delta = fabsf(delta - 2.0f * (float)M_PI);

                if (delta < thinness && fabsf(dist - radius) < 0.6f) {
                    float fade       = 1.0f - fabsf(dist - radius);
                    float brightness = fade * ray_brightness[ray_idx] * (intensity / 255.0f);

                    uint8_t r = 0, g = 0, b = 0;
                    switch (color_mode) {
                        case 0:  // Blue
                            b = (uint8_t)(brightness * 220.0f);
                            break;
                        case 1:  // Red
                            r = (uint8_t)(brightness * 255.0f);
                            break;
                        case 2:  // Magenta
                        default:
                            r = (uint8_t)(brightness * 230.0f);
                            b = (uint8_t)(brightness * 160.0f);
                            break;
                    }

                    int index = (y % 2 == 0)
                        ? y * width + x
                        : y * width + (width - 1 - x);

                    ws2812_set_pixel_color(index, r, g, b);
                }
            }
        }

        // Draw a glowing core in the center
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                float dx = x - cx, dy = y - cy;
                float dist = sqrtf(dx * dx + dy * dy);

                if (dist <= core_radius) {
                    float fade       = 1.0f - (dist / core_radius);
                    float brightness = fade * (intensity / 255.0f);

                    uint8_t r = 0, g = 0, b = 0;
                    switch (color_mode) {
                        case 0:
                            b = (uint8_t)(brightness * 120.0f);
                            break;
                        case 1:
                            r = (uint8_t)(brightness * 150.0f);
                            break;
                        case 2:
                        default:
                            r = (uint8_t)(brightness * 160.0f);
                            b = (uint8_t)(brightness * 100.0f);
                            break;
                    }

                    int index = (y % 2 == 0)
                        ? y * width + x
                        : y * width + (width - 1 - x);

                    ws2812_set_pixel_color(index, r, g, b);
                }
            }
        }

        ws2812_update();
        sleep_us(FRAME_INTERVAL_US);

        // Grow and shrink the ring radius
        radius += (expanding ? growth_speed : -growth_speed);
        if (radius >= max_radius) expanding = false;
        if (radius <= 0.5f && !expanding) expanding = true;
    }

    // Fade everything out at the end
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
        sleep_us(FRAME_INTERVAL_US);
    }
}
