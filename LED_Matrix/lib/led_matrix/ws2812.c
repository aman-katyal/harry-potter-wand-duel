#include "ws2812.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/clocks.h" // <--- ADD THIS (Required for clock_get_hz)
#include "ws2812.pio.h"

// Globals
PIO pio = pio0;
uint sm = 0;
int dma_chan = 0; // Changed to variable so we can claim it dynamically
uint32_t led_buffer[NUM_LEDS];
static int current_rotation = 0; // 0=Normal, 1=Rotated

// ------------------------------------------------------------
// Low-level PIO Init
// ------------------------------------------------------------
void ws2812_program_init_fixed(PIO pio, uint sm, uint offset, uint pin, float freq) {
    pio_sm_config c = ws2812_program_get_default_config(offset);

    sm_config_set_sideset_pins(&c, pin);
    sm_config_set_out_shift(&c, false, true, 24);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);

    // Correct WS2812 timing based on system clock
    float div = (float)clock_get_hz(clk_sys) / (freq * 10);
    sm_config_set_clkdiv(&c, div);

    pio_gpio_init(pio, pin);
    pio_sm_set_consecutive_pindirs(pio, sm, pin, 1, true);

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

// ------------------------------------------------------------
// DMA Setup
// ------------------------------------------------------------
static void ws2812_setup_dma() {
    // Best practice: Ask the SDK for an unused channel instead of hardcoding
    dma_chan = dma_claim_unused_channel(true);

    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));

    dma_channel_configure(
        dma_chan,           // Channel
        &c,                 // Config
        &pio->txf[sm],      // Destination (PIO FIFO)
        led_buffer,         // Source (LED Buffer)
        NUM_LEDS,           // Count
        false               // Don't start yet
    );
}

// ------------------------------------------------------------
// Public Init
// ------------------------------------------------------------
void ws2812_init() {
    // Add program to PIO instruction memory
    uint offset = pio_add_program(pio, &ws2812_program);

    // Initialize State Machine
    ws2812_program_init_fixed(pio, sm, offset, LED_PIN, 800000.0f);

    // Initialize DMA
    ws2812_setup_dma();
}

// ------------------------------------------------------------
// Core Driver Functions
// ------------------------------------------------------------

void ws2812_set_pixel_color(int index, uint8_t r, uint8_t g, uint8_t b) {
    if (index >= 0 && index < NUM_LEDS) {
        // PACKING: RBG Order (Specific to your hardware)
        led_buffer[index] =
            ((uint32_t)r << 16) |
            ((uint32_t)b << 8)  |
             (uint32_t)g;
    }
}

void ws2812_fill(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < NUM_LEDS; i++) {
        ws2812_set_pixel_color(i, r, g, b);
    }
}

void ws2812_clear() {
    ws2812_fill(0, 0, 0);
    ws2812_update();     
}

uint32_t* ws2812_get_buffer() {
    return led_buffer;
}

void ws2812_update() {
    dma_channel_set_read_addr(dma_chan, led_buffer, true);
}

// ------------------------------------------------------------
// High-Level Logic (Rotation & Mapping)
// ------------------------------------------------------------

void ws2812_set_rotation(int rotation) {
    current_rotation = rotation;
}

void ws2812_draw_pixel(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b) {
    int phys_x, phys_y;

    // 1. Apply Rotation
    if (current_rotation == 1) {
        // 90 Degree Rotation (Top becomes Left)
        phys_x = y;
        phys_y = (width - 1) - x;
    } 
    else {
        // Normal Orientation
        phys_x = x;
        phys_y = y;
    }

    // 2. Bounds Check (Safety first!)
    if (phys_x < 0 || phys_x >= width || phys_y < 0 || phys_y >= height) {
        return;
    }

    // 3. ZigZag Mapping
    // Assumes Even rows (0, 2, 4) go Right, Odd rows go Left
    int index;
    if (phys_y % 2 == 0) {
        // Left to Right
        index = phys_y * width + phys_x;
    } else {
        // Right to Left
        index = phys_y * width + (width - 1 - phys_x);
    }

    // 4. Set the actual buffer
    ws2812_set_pixel_color(index, r, g, b);
}