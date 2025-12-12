#ifndef WS2812_H
#define WS2812_H

#include <stdint.h>
#include "pico/stdlib.h"

// Configuration
#define LED_PIN     18
#define NUM_LEDS    256
#define DMA_CHANNEL 0

// Match the .c file exactly: use int for the index.
void ws2812_init(void);

void ws2812_set_pixel_color(int index, uint8_t r, uint8_t g, uint8_t b); 
// GRB order inside implementation

void ws2812_fill(uint8_t r, uint8_t g, uint8_t b);
void ws2812_clear(void);

// Pushes the current buffer to the LEDs
void ws2812_update(void);

// Pointers for the DMA system
uint32_t* ws2812_get_buffer(void);

void ws2812_set_rotation(int rotation);
void ws2812_draw_pixel(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b);

#endif // WS2812_H