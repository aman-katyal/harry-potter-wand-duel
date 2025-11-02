/*
 * Harry Potter Wand IR Transmitter
 * RP2350 – NEC 38 kHz carrier implementation
 * Author: Rhea Virk
 *
 * Sends NEC-encoded commands at 38 kHz using PWM on GPIO 36
 * for use with TSMP96000 or similar 38 kHz IR receivers.
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

// -----------------------------------------------------------------------------
// IR protocol constants (NEC standard, microseconds)
// -----------------------------------------------------------------------------
#define NEC_HDR_MARK    9000
#define NEC_HDR_SPACE   4500
#define NEC_BIT_MARK     560
#define NEC_ONE_SPACE   1690
#define NEC_ZERO_SPACE   560
#define NEC_STOP_MARK    560

// -----------------------------------------------------------------------------
// PWM configuration for 38 kHz IR carrier
// -----------------------------------------------------------------------------
#define IR_GPIO    36        // connect emitter driver here
#define PWM_FREQ   38000     // 38 kHz carrier
#define PWM_DUTY   0.33f     // 33 % duty cycle

static uint slice_num;

// -----------------------------------------------------------------------------
// Initialize PWM for 38 kHz modulation (disabled by default)
// -----------------------------------------------------------------------------
void ir_pwm_init(void) {
    gpio_set_function(IR_GPIO, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(IR_GPIO);

    // 125 MHz system clock → 38 kHz carrier → TOP ≈ 3289
    uint32_t top = (uint32_t)(125000000 / PWM_FREQ) - 1;
    pwm_set_wrap(slice_num, top);

    uint32_t level = (uint32_t)(PWM_DUTY * top);
    pwm_set_chan_level(slice_num, PWM_CHAN_A, level);

    pwm_set_enabled(slice_num, false); // start off
}

// -----------------------------------------------------------------------------
// Helpers for enabling/disabling carrier
// -----------------------------------------------------------------------------
static inline void pwm_on(void)  { pwm_set_enabled(slice_num, true);  }
static inline void pwm_off(void) { pwm_set_enabled(slice_num, false); }

static inline void mark(uint32_t usec) {
    pwm_on();
    sleep_us(usec);
    pwm_off();
}

static inline void space(uint32_t usec) {
    pwm_off();
    sleep_us(usec);
}

// -----------------------------------------------------------------------------
// Send one NEC frame: address, command, complements (32 bits total)
// -----------------------------------------------------------------------------
void ir_send_nec(uint8_t addr, uint8_t cmd) {
    uint32_t frame = (uint32_t)addr |
                     ((uint32_t)(~addr) << 8) |
                     ((uint32_t)cmd << 16) |
                     ((uint32_t)(~cmd) << 24);

    // Header burst
    mark(NEC_HDR_MARK);
    space(NEC_HDR_SPACE);

    // 32 data bits, LSB first
    for (int i = 0; i < 32; i++) {
        mark(NEC_BIT_MARK);
        if (frame & 1)
            space(NEC_ONE_SPACE);
        else
            space(NEC_ZERO_SPACE);
        frame >>= 1;
    }

    // Stop bit
    mark(NEC_STOP_MARK);
    pwm_off();
}

// -----------------------------------------------------------------------------
// Example main loop
// -----------------------------------------------------------------------------
int main(void) {
    stdio_init_all();
    printf("Starting IR transmitter (38 kHz NEC)...\n");

    ir_pwm_init();

    uint8_t address = 0x00;
    uint8_t command = 0x34;

    while (true) {
        printf("Sending NEC frame: addr=0x%02X cmd=0x%02X\n", address, command);
        ir_send_nec(address, command);
        sleep_ms(1000);   // 1 s between sends
    }

    return 0;
}
