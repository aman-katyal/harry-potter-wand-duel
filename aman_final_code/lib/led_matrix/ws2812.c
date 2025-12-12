#include "ws2812.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "ws2812.pio.h"

PIO pio = pio0;
uint sm = 0;
uint32_t led_buffer[NUM_LEDS];


// FIXED: renamed to avoid conflict with auto-generated function
void ws2812_program_init_fixed(PIO pio, uint sm, uint offset, uint pin, float freq) {
    pio_sm_config c = ws2812_program_get_default_config(offset);

    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_out_shift(&c, false, true, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    float div = (float)clock_get_hz(clk_sys) / (freq * 10);
    sm_config_set_clkdiv(&c, div);

    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

// DMA Setup
static void ws2812_setup_dma() {
    dma_channel_config c = dma_channel_get_default_config(DMA_CHANNEL);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));

    dma_channel_configure(
        DMA_CHANNEL, &c,
        &pio->txf[sm],
        led_buffer,
        NUM_LEDS,
        false
    );
}

// Load the WS2812 PIO program into PIO instruction memory
void ws2812_init() {
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init_fixed(pio, sm, offset, LED_PIN, 800000.0f);

    ws2812_setup_dma();
}

// RBG order  
void ws2812_set_pixel_color(int index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < NUM_LEDS) {
        led_buffer[index] =
            ((uint32_t)r << 16) |
            ((uint32_t)b << 8)  |
             (uint32_t)g;
    }
}

void ws2812_fill(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < NUM_LEDS; i++)
        ws2812_set_pixel_color(i, r, g, b);
}

// updates all leds to turn off
void ws2812_clear() {
    ws2812_fill(0, 0, 0);
    ws2812_update();     
}

uint32_t* ws2812_get_buffer() {
    return led_buffer;
}

void ws2812_update() {
    dma_channel_set_read_addr(DMA_CHANNEL, led_buffer, true);
}