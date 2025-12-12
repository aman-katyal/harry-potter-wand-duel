/**
 * @file ei_bridge.h
 * @brief C-compatible interface for Edge Impulse SDK
 */

#ifndef EI_BRIDGE_H
#define EI_BRIDGE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Structure to hold inference results
typedef struct {
    const char *label;          // predicted label string (e.g., "aguamenti")
    float       confidence;     // prediction probability [0..1]
    float       anomaly_score;  // GMM Anomaly score
    int         dsp_time_ms;    // Debug timing
    int         nn_time_ms;     // Debug timing
} spell_decision_t;

/**
 * @brief Get the required buffer size (number of floats) for the model
 */
size_t ei_bridge_get_input_frame_size(void);

/**
 * @brief Run the classifier on the provided data buffer
 * @param raw_buffer Pointer to the float array containing sensor data
 * @param length Number of floats in the buffer
 * @return spell_decision_t containing the raw prediction stats
 */
spell_decision_t ei_bridge_run_inference(float *raw_buffer, size_t length);

#ifdef __cplusplus
}
#endif

#endif // EI_BRIDGE_H