#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/pwm.h"

// -----------------------------------------------------------------------------
// Simple IR Protocol Constants (microseconds)
// -----------------------------------------------------------------------------
#define SIMPLE_HDR_MARK   2000  // Long start pulse
#define SIMPLE_HDR_SPACE  1000  // Space after start pulse
#define SIMPLE_BIT_MARK    500  // The pulse for every bit
#define SIMPLE_ONE_SPACE  1000  // The space for a '1'
#define SIMPLE_ZERO_SPACE  500  // The space for a '0'

// -----------------------------------------------------------------------------
// PWM configuration for 38 kHz IR carrier
// -----------------------------------------------------------------------------
#define IR_GPIO    36        // The pin your emitter is on
#define PWM_FREQ   38000     // 38 kHz carrier

static uint slice_num;

void ir_pwm_init(void) {
    gpio_set_function(IR_GPIO, GPIO_FUNC_PWM);
    slice_num = pwm_gpio_to_slice_num(IR_GPIO);
    uint32_t top = (uint32_t)(125000000 / PWM_FREQ) - 1;
    pwm_set_wrap(slice_num, top);
    pwm_set_chan_level(slice_num, PWM_CHAN_A, (uint32_t)(0.33f * top));
    pwm_set_enabled(slice_num, false);
}

static inline void mark(uint32_t usec) {
    pwm_set_enabled(slice_num, true);
    sleep_us(usec);
    pwm_set_enabled(slice_num, false);
}

static inline void space(uint32_t usec) {
    pwm_set_enabled(slice_num, false);
    sleep_us(usec);
}

// -----------------------------------------------------------------------------
// Send one 8-bit value using our simple protocol
// -----------------------------------------------------------------------------
void ir_send_simple(uint8_t data) {
    // Header
    mark(SIMPLE_HDR_MARK);
    space(SIMPLE_HDR_SPACE);

    // 8 data bits, LSB first
    for (int i = 0; i < 8; i++) {
        mark(SIMPLE_BIT_MARK);
        if (data & 1)
            space(SIMPLE_ONE_SPACE);
        else
            space(SIMPLE_ZERO_SPACE);
        data >>= 1;
    }
    // Final stop pulse
    mark(SIMPLE_BIT_MARK);
}

int main(void) {
    stdio_init_all();
    printf("Starting Simple IR Transmitter...\n");
    ir_pwm_init();
    uint8_t data_to_send = 123;

    while (true) {
        printf("Sending data: %d\n", data_to_send);
        ir_send_simple(data_to_send);
        sleep_ms(1000);
    }
    return 0;
}