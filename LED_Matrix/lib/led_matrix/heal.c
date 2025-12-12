#include "heal.h"
#include "ws2812.h"
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include <math.h>
#include <stdlib.h>

#define HEAL_FRAME_INTERVAL_US 15000

static inline uint8_t clamp8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

void heal_run(int width, int height, const heal_params_t* params)
{
    int num_frames = params->num_frames;
    float master_bright = params->brightness / 255.0f;

    const float cx = (width - 1) / 2.0f;
    const float cy = (height - 1) / 2.0f;

    // Clear screen
    ws2812_clear();

    for (int frame = 0; frame < num_frames; frame++)
    {
        // Reverted Scale to 6.0 (Fuller size)
        float current_scale = 6.0f;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {

                // --- SHAPE LOGIC ---
                // Reverted X-Squeeze to 0.85f.
                // This restores the "taller" look without making it look flattened/squashed.
                float u = (x - cx) / (current_scale * 0.85f);
                
                // Y-Axis standard
                float v = -(y - (cy + 0.5f)) / current_scale; 

                // Heart Equation
                float u2 = u * u;
                float v2 = v * v;
                float term1 = u2 + v2 - 1.0f;
                float term2 = term1 * term1 * term1;
                float term3 = u2 * (v * v * v);
                
                float val = term2 - term3;

                // --- DRAWING LOGIC ---
                if (val <= 0.0f) {
                    // Inside the Heart -> Solid Red
                    uint8_t r = 255;
                    uint8_t g = 0;
                    uint8_t b = 0;

                    // Apply Master Brightness
                    r = clamp8((int)(r * master_bright));
                    g = clamp8((int)(g * master_bright));
                    b = clamp8((int)(b * master_bright));

                    ws2812_draw_pixel(x, y, width, height, r, g, b);
                }
                else {
                    // Background -> Black
                    ws2812_draw_pixel(x, y, width, height, 0, 0, 0);
                }
            }
        }

        ws2812_update();
        sleep_us(HEAL_FRAME_INTERVAL_US);
    }
    
    ws2812_clear();
}