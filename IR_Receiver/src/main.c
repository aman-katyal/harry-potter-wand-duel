#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"

const uint IR_PIN = 17;
#define MAX_RAW_TIMINGS 250
#define MAX_CLEAN_TIMINGS 100
#define END_OF_MESSAGE_THRESHOLD 10000
#define CARRIER_TIMEOUT_US 45 
#define CLEANING_THRESHOLD_US 30

enum ir_state { STATE_SPACE, STATE_MARK };
volatile absolute_time_t last_pulse_time;
uint32_t raw_timings[MAX_RAW_TIMINGS];
uint8_t raw_timing_index = 0;
volatile bool message_ready = false;

void gpio_callback(uint gpio, uint32_t events) {
    if (gpio == IR_PIN) last_pulse_time = get_absolute_time();
}

int clean_timings(const uint32_t* raw, int raw_count, uint32_t* cleaned) {
    int clean_count = 0;
    uint32_t accumulator = 0;
    for (int i = 1; i < raw_count; i++) {
        accumulator += raw[i];
        if (raw[i] > CLEANING_THRESHOLD_US) {
            if (clean_count < MAX_CLEAN_TIMINGS) cleaned[clean_count++] = accumulator;
            accumulator = 0;
        }
    }
    return clean_count;
}

int main() {
    stdio_init_all();
    sleep_ms(2000);
    printf("Pico IR Raw Code Cloner\n");
    printf("Point your ORIGINAL remote at the sensor and tap a button.\n\n");

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
            if (raw_timing_index < MAX_RAW_TIMINGS) raw_timings[raw_timing_index++] = duration;
            current_state = new_state;
            last_state_change_time = now;
        }
        
        if (current_state == STATE_SPACE && raw_timing_index > 0 && !message_ready) {
            if (absolute_time_diff_us(last_state_change_time, now) > END_OF_MESSAGE_THRESHOLD) {
                message_ready = true;
            }
        }
        
        if (message_ready) {
            uint32_t cleaned_data[MAX_CLEAN_TIMINGS];
            int cleaned_count = clean_timings(raw_timings, raw_timing_index, cleaned_data);

            // --- FORMAT THE OUTPUT FOR PASTING ---
            printf("\n--- Copy the following lines into the transmitter code ---\n\n");
            printf("#define RAW_DATA_LEN %d\n", cleaned_count + 1); // +1 for trailing space
            printf("uint16_t rawData[RAW_DATA_LEN]={\n\t");
            for(int i=0; i < cleaned_count; i++) {
              printf("%u, ", cleaned_data[i]);
              if( (i > 0) && ((i+1) % 8)==0) printf("\n\t");
            }
            printf("1000};\n"); // Add arbitrary trailing space like the example
            printf("\n--- End of data ---\n\n");

            raw_timing_index = 0;
            message_ready = false;
        }
    }
}