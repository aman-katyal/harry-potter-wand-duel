#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "ws2812.pio.h"

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

// Correct for your hardware: Red, Blue, Green
void set_led_color(int index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < NUM_LEDS) {
        led_buffer[index] = ((uint32_t)r << 16) | ((uint32_t)b << 8) | g;
    }
}

void update_leds() {
    dma_channel_set_read_addr(DMA_CHANNEL, led_buffer, true);
    dma_channel_wait_for_finish_blocking(DMA_CHANNEL);
}

int main() {
    stdio_init_all();
    setup_pio();
    setup_dma();

    int color_mode = 0;

    // Diagnostic loop to confirm fix.
    // It should cycle Red, then Green, then Blue.
    while (true) {
      uint8_t r = 0, g = 0, b = 0;

      if (color_mode == 0) { r = 150; }       // Red
      else if (color_mode == 1) { g = 150; }  // Green
      else { b = 150; }                       // Blue

      for (int i = 0; i < NUM_LEDS; ++i) {
          set_led_color(i, r, g, b);
      }

      color_mode = (color_mode + 1) % 3;

      update_leds();
      sleep_ms(1000); // Wait 1 second
    }
}