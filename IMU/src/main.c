#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "bno085.h"
#include <stdio.h>
#include "pico/stdio.h" // <<< MAKE SURE THIS LINE IS PRESENT

// I2C0 on GP24=SDA, GP25=SCL
#define SDA_PIN 24
#define SCL_PIN 25
// Hardware reset pin for BNO085 (connect to RST). Use -1 if not connected.
#define RST_PIN 22

// VVVVVV  THIS LINE WAS MISSING VVVVVV
int main() {
    stdio_init_all();
    // Wait for USB serial connection
    while (!stdio_usb_connected()) {
        sleep_ms(100);
    }
    printf("\n--- BNO085 Pico C Example ---\n");

    // ---- Configure I2C0 ----
    i2c_init(i2c0, BNO085_I2C_BAUD_HZ);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    // Give the IMU time to power up
    sleep_ms(100);

    // ---- Initialize IMU ----
    bno085_t imu;
    if (!bno085_init(&imu, i2c0, RST_PIN)) {
        printf("Init failed! Check wiring and I2C address.\n");
        // A common issue is the sensor not responding. An I2C scan can help debug.
        printf("Scanning I2C0 bus...\n");
        for (int addr = 0; addr < 128; addr++) {
            uint8_t r;
            if (i2c_read_blocking(i2c0, addr, &r, 1, false) >= 0) {
                printf("Found device at 0x%02X\n", addr);
            }
        }
        while (1) tight_loop_contents();
    }
    printf("BNO085 Initialized Successfully!\n");

    // ---- Enable quaternion + accel reports ----
    // Set interval to 10000 us = 10ms = 100 Hz
    if (!bno085_enable_reports(&imu, 10000, 10000)) {
        printf("Failed to enable reports\n");
    } else {
        printf("Reports enabled.\n");
    }

    absolute_time_t last_print = get_absolute_time();

    while (true) {
        // Poll for new data. This function reads and parses one packet if available.
        bno085_poll(&imu);

        // Print data at a fixed interval
        if (absolute_time_diff_us(last_print, get_absolute_time()) > 100000) { // 100ms
            last_print = get_absolute_time();

            bno085_outputs_t out = bno085_get_outputs(&imu);

            if (out.quat.valid) {
                printf("Quat  i:% .3f  j:% .3f  k:% .3f  r:% .3f | ",
                       out.quat.qi, out.quat.qj, out.quat.qk, out.quat.qr);
            } else {
                printf("Quat (waiting...) | ");
            }

            if (out.accel.valid) {
                printf("Accel X:% .2f  Y:% .2f  Z:% .2f m/s^2",
                       out.accel.ax, out.accel.ay, out.accel.az);
            } else {
                printf("Accel (waiting...)");
            }
            printf("\n");
        }
    }

// VVVVVV  AND THIS FINAL BRACE WAS MISSING VVVVVV
}