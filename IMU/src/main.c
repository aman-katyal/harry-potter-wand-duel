#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "bno085.h"
#include <stdio.h>

// I2C1 on GP26=SDA, GP27=SCL
#define SDA_PIN 24
#define SCL_PIN 25

int main() {
    stdio_init_all();
    sleep_ms(300); // allow USB CDC to come up

    // ---- Configure pins for I2C1 ----
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);

    i2c_init(i2c0, BNO085_I2C_BAUD_HZ);

    sleep_ms(200); // allow IMU to power up

    // ---- I2C scan ----
    printf("Scanning I2C1...\n");
    for (int addr = 0; addr < 128; addr++) {
        uint8_t r;
        if (i2c_read_blocking(i2c1, addr, &r, 1, false) >= 0) {
            printf("Found device at 0x%02X\n", addr);
        }
    }

    // ---- Initialize IMU ----
    bno085_t imu;
    if (!bno085_init(&imu, i2c1, BNO085_I2C_ADDR_DEFAULT)) {
        printf("Init failed (check wiring)\n");
        while (1) tight_loop_contents();
    }

    // ---- Enable quaternion + accel reports ----
    if (!bno085_enable_reports(&imu, 10000, 10000)) {
        printf("Failed to enable reports\n");
    }

    absolute_time_t last_print = get_absolute_time();

    while (true) {
        bno085_poll(&imu);

        if (absolute_time_diff_us(last_print, get_absolute_time()) > 40000) {
            last_print = get_absolute_time();

            bno085_outputs_t out = bno085_get_outputs(&imu);

            if (out.quat.valid) {
                printf("Quat  i:% .4f  j:% .4f  k:% .4f  r:% .4f  ",
                       out.quat.qi, out.quat.qj, out.quat.qk, out.quat.qr);
            } else {
                printf("Quat  (waiting)  ");
            }

            if (out.accel.valid) {
                printf("Accel X:% .3f  Y:% .3f  Z:% .3f m/s^2",
                       out.accel.ax, out.accel.ay, out.accel.az);
            } else {
                printf("Accel (waiting)");
            }
            printf("\n");
        }

        sleep_ms(1);
    }
}
