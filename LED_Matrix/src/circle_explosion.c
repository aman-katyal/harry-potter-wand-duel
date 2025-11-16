#include "circle_explosion.h"
#include "ws2812.h"
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include "firework.h"
#include <math.h>
#include <stdlib.h>

void circle_explosion(int width, int height, int color_mode, int intensity)
{
    // Keep color_mode sane
    color_mode = color_mode % 3;

    const float cx = (width  - 1) / 2.0f;
    const float cy = (height - 1) / 2.0f;

    float radius = 0.5f;
    const float max_radius   = 7.0f;
    const float growth_speed = 0.30f;   // slightly faster than firework
    bool expanding = true;

    const float ring_thickness = 0.55f;
    const int fade_factor      = 225;

    for (int frame = 0; frame < 85; frame++)
    {
        // --- Step 1: Fade previous pixels ---
        uint32_t* buf = ws2812_get_buffer();

        for (int i = 0; i < NUM_LEDS; i++) {
            uint32_t c = buf[i];

            uint8_t r = (c >> 16) & 0xFF;
            uint8_t g = (c >> 8)  & 0xFF;
            uint8_t b =  c        & 0xFF;

            r = (uint8_t)((r * fade_factor) / 255);
            g = (uint8_t)((g * fade_factor) / 255);
            b = (uint8_t)((b * fade_factor) / 255);

            buf[i] = (r << 16) | (g << 8) | b;
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

                    int index =
                        (y % 2 == 0)
                        ? (y * width + x)
                        : (y * width + (width - 1 - x));

                    ws2812_set_pixel_color(index, r, g, b);
                }
            }
        }

        // --- Step 3: Send to LEDs via DMA ---
        dma_channel_set_read_addr(DMA_CHANNEL,
                                  (void*)ws2812_get_buffer(),
                                  true);
        sleep_us(FRAME_INTERVAL_US);

        // --- Step 4: Animate radius ---
        radius += (expanding ? growth_speed : -growth_speed);

        if (radius >= max_radius)
            expanding = false;

        if (radius <= 0.5f && !expanding)
            expanding = true;
    }
}
