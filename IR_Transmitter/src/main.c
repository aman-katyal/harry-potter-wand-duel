#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"

// The GPIO pin connected to the TSMP96000's output
const uint IR_PIN = 17;

// ----- Protocol and Timing Constants -----
#define MAX_TIMINGS 100
#define END_OF_MESSAGE_THRESHOLD 10000 // A 10ms gap signifies the end of a message

// Standard NEC Protocol Timings (in µs) with a +/- 25% tolerance
#define NEC_LEADER_PULSE_MIN 6750 // Nominal: 9000
#define NEC_LEADER_PULSE_MAX 11250
#define NEC_LEADER_SPACE_MIN 3375 // Nominal: 4500
#define NEC_LEADER_SPACE_MAX 5625

#define NEC_PULSE_MIN 420      // Nominal: 562
#define NEC_PULSE_MAX 700

#define NEC_ZERO_SPACE_MIN 420 // Nominal: 562
#define NEC_ZERO_SPACE_MAX 700
#define NEC_ONE_SPACE_MIN 1265 // Nominal: 1687
#define NEC_ONE_SPACE_MAX 2110

// ----- Software Demodulation Constants -----
// TUNED FOR 38kHz: The period is ~26.3µs. A timeout of 45µs is a safe threshold.
// This is also robust enough to work with your current 40kHz remote.
#define CARRIER_TIMEOUT_US 45 

// States for our state machine
enum ir_state {
    STATE_SPACE,
    STATE_MARK
};

// Global variables
volatile absolute_time_t last_pulse_time;
uint32_t ir_timings[MAX_TIMINGS];
uint8_t timing_index = 0;
volatile bool message_ready = false;

/**
 * @brief Simple interrupt callback. Updates the timestamp of the last pulse.
 */
void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == IR_PIN) {
        last_pulse_time = get_absolute_time();
    }
}

/**
 * @brief Decodes the received NEC IR timings into a 32-bit data value.
 */
bool decode_nec(uint32_t *data) {
    if (timing_index < 66) return false; 

    if (ir_timings[0] < NEC_LEADER_PULSE_MIN || ir_timings[0] > NEC_LEADER_PULSE_MAX ||
        ir_timings[1] < NEC_LEADER_SPACE_MIN || ir_timings[1] > NEC_LEADER_SPACE_MAX) {
        return false;
    }

    uint32_t decoded_value = 0;
    for (int i = 0; i < 32; i++) {
        uint32_t pulse_time = ir_timings[2 + (i * 2)];
        uint32_t space_time = ir_timings[3 + (i * 2)];

        if (pulse_time < NEC_PULSE_MIN || pulse_time > NEC_PULSE_MAX) return false;

        decoded_value <<= 1;
        if (space_time > NEC_ONE_SPACE_MIN && space_time < NEC_ONE_SPACE_MAX) {
            decoded_value |= 1;
        } else if (space_time > NEC_ZERO_SPACE_MIN && space_time < NEC_ZERO_SPACE_MAX) {
            // It's a '0'
        } else {
            return false;
        }
    }
    
    *data = decoded_value;
    return true;
}

int main() {
    stdio_init_all();
    sleep_ms(2000);
    printf("Tuned 38kHz IR NEC Decoder Ready...\n");
    printf("Tap a standard NEC remote button.\n");

    // Robust interrupt setup
    gpio_init(IR_PIN);
    gpio_set_dir(IR_PIN, GPIO_IN);
    gpio_set_irq_enabled_with_callback(IR_PIN, GPIO_IRQ_EDGE_RISE, true, &gpio_callback);
    irq_set_enabled(IO_IRQ_BANK0, true);

    last_pulse_time = get_absolute_time();
    enum ir_state current_state = STATE_SPACE;
    absolute_time_t last_state_change_time = get_absolute_time();

    while (1) {
        // --- Software Demodulation State Machine ---
        absolute_time_t now = get_absolute_time();
        uint32_t time_since_last_pulse = absolute_time_diff_us(last_pulse_time, now);
        enum ir_state new_state = (time_since_last_pulse < CARRIER_TIMEOUT_US) ? STATE_MARK : STATE_SPACE;

        if (new_state != current_state) {
            uint32_t duration = absolute_time_diff_us(last_state_change_time, now);
            if (timing_index < MAX_TIMINGS) {
                ir_timings[timing_index++] = duration;
            }
            current_state = new_state;
            last_state_change_time = now;
        }
        
        if (current_state == STATE_SPACE && timing_index > 0 && !message_ready) {
            if (absolute_time_diff_us(last_state_change_time, now) > END_OF_MESSAGE_THRESHOLD) {
                message_ready = true;
            }
        }
        
        if (message_ready) {
            uint32_t decoded_data;
            if (decode_nec(&decoded_data)) {
                uint8_t address = (decoded_data >> 24) & 0xFF;
                uint8_t command = (decoded_data >> 8) & 0xFF;
                printf("NEC Code: 0x%08lX (Addr: 0x%02X, Cmd: 0x%02X)\n", decoded_data, address, command);
            } else {
                printf("Failed to decode. (Is this an NEC remote?)\n");
            }

            timing_index = 0;
            message_ready = false;
        }
    }
}