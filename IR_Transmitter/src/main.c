

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include <stdio.h>
#include <stdint.h>

#define TX_PIN 36  // IR LED pin (PWM slice 10A)

// === NEC Timing (microseconds) ===
#define NEC_UNIT       560
#define NEC_HDR_MARK   (16 * NEC_UNIT)
#define NEC_HDR_SPACE  (8  * NEC_UNIT)
#define NEC_BIT_MARK   NEC_UNIT
#define NEC_ONE_SPACE  (3 * NEC_UNIT)
#define NEC_ZERO_SPACE NEC_UNIT
#define NEC_STOP_BIT   NEC_UNIT
#define NEC_GAP_MS     110

// --- simple helpers ---
static inline void pwm_on(uint slice)  { pwm_set_enabled(slice, true); }
static inline void pwm_off(uint slice) { pwm_set_enabled(slice, false); }

void mark(uint slice, uint32_t usec) {
    pwm_on(slice);
    sleep_us(usec);
    pwm_off(slice);
}

void space(uint slice, uint32_t usec) {
    pwm_off(slice);  // ✅ fixed — correct argument
    if (usec) sleep_us(usec);
}

// === Build 32-bit NEC frame ===
uint32_t nec_build(uint8_t addr, uint8_t cmd) {
    uint32_t data = 0;
    data  = addr;
    data |= ((uint32_t)(~addr) & 0xFF) << 8;
    data |= ((uint32_t)cmd & 0xFF) << 16;
    data |= ((uint32_t)(~cmd) & 0xFF) << 24;
    return data;
}

// === Transmit NEC packet ===
void nec_send(uint slice, uint8_t addr, uint8_t cmd) {
    uint32_t frame = nec_build(addr, cmd);

    mark(slice, NEC_HDR_MARK);
    space(slice, NEC_HDR_SPACE);

    for (int i = 0; i < 32; i++) {
        mark(slice, NEC_BIT_MARK);
        if (frame & (1u << i))
            space(slice, NEC_ONE_SPACE);
        else
            space(slice, NEC_ZERO_SPACE);
    }

    mark(slice, NEC_STOP_BIT);
    space(slice, 0);
}

int main() {
    stdio_init_all();
    sleep_ms(3000);  // allow USB serial to come up

    printf("\n=== RP2350 NEC IR Transmitter ===\n");

    // PWM setup for 38kHz carrier
    gpio_set_function(TX_PIN, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(TX_PIN);
    float div = 125000000.0f / (38000.0f * 64.0f);
    pwm_set_clkdiv(slice, div);
    pwm_set_wrap(slice, 63);
    pwm_set_chan_level(slice, PWM_CHAN_A, 21); // ~33% duty
    pwm_set_enabled(slice, false);

    printf("PWM slice=%u, div=%.3f (≈38kHz)\n\n", slice, div);

    uint8_t addr = 1;
    uint8_t cmd = 1;

    while (true) {
        printf("Sending NEC packet: addr=%d, cmd=%d\n", addr, cmd);
        nec_send(slice, addr, cmd);

        // Cycle addr 1–4, cmd 1–4
        addr++;
        cmd++;
        if (addr > 4) addr = 1;
        if (cmd > 4) cmd = 1;

        sleep_ms(NEC_GAP_MS);
    }
}
