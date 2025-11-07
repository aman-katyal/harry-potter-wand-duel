/*
 * ir_rp_hal.c
 *
 * C implementation of the RP2040/RP2350 Hardware Abstraction Layer.
 *
 * This file is part of a C conversion of the Arduino-IRremote library.
 */

#include "ir_rp_hal.h"
#include "pico/critical_section.h"
#include "ir_decode_nec.h" // Include the decoder header

// Define the global state variable
ir_params_t g_ir_params;

// Define a critical section for safe access to ir_params from main loop
static critical_section_t g_ir_crit_sec;

// in ir_rp_hal.c
void ir_gpio_callback(uint gpio, uint32_t event_mask) {
    // --- 1. Timestamping ---
    uint64_t now_us = time_us_64();
    uint32_t duration_us = (uint32_t)(now_us - g_ir_params.last_event_time_us);
    g_ir_params.last_event_time_us = now_us;

    if (gpio != g_ir_params.receive_pin) {
        return;
    }

    // --- 2. State Machine Logic ---
    switch (g_ir_params.state) {
        case IR_STATE_IDLE:
            if (event_mask & GPIO_IRQ_EDGE_FALL) {
                g_ir_params.raw_len = 0;
                g_ir_params.overflow = false;
                g_ir_params.state = IR_STATE_MARK;
            }
            break;

        case IR_STATE_MARK:
            if (event_mask & GPIO_IRQ_EDGE_RISE) {
                if (g_ir_params.raw_len >= IR_RAW_BUFFER_LEN) {
                    g_ir_params.overflow = true;
                    g_ir_params.state = IR_STATE_STOP;
                } else {
                    g_ir_params.raw_buf[g_ir_params.raw_len++] = duration_us;
                    g_ir_params.state = IR_STATE_SPACE;
                }
            }
            break;

        case IR_STATE_SPACE:
            if (event_mask & GPIO_IRQ_EDGE_FALL) {
                if (duration_us > IR_GAP_TIMEOUT_US) {
                    // GAP found. Frame is complete and in the buffer.
                    // This new FALL is the start of the *next* frame.
                    g_ir_params.state = IR_STATE_STOP; // Signal main loop to decode
                } else {
                    if (g_ir_params.raw_len >= IR_RAW_BUFFER_LEN) {
                        g_ir_params.overflow = true;
                        g_ir_params.state = IR_STATE_STOP;
                    } else {
                        g_ir_params.raw_buf[g_ir_params.raw_len++] = duration_us;
                        g_ir_params.state = IR_STATE_MARK;
                    }
                }
            }
            break;

        case IR_STATE_STOP:
            // --- FIX FOR RACE CONDITION ---
            // If an edge comes in while we are stopped, it means the main loop
            // was too slow to call resume(). We handle it here to avoid losing sync.
            // This is the "self-resuming" mechanism.
            if (event_mask & GPIO_IRQ_EDGE_RISE) {
                // This is the rising edge of the new frame's header mark.
                // We must start the new capture *now*.
                g_ir_params.raw_len = 0;
                g_ir_params.overflow = false;
                g_ir_params.raw_buf[g_ir_params.raw_len++] = duration_us; // Save the mark we just measured.
                g_ir_params.state = IR_STATE_SPACE; // We are now in the header space.
            }
            // We ignore any falling edges while in the STOP state.
            break;
    }
}

/**
 * @brief Initializes the IR receiver.
 */
void ir_receiver_init(uint8_t receive_pin) {
    g_ir_params.receive_pin = receive_pin;

    // Initialize critical section
    critical_section_init(&g_ir_crit_sec);

    gpio_init(receive_pin);
    gpio_set_dir(receive_pin, GPIO_IN);
    gpio_pull_up(receive_pin); // IR receivers are typically open-drain, active low.

    // Use the exact function provided by the user.
    gpio_set_irq_enabled_with_callback(
        receive_pin,
        GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL,
        true,
        &ir_gpio_callback
    );

    // Set initial state
    critical_section_enter_blocking(&g_ir_crit_sec);
    // We must initialize last_event_time_us *once* at init.
    g_ir_params.last_event_time_us = time_us_64(); 
    g_ir_params.state = IR_STATE_IDLE;
    critical_section_exit(&g_ir_crit_sec);
}

/**
 * @brief Attempts to decode the most recently received IR frame.
 */
bool ir_receiver_decode(ir_decoded_data_t *decoded_data) {
    if (g_ir_params.state != IR_STATE_STOP) {
        return false; // Not ready to decode
    }

    // --- Atomically copy the volatile data ---
    ir_params_t params_copy;
    critical_section_enter_blocking(&g_ir_crit_sec);
    params_copy = g_ir_params;
    critical_section_exit(&g_ir_crit_sec);
    
    // Initialize output data
    decoded_data->protocol = IR_PROTOCOL_UNKNOWN;
    decoded_data->address = 0;
    decoded_data->command = 0;
    decoded_data->flags = 0;

    if (params_copy.overflow) {
        decoded_data->flags = IRDATA_FLAGS_WAS_OVERFLOW;
    }

    // --- Pass to NEC decoder ---
    if (decode_nec_protocol(decoded_data, &params_copy)) {
        // Success
    } else {
        // Failed to decode as NEC
        decoded_data->protocol = IR_PROTOCOL_UNKNOWN;
    }
    
    // User must call ir_receiver_resume() to start next capture
    return true;
}
// in ir_rp_hal.c
void ir_receiver_resume(void) {
    critical_section_enter_blocking(&g_ir_crit_sec);
    
    // We only reset the state if the ISR has not already "self-resumed".
    // If the state is still STOP, it means no new edges have arrived and it's
    // safe for us to prepare for the next frame.
    if (g_ir_params.state == IR_STATE_STOP) {
        g_ir_params.raw_len = 0;
        g_ir_params.overflow = false;
        
        // The falling edge that triggered the STOP state was the start of the
        // new header mark. We now set the state to MARK and wait for the
        // rising edge.
        g_ir_params.state = IR_STATE_MARK;
    }
    
    // If the state is NOT STOP, it means the ISR's self-resume logic has
    // already triggered and started the next capture. In that case, we do
    // nothing, because the ISR is already in control.
    
    critical_section_exit(&g_ir_crit_sec);
}
/**
 * @brief Checks if the receiver state machine is idle or stopped.
 */
bool ir_receiver_is_idle(void) {
    bool is_idle;
    critical_section_enter_blocking(&g_ir_crit_sec);
    is_idle = (g_ir_params.state == IR_STATE_IDLE || g_ir_params.state == IR_STATE_STOP);
    critical_section_exit(&g_ir_crit_sec);
    return is_idle;
}


// --- Timing Match Functions ---

/**
 * @brief Compares a measured timing against a target timing with tolerance.
 */
bool ir_match_timing(uint32_t measured_us, uint32_t target_us) {
    // Calculate 75% and 125% of the target timing
    uint32_t lower_bound_us = (target_us * (100 - IR_DECODE_TOLERANCE_PERCENT)) / 100;
    uint32_t upper_bound_us = (target_us * (100 + IR_DECODE_TOLERANCE_PERCENT)) / 100;

    return (measured_us >= lower_bound_us) && (measured_us <= upper_bound_us);
}

/**
 * @brief Compares a measured mark timing against a target, applying compensation.
 * Mark pulses are measured *shorter* than they are, so we subtract
 * the excess from the *target* to match the measurement.
 */
bool ir_match_mark(uint32_t measured_us, uint32_t target_us) {
    return ir_match_timing(measured_us, target_us - IR_MARK_EXCESS_US);
}

/**
 * @brief Compares a measured space timing against a target, applying compensation.
 * Space pulses are measured *longer* than they are, so we add
 * the excess to the *target* to match the measurement.
 */
bool ir_match_space(uint32_t measured_us, uint32_t target_us) {
    return ir_match_timing(measured_us, target_us + IR_MARK_EXCESS_US);
}