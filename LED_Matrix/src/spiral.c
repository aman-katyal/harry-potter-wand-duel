#include "spiral.h"
#include "ws2812.h"
#include "pico/stdlib.h"
#include "hardware/dma.h"
#include <math.h>
#include <stdlib.h>

#define FRAME_INTERVAL_US 20000

void spiral(int width, int height, int color_mode, int intensity) {
    const float cx = (width - 1) / 2.0f;
    const float cy = (height - 1) / 2.0f;

    const float max_radius = hypotf(cx, cy);
    const float speed = 0.25f;   // controls rotation speed
    const float spiral_gap = 0.6f; // distance between spiral arms

    float angle_offset = 0.0f;

    for (int frame = 0; frame < 120; frame++) {
        uint32_t* buf = ws2812_get_buffer();

        // clear frame
        for (int i = 0; i < width * height; i++) {
            buf[i] = 0;
        }

        // draw spiral
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                float dx = x - cx;
                float dy = y - cy;
                float dist = sqrtf(dx * dx + dy * dy);
                float angle = atan2f(dy, dx);

                if (angle < 0) angle += 2.0f * (float)M_PI;

                // Spiral condition: light up points near a moving arm
                float spiral_pos = angle + dist / spiral_gap + angle_offset;
                float wave = fmodf(spiral_pos, 2.0f * (float)M_PI);
                float brightness = cosf(wave) * 0.5f + 0.5f;  // smooth fade

                if (brightness > 0.6f) {  // only light up arm
                    uint8_t r = 0, g = 0, b = 0;

                    switch (color_mode % 3) {
                        case 0: b = (uint8_t)(brightness * intensity); break;  // blue
                        case 1: r = (uint8_t)(brightness * intensity); break;  // red
                        case 2: r = (uint8_t)(brightness * intensity);
                                b = (uint8_t)(brightness * (intensity * 0.6f)); break; // magenta
                    }

                    int index = (y % 2 == 0)
                        ? y * width + x
                        : y * width + (width - 1 - x);

                    ws2812_set_pixel_color(index, r, g, b);
                }
            }
        }

        dma_channel_set_read_addr(DMA_CHANNEL, (void*)ws2812_get_buffer(), true);
        
        sleep_us(FRAME_INTERVAL_US);

        angle_offset += speed;
        if (angle_offset > 2.0f * (float)M_PI)
            angle_offset -= 2.0f * (float)M_PI;
    }
}
