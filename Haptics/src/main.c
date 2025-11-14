#include <stdio.h>
#include "pico/stdlib.h"
#include "drv2605.h"

#define I2C_SDA 16
#define I2C_SCL 17

int main() {
    stdio_init_all();
    sleep_ms(3000);
    
    printf("--- Haptics Test ---\n");
    
    drv2605_t haptic;
    drv2605_init(&haptic, i2c0, I2C_SDA, I2C_SCL);
    drv2605_set_mode(&haptic, DRV2605_MODE_INTTRIG);
    drv2605_select_library(&haptic, 1);
    
    while (true) {
        printf("Playing buzz...\n");
        drv2605_set_waveform(&haptic, 0, 84);
        drv2605_set_waveform(&haptic, 1, 0);
        drv2605_go(&haptic);
        
        sleep_ms(2000);
    }
}