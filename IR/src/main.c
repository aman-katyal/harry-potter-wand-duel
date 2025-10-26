#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"

// =============================================================
// === Emitter Configuration ===
// =============================================================
#define IR_GPIO       36
#define PWM_FREQ      38000
#define DUTY_CYCLE    0.33f

#define NEC_HDR_MARK   9000
#define NEC_HDR_SPACE  4500
#define NEC_BIT_MARK   560
#define NEC_ONE_SPACE  1690
#define NEC_ZERO_SPACE 560
#define NEC_STOP_MARK  560

// =============================================================
// === Receiver Configuration ===
// =============================================================
#define IR_RX_PIN 15

volatile uint32_t ir_buffer[100];
volatile int buf_len = 0;
volatile uint32_t last_transition_time = 0;
volatile bool carrier_active = false;

// =============================================================
// === Emitter (Core 1) ===
// =============================================================
void ir_pwm_init(void) {
    gpio_set_function(IR_GPIO, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(IR_GPIO);
    pwm_set_clkdiv(slice, 150);               // 125 MHz / 150 ≈ 833 kHz
    uint32_t period = (1000000 / PWM_FREQ) - 1;
    pwm_set_wrap(slice, period);
    pwm_set_chan_level(slice, PWM_CHAN_A, (uint32_t)(period * DUTY_CYCLE));
    pwm_set_enabled(slice, false);
}

void ir_mark(uint32_t usec, uint slice) {
    pwm_set_enabled(slice, true);
    sleep_us(usec);
    pwm_set_enabled(slice, false);
}

void ir_space(uint32_t usec) {
    sleep_us(usec);
}

void ir_send_nec(uint32_t data, int nbits) {
    uint slice = pwm_gpio_to_slice_num(IR_GPIO);

    // Header
    ir_mark(NEC_HDR_MARK, slice);
    ir_space(NEC_HDR_SPACE);

    // Data bits (LSB first)
    for (int i = 0; i < nbits; i++) {
        if (data & 1)
            { ir_mark(NEC_BIT_MARK, slice); ir_space(NEC_ONE_SPACE); }
        else
            { ir_mark(NEC_BIT_MARK, slice); ir_space(NEC_ZERO_SPACE); }
        data >>= 1;
    }

    // Stop bit
    ir_mark(NEC_STOP_MARK, slice);
    ir_space(0);
}

void core1_entry() {
    ir_pwm_init();
    uint32_t code = 0x20DF10EF;  // Example NEC code

    while (1) {
        printf("[Core1] Sending NEC 0x%08lX\n", code);
        ir_send_nec(code, 32);
        sleep_ms(1000);
    }
}

// =============================================================
// === Receiver (Core 0) ===
// =============================================================

// ISR: convert raw 38 kHz edges → envelope timing
void gpio_irq_handler(uint gpio, uint32_t events) {
    uint32_t now = time_us_32();
    uint32_t delta = now - last_transition_time;

    // Ignore the first long idle period or reset between frames
    if (delta > 20000) {  // > 20 ms = new frame
        buf_len = 0;
        carrier_active = false;
        last_transition_time = now;
        return;
    }

    last_transition_time = now;

    // Detect carrier start/end transitions (gaps > 0.5 ms)
    if (carrier_active && delta > 500) {
        // End of mark
        if (buf_len < 100)
            ir_buffer[buf_len++] = delta;
        carrier_active = false;
    } 
    else if (!carrier_active && delta > 500) {
        // Start of new mark
        if (buf_len < 100)
            ir_buffer[buf_len++] = delta;
        carrier_active = true;
    }
}

// Decode the captured envelope as NEC frame
void decode_nec() {
    if (buf_len < 10) return;
    printf("Raw pulse count = %d\n", buf_len);

    for (int i = 0; i < 10 && i < buf_len; i++)
        printf("%d: %lu us\n", i, ir_buffer[i]);

    // Skip bogus first element if huge
    int start = (ir_buffer[0] > 20000) ? 1 : 0;

    // Header check
    if (ir_buffer[start] < 6000 || ir_buffer[start] > 10000) {
        printf("Header mark out of range (%lu us)\n", ir_buffer[start]);
        return;
    }
    if (ir_buffer[start + 1] < 2500 || ir_buffer[start + 1] > 6000) {
        printf("Header space out of range (%lu us)\n", ir_buffer[start + 1]);
        return;
    }

    // Decode 32 bits
    uint32_t data = 0;
    int bit_index = 0;
    for (int i = start + 2; i + 1 < buf_len && bit_index < 32; i += 2) {
        uint32_t space = ir_buffer[i + 1];
        if (space > 1200)  // “1”
            data |= (1UL << bit_index);
        bit_index++;
    }

    printf("Decoded NEC: 0x%08lX\n", data);
}

// =============================================================
// === Main (Receiver Core 0) ===
// =============================================================
int main() {
    stdio_init_all();
    printf("RP2350 IR TX + RX Test (NEC / TSMP96000)\n");

    // Receiver setup
    gpio_init(IR_RX_PIN);
    gpio_set_dir(IR_RX_PIN, GPIO_IN);
    gpio_pull_up(IR_RX_PIN);
    gpio_set_irq_enabled_with_callback(
        IR_RX_PIN,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
        true,
        &gpio_irq_handler
    );

    // Launch transmitter on Core 1
    multicore_launch_core1(core1_entry);

    // Decode loop
    while (true) {
        sleep_ms(2000);
        if (buf_len > 0) {
            decode_nec();
            buf_len = 0;
            carrier_active = false;
            last_transition_time = 0;
        }
    }
}
