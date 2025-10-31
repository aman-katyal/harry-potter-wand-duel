#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

#include "sh2.h"
#include "sh2_hal.h"
#include "sh2_err.h"
#include "sh2_SensorValue.h"

// --- I2C Configuration ---
#define I2C_PORT i2c0
#define I2C_SDA_PIN 16
#define I2C_SCL_PIN 17
#define BNO08X_I2C_ADDR_DEFAULT 0x4A

// --- Globals & Prototypes ---
static sh2_SensorValue_t sensor_value;
static bool has_sensor_event = false;
static int i2chal_open(sh2_Hal_t *self);
static void i2chal_close(sh2_Hal_t *self);
static int i2chal_read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us);
static int i2chal_write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len);
static uint32_t hal_get_time_us(sh2_Hal_t *self);

void sensor_handler(void *cookie, sh2_SensorEvent_t *pEvent) {
    if (sh2_decodeSensorEvent(&sensor_value, pEvent) == SH2_OK) has_sensor_event = true;
}

// --- FINAL, ROBUST HAL IMPLEMENTATION ---

static int i2chal_open(sh2_Hal_t *self) {
    i2c_init(I2C_PORT, 100 * 1000); // Use the reliable 100kHz speed
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);
    sleep_ms(100);
    uint8_t softreset_pkt[] = {5, 0, 1, 0, 1};
    if (i2c_write_blocking(I2C_PORT, BNO08X_I2C_ADDR_DEFAULT, softreset_pkt, sizeof(softreset_pkt), false) > 0) {
        sleep_ms(300); // Give it time to reset
        return 0;
    }
    return -1;
}

static void i2chal_close(sh2_Hal_t *self) { i2c_deinit(I2C_PORT); }

static int i2chal_write(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len) {
    int bytes_written = i2c_write_blocking(I2C_PORT, BNO08X_I2C_ADDR_DEFAULT, pBuffer, len, false);
    if (bytes_written < 0) return 0;
    return bytes_written;
}

// This is the final, corrected read function that solves the "stuck packet" problem.
static int i2chal_read(sh2_Hal_t *self, uint8_t *pBuffer, unsigned len, uint32_t *t_us) {
    *t_us = time_us_32();
    uint8_t header[4];

    // 1. Read the 4-byte header to see how long the BNO *thinks* the packet is.
    if (i2c_read_blocking(I2C_PORT, BNO08X_I2C_ADDR_DEFAULT, header, 4, false) != 4) {
        return 0; // Failed to read header
    }

    uint16_t packet_size = (uint16_t)header[0] | ((uint16_t)header[1] << 8);
    packet_size &= ~0x8000;

    // If the sensor reports an empty packet, we're done.
    if (packet_size == 0) return 0;

    // Check if the reported packet size will fit in the library's buffer.
    if (packet_size > len) {
        // It won't fit. We MUST read the data to clear the sensor's buffer,
        // but we will discard it and tell the library that we failed.
        uint8_t temp_buffer[packet_size];
        i2c_read_blocking(I2C_PORT, BNO08X_I2C_ADDR_DEFAULT, temp_buffer, packet_size, false);
        return 0;
    }

    // 2. The packet fits. Now, perform a NEW read for the ENTIRE packet.
    // This is the crucial step that "consumes" the packet from the BNO's buffer.
    if (i2c_read_blocking(I2C_PORT, BNO08X_I2C_ADDR_DEFAULT, pBuffer, packet_size, false) != packet_size) {
        return 0; // The full read failed for some reason.
    }
    
    // Success! Return the size of the packet we read.
    return packet_size;
}

static uint32_t hal_get_time_us(sh2_Hal_t *self) { return time_us_32(); }

// --- Main Application ---
int main() {
    stdio_init_all();
    sleep_ms(3000);
    printf("\n--- BNO085 I2C Test for RP2350 (Final Version) ---\n");

    sh2_Hal_t hal;
    hal.open = i2chal_open; hal.close = i2chal_close;
    hal.read = i2chal_read; hal.write = i2chal_write;
    hal.getTimeUs = hal_get_time_us;

    int status = sh2_open(&hal, NULL, NULL);
    if (status != SH2_OK) {
        printf("FAILURE: sh2_open failed. Check wiring.\n");
        while(1) tight_loop_contents();
    }
    printf("SUCCESS: sh2_open completed.\n");
    
    // Add a delay here to ensure the BNO is fully initialized after reset and advertisement.
    sleep_ms(250);

    sh2_setSensorCallback(sensor_handler, NULL);
    
    printf("Enabling Game Rotation Vector...\n");
    sh2_SensorConfig_t config;
    config.reportInterval_us = 10000;
    status = sh2_setSensorConfig(SH2_GAME_ROTATION_VECTOR, &config);
    if (status != SH2_OK) {
        printf("FAILURE: Could not enable Game Rotation Vector (Error: %d)\n", status);
    } else {
        printf("SUCCESS: Game Rotation Vector enabled!\n");
    }

    printf("Setup complete. Reading events...\n\n");
    while(1) {
        sh2_service();
        if (has_sensor_event) {
            has_sensor_event = false;
            printf("Game RV - R: %.2f, I: %.2f, J: %.2f, K: %.2f\r",
                   sensor_value.un.gameRotationVector.real, sensor_value.un.gameRotationVector.i,
                   sensor_value.un.gameRotationVector.j, sensor_value.un.gameRotationVector.k);
        }
    }
    return 0;
}