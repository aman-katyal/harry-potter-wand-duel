/**
 * @file ei_bridge.cpp
 * @brief C++ Wrapper for Edge Impulse SDK with GMM Anomaly Detection
 */

#include "ei_bridge.h"
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// --- Configuration ---
// Thresholds for decision making
#define CONFIDENCE_THRESHOLD  0.8f
#define ANOMALY_THRESHOLD     1.8f  // Lower = Stricter, Higher = Looser

// =============================================================================
// --- Edge Impulse Porting Hooks ---
// =============================================================================

void ei_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

void ei_printf_float(float f) {
    printf("%f", f);
}

void *ei_malloc(size_t size) {
    return malloc(size);
}

void *ei_calloc(size_t nitems, size_t size) {
    return calloc(nitems, size);
}

void ei_free(void *ptr) {
    free(ptr);
}

uint64_t ei_read_timer_ms() {
    return to_ms_since_boot(get_absolute_time());
}

uint64_t ei_read_timer_us() {
    return to_us_since_boot(get_absolute_time());
}

EI_IMPULSE_ERROR ei_sleep(int32_t time_ms) {
    sleep_ms(time_ms);
    return EI_IMPULSE_OK;
}

EI_IMPULSE_ERROR ei_run_impulse_check_canceled() {
    return EI_IMPULSE_OK;
}

// =============================================================================
// --- Bridge Implementation ---
// =============================================================================
extern "C" {

    // Global pointer to the current data being processed
    // This allows the signal callback to access the buffer without copying
    static float *g_inference_buffer = NULL;

    // Callback function required by EI SDK to fetch data
    static int get_signal_data(size_t offset, size_t length, float *out_ptr) {
        if (!g_inference_buffer) return -1;
        
        // Copy from our flat exchange buffer into the SDK's processing buffer
        // The exchange buffer is already arranged as [x,y,z,r,p,y, x,y,z,r,p,y...]
        for (size_t i = 0; i < length; i++) {
            out_ptr[i] = g_inference_buffer[offset + i];
        }
        return EIDSP_OK;
    }

    size_t ei_bridge_get_input_frame_size(void) {
        return EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
    }
    
    int ei_bridge_get_axis_count(void) {
        return EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME;
    }

    spell_decision_t ei_bridge_run_inference(float *raw_buffer, size_t length) {
        spell_decision_t output = { 0 };
        output.detected = false;
        output.label = "unknown";
        output.confidence = 0.0f;
        output.anomaly_score = 0.0f;

        // 1. Setup Signal
        g_inference_buffer = raw_buffer;
        signal_t signal;
        signal.total_length = length;
        signal.get_data = &get_signal_data;

        // 2. Run Classifier
        ei_impulse_result_t result = { 0 };
        EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

        if (res != EI_IMPULSE_OK) {
            ei_printf("ERR: Inference failed (%d)\n", res);
            return output;
        }

        output.dsp_time_ms = result.timing.dsp;
        output.nn_time_ms = result.timing.classification;
        output.anomaly_score = result.anomaly; // Read GMM score

        // 3. Process Results
        // First check: Is this an anomaly? (e.g., random flailing)
        // if (result.anomaly > ANOMALY_THRESHOLD) {
        //     // High anomaly score means the motion didn't look like ANY known spell training data.
        //     // Even if the classifier thinks it's 80% Aguamenti, we reject it.
        //     output.label = "anomaly";
        //     output.detected = false;
        //     return output;
        // }

        // Second check: Find the best label
        float best_val = 0.0f;
        int best_idx = -1;

        for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
            if (result.classification[i].value > best_val) {
                best_val = result.classification[i].value;
                best_idx = i;
            }
        }

        if (best_idx >= 0) {
            output.label = ei_classifier_inferencing_categories[best_idx];
            output.confidence = best_val;
            
            // Third check: Confidence Threshold
            if (best_val > CONFIDENCE_THRESHOLD) {
                output.detected = true;
            }
        }

        return output;
    }
}