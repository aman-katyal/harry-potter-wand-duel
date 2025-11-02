#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"

const uint IR_PIN = 17;

#define MAX_TIMINGS 40 // A message is only ~18 timings, 40 is safe
#define END_OF_MESSAGE_THRESHOLD 5000 // 5ms gap ends a message

// ----- Simple Protocol Timings (microseconds) with wide tolerance -----
#define HDR_MARK_MIN   1500
#define HDR_MARK_MAX   2500
#define ONE_SPACE_MIN   750 // Anything over 750us is a '1'
#define ZERO_SPACE_MAX  750 // Anything under 750us is a '0'

#define CARRIER_TIMEOUT_US 45 

enum ir_state { STATE_SPACE, STATE_MARK };

volatile absolute_time_t last_pulse_time;
uint32_t ir_timings[MAX_TIMINGS];
uint8_t timing_index = 0;
volatile bool message_ready = false;

void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == IR_PIN) last_pulse_time = get_absolute_time();
}

bool decode_simple(uint8_t *data) {
    // A message is a header + 8 bits = 1 mark + 1 space + 8*(mark+space) = 18 timings
    if (timing_index < 18) return false;

    // Check for the unique long header pulse
    if (ir_timings[0] < HDR_MARK_MIN || ir_timings[0] > HDR_MARK_MAX) return false;

    uint8_t received_value = 0;
    for (int i = 0; i < 8; i++) {
        // We only care about the SPACE after each bit's MARK pulse
        uint32_t bit_space = ir_timings[3 + (i * 2)];

        if (bit_space > ONE_SPACE_MIN) {
            received_value |= (1 << i); // It's a '1'
        }
        // If it's a short space, it's a '0', so we do nothing
    }
    *data = received_value;
    return true;
}

int main() {
    stdio_init_all();
    sleep_ms(2000);
    printf("Simple IR Receiver Ready...\n");

    gpio_init(IR_PIN);
    gpio_set_dir(IR_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(IR_PIN, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
    irq_set_enabled(IO_IRQ_BANK0, true);

    last_pulse_time = get_absolute_time();
    enum ir_state current_state = STATE_SPACE;
    absolute_time_t last_state_change_time = get_absolute_time();

    while (1) {
        absolute_time_t now = get_absolute_time();
        uint32_t time_since_last_pulse = absolute_time_diff_us(last_pulse_time, now);
        enum ir_state new_state = (time_since_last_pulse < CARRIER_TIMEOUT_US) ? STATE_MARK : STATE_SPACE;

        if (new_state != current_state) {
            uint32_t duration = absolute_time_diff_us(last_state_change_time, now);
            if (timing_index < MAX_TIMINGS) ir_timings[timing_index++] = duration;
            current_state = new_state;
            last_state_change_time = now;
        }
        
        if (current_state == STATE_SPACE && timing_index > 0 && !message_ready) {
            if (absolute_time_diff_us(last_state_change_time, now) > END_OF_MESSAGE_THRESHOLD) {
                message_ready = true;
            }
        }
        
        if (message_ready) {
            uint8_t received_data;
            if (decode_simple(&received_data)) {
                printf("Received data: %d\n", received_data);
            } else {
                printf("Failed to decode signal.\n");
            }
            timing_index = 0;
            message_ready = false;
        }
    }
}