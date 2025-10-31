// src/sh2_hal.c

#include "sh2_hal.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

// --- PIN DEFINITIONS FOR YOUR WIRING (Option 2) ---
#define BNO08X_SPI_PORT     spi0
#define BNO08X_PIN_MISO     16
#define BNO08X_PIN_CS       17
#define BNO08X_PIN_SCK      18
#define BNO08X_PIN_MOSI     19
#define BNO08X_PIN_RESET    15
#define BNO08X_PIN_INT      14

#define BNO08X_SPI_SPEED_HZ 1000 * 1000 // 1 MHz, safe starting speed

int sh2_hal_init() {
    // Initialize SPI
    spi_init(BNO08X_SPI_PORT, BNO08X_SPI_SPEED_HZ);
    
    // Set SPI pin functions
    gpio_set_function(BNO08X_PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(BNO08X_PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(BNO08X_PIN_MOSI, GPIO_FUNC_SPI);
    
    // Initialize Chip Select pin
    gpio_init(BNO08X_PIN_CS);
    gpio_set_dir(BNO08X_PIN_CS, GPIO_OUT);
    gpio_put(BNO08X_PIN_CS, 1); // Active low, so set high initially

    // Initialize Reset pin
    gpio_init(BNO08X_PIN_RESET);
    gpio_set_dir(BNO08X_PIN_RESET, GPIO_OUT);
    gpio_put(BNO08X_PIN_RESET, 1); // Keep out of reset

    // Initialize Interrupt pin
    gpio_init(BNO08X_PIN_INT);
    gpio_set_dir(BNO08X_PIN_INT, GPIO_IN);

    // Perform initial hardware reset
    sh2_hal_reset();

    return 0; // Success
}

void sh2_hal_reset() {
    // Pull reset pin low
    gpio_put(BNO08X_PIN_RESET, 0);
    sleep_ms(10); // Hold in reset for 10ms
    // Pull reset pin high
    gpio_put(BNO08X_PIN_RESET, 1);
    sleep_ms(100); // Wait for the sensor to boot
}

int sh2_hal_transfer(const uint8_t *pSend, uint8_t *pRecv, uint32_t len) {
    // Assert chip select (active low)
    gpio_put(BNO08X_PIN_CS, 0);
    
    // Perform the SPI transaction
    spi_write_read_blocking(BNO08X_SPI_PORT, pSend, pRecv, len);
    
    // De-assert chip select
    gpio_put(BNO08X_PIN_CS, 1);

    return 0; // Success
}

bool sh2_hal_read_interrupt() {
    // The INT pin on the BNO08x is active low.
    // Return true if the pin is low (interrupt is active).
    return !gpio_get(BNO08X_PIN_INT);
}

uint64_t sh2_hal_get_timestamp_us() {
    return time_us_64();
}