#ifndef WS2812_H
#define WS2812_H

#include "pico/stdlib.h"

// Configuration
#define LED_PIN 17
#define NUM_LEDS 256
#define DMA_CHANNEL 0

void ws2812_init();
void ws2812_set_pixel_color(uint index, uint8_t r, uint8_t g, uint8_t b);
void ws2812_fill(uint8_t r, uint8_t g, uint8_t b); // <-- ADD THIS
void ws2812_update();
void ws2812_clear();

uint32_t* ws2812_get_buffer(void);

#endif // WS2812_H