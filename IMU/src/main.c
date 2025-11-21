/**
 * @file main.c
 * @brief BNO08x spell classifier - Multicore Version
 * 
 * Core 0: Handles USB Output (Printf)
 * Core 1: Handles Sensor I2C, Data Collection, and Neural Network Inference
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/multicore.h" // Required for multicore
#include "hardware/i2c.h"
#include "hardware/timer.h"

// Include C drivers
#include "bno08x_driver.h"
#include "ei_bridge.h" 

// ---------------------------------------------------------------------------
// USER CONFIG SECTION
// ---------------------------------------------------------------------------
#define SAMPLING_FREQUENCY_HZ       74
#define SENSOR_POLL_INTERVAL_US     (1000000 / SAMPLING_FREQUENCY_HZ)
#define SENSOR_REPORT_INTERVAL_US   (1000000 / SAMPLING_FREQUENCY_HZ)

#define MOVING_WINDOW_MS            2000    // 2 seconds
#define INFERENCE_INTERVAL_MS       250     // 0.25 seconds (stride)

#define I2C_PORT                    i2c0
#define I2C_SDA_PIN                 16
#define I2C_SCL_PIN                 17
#define I2C_BAUDRATE                (400 * 1000)
#define BNO08X_RESET_PIN            -1
#define BNO08X_I2C_ADDR             BNO08x_I2CADDR_DEFAULT

#define EI_YPR_IN_DEGREES           1

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// ---------------------------------------------------------------------------
// Globals (Shared Memory)
// ---------------------------------------------------------------------------
typedef struct { float yaw, pitch, roll; } euler_t;

// These are accessed primarily by Core 1 (ISR and Main Loop)
static bno08x_driver_t bno08x;
static volatile euler_t g_ypr = {0};
static volatile sh2_Accelerometer_t g_acc = {0};
static volatile bool g_sensor_data_updated = false;

// Buffer for Moving Window
static float *features_buffer = NULL;
static size_t dsp_input_frame_size = 0;
static int raw_axis_count = 0;

// ---------------------------------------------------------------------------
// Prototypes
// ---------------------------------------------------------------------------
void quaternion_to_euler(sh2_RotationVectorWAcc_t* rv, euler_t* ypr, bool degrees);
void set_reports(bno08x_driver_t *driver);

// ---------------------------------------------------------------------------
// ISR (Runs on Core 1)
// ---------------------------------------------------------------------------
bool sensor_poll_callback(repeating_timer_t *t) {
    static sh2_SensorValue_t sensor_value;

    // Because this timer is initialized on Core 1, this ISR runs on Core 1.
    // It is safe to access bno08x here.
    if (bno08x_get_sensor_event(&bno08x, &sensor_value)) {
        switch (sensor_value.sensorId) {
            case SH2_ARVR_STABILIZED_RV:
                quaternion_to_euler(&sensor_value.un.arvrStabilizedRV, (euler_t*)&g_ypr, EI_YPR_IN_DEGREES);
                break;
            case SH2_ACCELEROMETER:
                memcpy((void*)&g_acc, &sensor_value.un.accelerometer, sizeof(sh2_Accelerometer_t));
                break;
            default: break;
        }
        g_sensor_data_updated = true;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Core 1 Entry Point - Worker Thread
// ---------------------------------------------------------------------------
void core1_entry() {
    static int samples_collected = 0;
    static uint64_t last_inference_time = 0;
    static repeating_timer_t timer;

    // 1. Initialize I2C and Sensor (Must be done on the core that uses them)
    i2c_init(I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    if (!bno08x_begin_i2c(&bno08x, I2C_PORT, BNO08X_I2C_ADDR, BNO08X_RESET_PIN)) {
        // We can't print easily from here without locking, so we just hang or blink
        // Ideally send error code to Core 0, but keeping it simple.
        while(1) tight_loop_contents();
    }
    set_reports(&bno08x);

    // 2. Init Bridge Config
    dsp_input_frame_size = ei_bridge_get_input_frame_size();
    raw_axis_count = ei_bridge_get_axis_count();
    
    // Allocate memory
    features_buffer = (float*)calloc(dsp_input_frame_size, sizeof(float));
    if (!features_buffer) {
        while(1) tight_loop_contents();
    }

    int last_index = dsp_input_frame_size - raw_axis_count;
    int steps_per_window = dsp_input_frame_size / raw_axis_count;
    
    // 3. Start Timer (Runs on Core 1)
    add_repeating_timer_us(SENSOR_POLL_INTERVAL_US, sensor_poll_callback, NULL, &timer);

    // 4. Main Worker Loop
    while (true) {
        if (g_sensor_data_updated) {
            g_sensor_data_updated = false;

            // Shift Buffer Left
            memmove(features_buffer, 
                    features_buffer + raw_axis_count, 
                    last_index * sizeof(float));

            // Insert New Data
            features_buffer[last_index + 0] = g_acc.x;
            features_buffer[last_index + 1] = g_acc.y;
            features_buffer[last_index + 2] = g_acc.z;
            features_buffer[last_index + 3] = g_ypr.roll;
            features_buffer[last_index + 4] = g_ypr.pitch;
            features_buffer[last_index + 5] = g_ypr.yaw;

            // Warmup Check
            if (samples_collected < steps_per_window) {
                samples_collected++;
                continue;
            }

            // Inference Timer Check
            uint64_t now = to_ms_since_boot(get_absolute_time());
            if ((now - last_inference_time) >= INFERENCE_INTERVAL_MS) {
                
                // Run Inference (Heavy computation)
                spell_decision_t result = ei_bridge_run_inference(features_buffer, dsp_input_frame_size);
                last_inference_time = now;

                // Send Result to Core 0
                // FIFO is 32-bit, so we send the ADDRESS of our local struct.
                // NOTE: This blocks Core 1 until Core 0 reads it.
                multicore_fifo_push_blocking((uint32_t)&result);

                // Wait for Core 0 to acknowledge it is done printing
                // This prevents Core 1 from overwriting 'result' while Core 0 reads it.
                multicore_fifo_pop_blocking();
            }
        }
        else {
             tight_loop_contents();
        }
    }
}

// ---------------------------------------------------------------------------
// Core 0 Entry Point - Main / Printer
// ---------------------------------------------------------------------------
int main() {
    // 1. Init Stdio (USB/UART)
    stdio_init_all();
    sleep_ms(2000); // Wait for serial monitor
    printf("=== Multicore Edge Impulse Classifier ===\n");
    printf("Core 0: UI/Print | Core 1: Sensor/Inference\n");

    // 2. Launch Core 1
    multicore_launch_core1(core1_entry);

    // 3. Print Loop
    while (true) {
        // Wait for data from Core 1
        uint32_t raw_ptr = multicore_fifo_pop_blocking();
        
        // Cast back to struct pointer
        // (Safe because both cores share SRAM)
        spell_decision_t *decision = (spell_decision_t*)raw_ptr;

        // Print results
        printf("Inference (%d ms): ", decision->dsp_time_ms + decision->nn_time_ms);
        if (decision->detected) {
            printf("DETECTED: %s (%.2f)\n", decision->label, decision->confidence);
        } else {
            printf("%s (%.2f)\n", decision->label, decision->confidence);
        }

        // Send Acknowledge back to Core 1 so it can resume
        multicore_fifo_push_blocking(1);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
void set_reports(bno08x_driver_t *driver) {
    bno08x_enable_report(driver, SH2_ARVR_STABILIZED_RV, SENSOR_REPORT_INTERVAL_US);
    bno08x_enable_report(driver, SH2_ACCELEROMETER, SENSOR_REPORT_INTERVAL_US);
}

void quaternion_to_euler(sh2_RotationVectorWAcc_t* rv, euler_t* ypr, bool degrees) {
    float qr = rv->real; float qi = rv->i; float qj = rv->j; float qk = rv->k;
    float sqr = qr * qr; float sqi = qi * qi; float sqj = qj * qj; float sqk = qk * qk;

    ypr->yaw = atan2f(2.0f * (qi * qj + qk * qr), (sqi - sqj - sqk + sqr));
    ypr->pitch = asinf(-2.0f * (qi * qk - qj * qr) / (sqi + sqj + sqk + sqr));
    ypr->roll = atan2f(2.0f * (qj * qk + qi * qr), (-sqi - sqj + sqk + sqr));

    if (degrees) {
        float rad2deg = 180.0f / M_PI;
        ypr->yaw *= rad2deg; ypr->pitch *= rad2deg; ypr->roll *= rad2deg;
    }
}