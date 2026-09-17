#include "blue_shield.h"
#include "ws2812.h"
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include <math.h>
#include <stdlib.h>

#define SHIELD_FRAME_INTERVAL_US 12000
#define PI_FLOAT 3.14159f

static inline uint8_t clamp8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

void blue_shield_run(int width, int height, const blue_shield_params_t* params)
{
    int num_frames = params->num_frames;
    float master_bright = params->brightness / 255.0f;

    const float cx = (width - 1) / 2.0f;       

    // --- GEOMETRY SETTINGS ---
    const float max_width_half = 7.0f; 
    const float top_peak_y = 0.5f; 
    const float top_dip_depth = 1.8f; 
    const float bottom_y = 15.5f;

    const int fade_factor = 200; 

    for (int frame = 0; frame < num_frames; frame++)
    {
        uint32_t* buf = ws2812_get_buffer();

        // --- Step 1: Fade previous frame ---
        // (We manipulate the buffer directly for fading, which is faster and rotation-agnostic)
        for (int i = 0; i < NUM_LEDS; i++) {
            uint32_t c = buf[i];
            uint8_t r = (c >> 16) & 0xFF;
            uint8_t b = (c >> 8)  & 0xFF; 
            uint8_t g =  c        & 0xFF;

            r = (uint8_t)((r * fade_factor) / 255);
            b = (uint8_t)((b * fade_factor) / 255);
            g = (uint8_t)((g * fade_factor) / 255);

            buf[i] = ((uint32_t)r << 16) | ((uint32_t)b << 8) | g;
        }

        // --- Step 2: Draw the Shield ---
        for (int y = 0; y < height; y++) {
            
            // Shape Logic
            float pct_down = (y - top_peak_y) / (bottom_y - top_peak_y);
            if (pct_down < 0.0f) pct_down = 0.0f;
            if (pct_down > 1.0f) pct_down = 1.0f;

            float base_curve = cosf(pct_down * 1.5708f); 
            if (base_curve < 0.0f) base_curve = 0.0f;
            float allowed_width = max_width_half * powf(base_curve, 0.6f); 

            for (int x = 0; x < width; x++) {
                
                float dx = fabsf(x - cx);
                if (allowed_width < 0.1f || dx > allowed_width) continue;

                float norm_x = dx / max_width_half; 
                if (norm_x > 1.0f) norm_x = 1.0f;

                float wave_val = sinf(norm_x * PI_FLOAT);
                float current_top_limit = top_peak_y + (top_dip_depth * wave_val);

                if (y < (int)current_top_limit) continue;

                // Color Logic
                float dist_from_side = allowed_width - dx;
                float dist_from_top  = y - current_top_limit;
                
                bool is_border = (dist_from_side < 1.0f) || (dist_from_top < 0.9f);

                uint8_t r, g, b;

                if (is_border) {
                    float shine = sinf((x * 0.6f) + (y * 0.2f) + (frame * 0.15f));
                    float metal = 0.6f + (0.4f * shine); 
                    r = (uint8_t)(180 * metal * master_bright);
                    g = (uint8_t)(200 * metal * master_bright);
                    b = (uint8_t)(225 * metal * master_bright); 
                } 
                else {
                    float scan = sinf(y * 0.5f - frame * 0.25f);
                    float energy = 40.0f + (scan * 20.0f); 
                    if (dx < 1.0f) energy += 15.0f;
                    r = 0;
                    g = clamp8((int)(energy * 0.4f * master_bright));
                    b = clamp8((int)(energy * master_bright));
                }

                // ---------------------------------------------------------
                // UPDATED: Using centralized driver
                // ---------------------------------------------------------
                ws2812_draw_pixel(x, y, width, height, r, g, b);
            }
        }

        ws2812_update();
        sleep_us(SHIELD_FRAME_INTERVAL_US);
    }
    
    ws2812_clear();
}