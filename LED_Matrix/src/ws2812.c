#include "ws2812.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "ws2812.pio.h"

// --- Global variables for this driver ---
static PIO pio = pio0;
static uint sm = 0;
static uint32_t led_buffer[NUM_LEDS];

// --- Private Functions ---
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
    // This line will now work because led_buffer is declared above
    dma_channel_configure(DMA_CHANNEL, &c, &pio->txf[sm], led_buffer, NUM_LEDS, false);
}

// --- Public Functions ---
void ws2812_init() {
    setup_pio();
    setup_dma();
}

void ws2812_set_pixel_color(uint index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < NUM_LEDS) {
        // WS2812 LEDs require GRB color order.
        // The 32-bit value is packed as 0x00GGRRBB
        led_buffer[index] = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
    }
}

void ws2812_fill(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t color = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
    for (int i = 0; i < NUM_LEDS; ++i) {
        led_buffer[i] = color;
    }
}

void ws2812_update() {
    dma_channel_set_read_addr(DMA_CHANNEL, led_buffer, true);
    // Note: For very high-speed animations, you might remove the blocking wait
    // and instead check dma_channel_is_busy()
    dma_channel_wait_for_finish_blocking(DMA_CHANNEL);
}

void ws2812_clear() {
    ws2812_fill(0, 0, 0); // Fill buffer with black
    ws2812_update();      // Push the cleared buffer to the LEDs
}