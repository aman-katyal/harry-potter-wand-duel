#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "ws2812.pio.h"
#include "math.h"
#include "stdlib.h"
#include <time.h>

#define LED_PIN 17
#define NUM_LEDS 256
#define DMA_CHANNEL 0

PIO pio = pio0;
uint sm = 0;
uint32_t led_buffer[NUM_LEDS];

void setup_pio() {
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, LED_PIN, 800000, false);
}

void setup_dma() {
    dma_channel_config c = dma_channel_get_default_config(DMA_CHANNEL);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));
    dma_channel_configure(DMA_CHANNEL, &c, &pio->txf[sm], led_buffer, NUM_LEDS, false);
}

int health_leds = 16; // full width
int health = 16;      // start full health

void draw_health_bar(int width, int height) {
    for (int x = 0; x < width; x++) {
        int top_index = (0 % 2 == 0)
            ? 0 * width + x
            : 0 * width + (width - 1 - x);
        int bottom_index = (1 % 2 == 0)
            ? 1 * width + x
            : 1 * width + (width - 1 - x);

        if (x < health) {
            set_led_color(top_index, 255, 0, 0);
            set_led_color(bottom_index, 255, 0, 0);
        } else {
            set_led_color(top_index, 0, 0, 0);
            set_led_color(bottom_index, 0, 0, 0);
        }
    }
}

void show_loser_screen(int width, int height) {

    for (int i = 0; i < NUM_LEDS; i++)
        led_buffer[i] = 0;

    int pattern[16][16] = {0};

    pattern[5][3] = 1;
    pattern[5][4] = 1; 
    pattern[6][2] = 1;
    pattern[6][5] = 1;
    pattern[4][2] = 1;
    pattern[4][5] = 1;
    pattern[7][1] = 1;
    pattern[7][6] = 1;
    pattern[3][1] = 1;
    pattern[3][6] = 1;

    pattern[5][10] = 1;
    pattern[5][11] = 1;
    pattern[6][9]  = 1;
    pattern[6][12] = 1;
    pattern[4][9]  = 1;
    pattern[4][12] = 1;
    pattern[7][8]  = 1;
    pattern[7][13] = 1;
    pattern[3][8] = 1;
    pattern[3][13] = 1;

    pattern[12][4] = 1;
    pattern[12][5] = 1;
    pattern[12][6] = 1;
    pattern[12][9] = 1;
    pattern[12][10] = 1;
    pattern[12][11] = 1;
    pattern[12][7] = 1;
    pattern[12][8] = 1;

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            if (pattern[y][x]) {
                int index = (y % 2 == 0)
                    ? y * width + x
                    : y * width + (width - 1 - x);
                set_led_color(index, 255, 0, 0);
            }
        }
    }

    update_leds();
}


void set_led_color(int index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < NUM_LEDS) {
        led_buffer[index] = ((uint32_t)r << 16) | ((uint32_t)b << 8) | g;
    }
}

void update_leds() {
    dma_channel_set_read_addr(DMA_CHANNEL, led_buffer, true);
    dma_channel_wait_for_finish_blocking(DMA_CHANNEL);
}


// FIREWORK 

int main() {
    stdio_init_all();
    setup_pio();
    setup_dma();

    const int WIDTH  = 16;
    const int HEIGHT = 16;
    const float cx = (WIDTH - 1) / 2.0f;
    const float cy = (HEIGHT - 1) / 2.0f;

    float radius = 0.5f;
    const float max_radius   = 7.0f;
    const float growth_speed = 0.25f;
    bool expanding = true;
    
    const int   spikes      = 9;
    const float thinness    = 0.12f;  
    const int   fade_factor = 220;   
    int color_mode          = 0;          

    float ray_brightness[spikes];
    void randomize_brightness() {
        for (int i = 0; i < spikes; i++)
            ray_brightness[i] = 0.7f + (rand() % 30) / 100.0f;
    }

    srand(time(NULL));

    randomize_brightness();

    while (true) {
        for (int i = 0; i < NUM_LEDS; ++i) {
            uint32_t color = led_buffer[i];
            uint8_t r = (color >> 16) & 0xFF;
            uint8_t g = (color >> 8)  & 0xFF;
            uint8_t b =  color & 0xFF;
            r = (r * fade_factor) / 255;
            g = (g * fade_factor) / 255;
            b = (b * fade_factor) / 255;
            led_buffer[i] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
        }

        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                float dx = x - cx;
                float dy = y - cy;
                float dist  = sqrtf(dx * dx + dy * dy);
                float angle = atan2f(dy, dx);
                if (angle < 0) angle += 2 * M_PI;

                float segment = (2 * M_PI) / spikes;
                int ray_idx   = (int)(angle / segment);
                if (ray_idx >= spikes) ray_idx = spikes - 1;

                float ray_angle  = ray_idx * segment + segment / 2;
                float delta_angle = fabsf(angle - ray_angle);
                if (delta_angle > M_PI) delta_angle = fabsf(delta_angle - 2 * M_PI);

                if (delta_angle < thinness && fabsf(dist - radius) < 0.6f) {
                    float fade = 1.0f - fabsf(dist - radius);
                    if (fade < 0.0f) fade = 0.0f;
                    float brightness = fade * ray_brightness[ray_idx];

                    uint8_t r = 0, g = 0, b = 0;
                    if (color_mode == 0) {         
                        r = 0;
                        b = (uint8_t)(brightness * 220.0f);
                    } else if (color_mode == 1) {   
                        r = (uint8_t)(brightness * 255.0f);
                        b = 0;
                    } else {                       
                        r = (uint8_t)(brightness * 230.0f);
                        b = (uint8_t)(brightness * 160.0f);
                    }

                    int index = (y % 2 == 0)
                        ? y * WIDTH + x
                        : y * WIDTH + (WIDTH - 1 - x);
                    set_led_color(index, r, g, b);
                }
            }
        }

        const float core_radius = 1.5f;
        for (int y = 0; y < HEIGHT; ++y) {
            for (int x = 0; x < WIDTH; ++x) {
                float dx = x - cx;
                float dy = y - cy;
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist <= core_radius) {
                    float fade = 1.0f - (dist / core_radius);
                    uint8_t r = 0, g = 0, b = 0;
                    if (color_mode == 0) {
                        r = 0;
                        b = (uint8_t)(fade * 120.0f);
                    } else if (color_mode == 1) {   
                        r = (uint8_t)(fade * 150.0f);
                        b = 0;
                    } else {                        
                        r = (uint8_t)(fade * 160.0f);
                        b = (uint8_t)(fade * 100.0f);
                    }

                    int index = (y % 2 == 0)
                        ? y * WIDTH + x
                        : y * WIDTH + (WIDTH - 1 - x);
                    set_led_color(index, r, g, b);
                }
            }
        }

        draw_health_bar(WIDTH, HEIGHT);

        static int frame_counter = 0;
        frame_counter++;
        if (frame_counter > 50) {
            frame_counter = 0;

            int rand_num = rand() % 11; 
            health -= rand_num;
            if (health > 0) {
                int rand_num = rand() % 6;
                int columns_to_remove = rand_num * 2;
                health -= columns_to_remove;
                if (health < 0) health = 0;
        } else {
            // --- Show LOSER screen ---
            show_loser_screen(WIDTH, HEIGHT);

            // Flash it a few times
            for (int i = 0; i < 3; i++) {
                sleep_ms(500);
                // toggle off
                for (int j = 0; j < NUM_LEDS; j++) led_buffer[j] = 0;
                update_leds();
                sleep_ms(300);
                show_loser_screen(WIDTH, HEIGHT);
            }

            // Refill health
            sleep_ms(1000);
            health = 16;
        }

        }

        srand(time(NULL));

        update_leds();
        sleep_ms(40);

        if (expanding)
            radius += growth_speed;
        else
            radius -= growth_speed;

        if (radius >= max_radius) expanding = false;

        if (radius <= 0.5f && !expanding) {
            expanding = true;
            radius = 0.5f;
            color_mode = (color_mode + 1) % 3;  
            randomize_brightness();
            sleep_ms(400);
        }
    }
}






// Circle explosion

// #include <math.h>
// #include <stdlib.h>

// int main() {
//     stdio_init_all();
//     setup_pio();
//     setup_dma();

//     const int WIDTH = 16;
//     const int HEIGHT = 16;
//     const float cx = (WIDTH - 1) / 2.0f;
//     const float cy = (HEIGHT - 1) / 2.0f;

//     float radius = 0.5f;
//     const float max_radius = 7.0f;
//     const float growth_speed = 0.25f;
//     bool expanding = true;

//     // --- Firework settings ---
//     const int spikes = 12;        // number of streaks
//     const float thinness = 0.3f;  // lower = thinner lines
//     const int fade_factor = 220;
//     int color_mode = 0;           // 0=blue, 1=red, 2=magenta

//     while (true) {
//         // --- Step 1: fade previous frame ---
//         for (int i = 0; i < NUM_LEDS; ++i) {
//             uint32_t color = led_buffer[i];
//             uint8_t r = (color >> 16) & 0xFF;
//             uint8_t g = (color >> 8) & 0xFF;
//             uint8_t b = color & 0xFF;
//             r = (r * fade_factor) / 255;
//             g = (g * fade_factor) / 255;
//             b = (b * fade_factor) / 255;
//             led_buffer[i] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
//         }

//         // --- Step 2: draw thin radial lines ---
//         for (int y = 0; y < HEIGHT; ++y) {
//             for (int x = 0; x < WIDTH; ++x) {
//                 float dx = x - cx;
//                 float dy = y - cy;
//                 float dist = sqrtf(dx * dx + dy * dy);
//                 float angle = atan2f(dy, dx);

//                 // normalize angle 0..2π
//                 if (angle < 0) angle += 2 * M_PI;

//                 // which spike direction this pixel is near
//                 float segment = (2 * M_PI) / spikes;
//                 float modAngle = fmodf(angle, segment);

//                 // close to the center of a spike ray?
//                 if (fabsf(modAngle - segment / 2) < thinness && fabsf(dist - radius) < 0.5f) {
//                     float fade = 1.0f - fabsf(dist - radius);
//                     if (fade < 0.0f) fade = 0.0f;

//                     uint8_t r, g = 0, b;
//                     if (color_mode == 0) {        // blue
//                         r = 0; b = (uint8_t)(fade * 255.0f);
//                     } else if (color_mode == 1) { // red
//                         r = (uint8_t)(fade * 255.0f); b = 0;
//                     } else {                      // magenta
//                         r = (uint8_t)(fade * 255.0f); b = (uint8_t)(fade * 200.0f);
//                     }

//                     int index = (y % 2 == 0)
//                                 ? y * WIDTH + x
//                                 : y * WIDTH + (WIDTH - 1 - x);

//                     set_led_color(index, r, g, b);
//                 }
//             }
//         }

//         update_leds();
//         sleep_ms(40);

//         // --- Step 3: animate radius ---
//         if (expanding)
//             radius += growth_speed;
//         else
//             radius -= growth_speed;

//         if (radius >= max_radius) expanding = false;

//         // when it contracts fully, change color
//         if (radius <= 0.5f && !expanding) {
//             expanding = true;
//             radius = 0.5f;
//             color_mode = (color_mode + 1) % 3; // blue→red→magenta→repeat
//             sleep_ms(300);
//         }
//     }
// }




