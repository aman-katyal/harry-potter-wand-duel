/**
 * @file ei_bridge.cpp
 * @brief C++ Wrapper for Edge Impulse SDK
 */

#include "ei_bridge.h"
#include "edge-impulse-sdk/classifier/ei_run_classifier.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// =============================================================================
// --- Edge Impulse Porting Hooks (Hardware Abstraction Layer) ---
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
// --- Bridge Implementation (Extern "C") ---
// =============================================================================
extern "C" {

    static float *g_inference_buffer = NULL;

    static int get_signal_data(size_t offset, size_t length, float *out_ptr) {
        if (!g_inference_buffer) return -1; // Error: buffer not set
        
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

        g_inference_buffer = raw_buffer;

        signal_t signal;
        signal.total_length = length;
        signal.get_data = &get_signal_data;

        ei_impulse_result_t result = { 0 };

        EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

        if (res != EI_IMPULSE_OK) {
            ei_printf("ERR: Inference failed (%d)\n", res);
            return output;
        }

        output.dsp_time_ms = result.timing.dsp;
        output.nn_time_ms = result.timing.classification;

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
            if (best_val > 0.7f) {
                output.detected = true;
            }
        }

        return output;
    }
}