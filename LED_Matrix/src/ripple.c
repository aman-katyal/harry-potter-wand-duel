#include "ripple.h"
#include "ws2812.h"
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include <math.h>

#define WIDTH  16
#define HEIGHT 16
#define FRAME_INTERVAL_US 8000  

void ripple(int color_mode, int intensity) {
    const float cx = (WIDTH - 1) / 2.0f;
    const float cy = (HEIGHT - 1) / 2.0f;
    const float max_radius = 8.0f;
    const float growth_speed = 0.6f;  
    const float fade_speed = 0.92f;   
    const float core_brightness = 1.3f;

    float radius = 0.0f;

    
    while (radius < max_radius) {
        uint32_t* buf = ws2812_get_buffer();

        
        for (int i = 0; i < NUM_LEDS; ++i) {
            uint32_t color = buf[i];
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;
            r *= fade_speed;
            g *= fade_speed;
            b *= fade_speed;
            buf[i] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }

        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                float dx = x - cx, dy = y - cy;
                float dist = sqrtf(dx * dx + dy * dy);

                float diff = fabsf(dist - radius);
                if (diff < 0.5f) {
                    float brightness = (1.0f - diff) * (intensity / 255.0f) * core_brightness;
                    uint8_t r = 0, g = 0, b = 0;

                    if (color_mode == 0) b = (uint8_t)(brightness * 255.0f);
                    else if (color_mode == 1) r = (uint8_t)(brightness * 255.0f);
                    else if (color_mode == 2) { r = brightness * 255; b = brightness * 180; } 
                    else if (color_mode == 3) { r = brightness * 255; g = brightness * 255; }
                    else g = brightness * 255;

                    int index = (y % 2 == 0)
                        ? y * WIDTH + x
                        : y * WIDTH + (WIDTH - 1 - x); // <-- This was the line with the typo
                    ws2812_set_pixel_color(index, r, g, b);
                }
            }
        }

        ws2812_update();
        sleep_us(FRAME_INTERVAL_US);
        radius += growth_speed;
    }

    for (int frame = 0; frame < 30; frame++) {
        uint32_t* buf = ws2812_get_buffer();
        for (int i = 0; i < NUM_LEDS; ++i) {
            uint32_t color = buf[i];
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8) & 0xFF;
            uint8_t b = color & 0xFF;
            r *= fade_speed;
            g *= fade_speed;
            b *= fade_speed;
            buf[i] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }
        ws2812_update();
        sleep_us(FRAME_INTERVAL_US);
    }
}