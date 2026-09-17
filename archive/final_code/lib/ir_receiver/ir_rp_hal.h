/*
 * ir_rp_hal.h
 *
 * Internal Hardware Abstraction Layer for the RP2040/RP2350.
 * Defines the ISR state machine and core timing logic.
 *
 * This file is part of a C conversion of the Arduino-IRremote library.
 */

#ifndef IR_RP_HAL_H
#define IR_RP_HAL_H

#include "irremote.h"
#include <stdint.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

/*
 * ISR State-Machine: Receiver States
 */
typedef enum {
    IR_STATE_IDLE,  // Waiting for the first mark
    IR_STATE_MARK,  // Measuring a mark pulse
    IR_STATE_SPACE, // Measuring a space pulse
    IR_STATE_STOP   // Frame received, waiting for decode
} ir_state_t;

/**
 * @brief Buffer size for raw pulse timings.
 * NEC protocol has 67 pulses (Header Mark + Space, 32* (BitMark + Space), StopMark).
 * 100 is a safe value.
 */
#define IR_RAW_BUFFER_LEN 100

/**
 * @brief Timeout to detect the end of a frame (gap).
 * 8000 us (8 ms) is a safe value, longer than any NEC space but
 * shorter than the repeat gap.
 */
#define IR_GAP_TIMEOUT_US 8000

/**
 * @brief Tolerance for matching pulse widths (in percent).
 * 25% is the default from the original library.
 */
#define IR_DECODE_TOLERANCE_PERCENT 25

/**
 * @brief Compensation for distortion introduced by the IR receiver module.
 * 20 us is the default from the original library.
 */
#define IR_MARK_EXCESS_US 20

/**
 * @brief Main structure holding the ISR state and raw data.
 */
typedef struct {
    // Pin and State
    uint8_t receive_pin;
    volatile ir_state_t state;

    // Timestamping
    volatile uint64_t last_event_time_us;

    // Raw data buffer
    volatile uint16_t raw_len;
    volatile bool overflow;
    volatile uint32_t raw_buf[IR_RAW_BUFFER_LEN];

} ir_params_t;

/**
 * @brief Global instance of the receiver state.
 */
extern ir_params_t g_ir_params;

/**
 * @brief The GPIO IRQ handler function.
 * This is the core of the event-driven receiver.
 */
void ir_gpio_callback(uint gpio, uint32_t event_mask);

/**
 * @brief Compares a measured timing against a target timing with tolerance.
 * @param measured_us The measured pulse width in microseconds.
 * @param target_us The target pulse width in microseconds.
 * @return true if the measured value is within tolerance of the target.
 */
bool ir_match_timing(uint32_t measured_us, uint32_t target_us);

/**
 * @brief Compares a measured mark timing against a target, applying compensation.
 */
bool ir_match_mark(uint32_t measured_us, uint32_t target_us);

/**
 * @brief Compares a measured space timing against a target, applying compensation.
 */
bool ir_match_space(uint32_t measured_us, uint32_t target_us);

#endif // IR_RP_HAL_H