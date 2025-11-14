// #include "ws2812.h"
// #include "hardware/pio.h"
// #include "hardware/dma.h"
// #include "ws2812.pio.h"

// // --- Global variables for this driver ---
// static PIO pio = pio0;
// static uint sm = 0;
// static uint32_t led_buffer[NUM_LEDS];

// static void setup_pio() {
//     uint offset = pio_add_program(pio, &ws2812_program);
//     ws2812_program_init(pio, sm, offset, LED_PIN, 800000, false);
// }

// static void setup_dma() {
//     dma_channel_config c = dma_channel_get_default_config(DMA_CHANNEL);
//     channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
//     channel_config_set_read_increment(&c, true);
//     channel_config_set_write_increment(&c, false);
//     channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));
//     dma_channel_configure(DMA_CHANNEL, &c, &pio->txf[sm], led_buffer, NUM_LEDS, false);
// }

// void ws2812_init() {
//     setup_pio();
//     setup_dma();
// }

// void ws2812_set_pixel_color(uint index, uint8_t r, uint8_t g, uint8_t b) {
//     if (index < NUM_LEDS) {
//         led_buffer[index] = ((uint32_t)r << 16) | ((uint32_t)b << 8) | g;
//     }
// }

// void ws2812_fill(uint8_t r, uint8_t g, uint8_t b) {
//     uint32_t color = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
//     for (int i = 0; i < NUM_LEDS; ++i) {
//         led_buffer[i] = color;
//     }
// }

// void ws2812_update() {
//     dma_channel_set_read_addr(DMA_CHANNEL, led_buffer, true);
//     dma_channel_wait_for_finish_blocking(DMA_CHANNEL);
// }

// void ws2812_clear() {
//     ws2812_fill(0, 0, 0);
//     ws2812_update();     
// }

// uint32_t* ws2812_get_buffer() {
//     extern uint32_t led_buffer[];  
//     return led_buffer;
// }

#include "ws2812.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "ws2812.pio.h"

PIO pio = pio0;
uint sm = 0;
uint32_t led_buffer[NUM_LEDS];   // <-- NOT STATIC

static void setup_pio() {
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, LED_PIN, 800000, false);
}

static void setup_dma() {
    dma_channel_config c = dma_channel_get_default_config(DMA_CHANNEL);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));
    dma_channel_configure(DMA_CHANNEL, &c, &pio->txf[sm], led_buffer, NUM_LEDS, false);
}

void ws2812_init() {
    setup_pio();
    setup_dma();
}

void ws2812_set_pixel_color(uint index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < NUM_LEDS) {
        led_buffer[index] = (r << 16) | (g << 8) | b;
    }
}

void ws2812_update() {
    dma_channel_set_read_addr(DMA_CHANNEL, led_buffer, true);
    dma_channel_wait_for_finish_blocking(DMA_CHANNEL);
}

uint32_t* ws2812_get_buffer() {
    return led_buffer;    // works now
}

