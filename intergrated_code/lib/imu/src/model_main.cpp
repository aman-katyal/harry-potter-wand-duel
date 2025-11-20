// /**
//  * @file main.cpp
//  * @brief BNO08x gesture recognition with Moving Window on RP2350B
//  */

// #include <stdio.h>
// #include <string.h>
// #include <math.h>
// #include <stdlib.h> 
// #include "pico/stdlib.h"
// #include "hardware/i2c.h"
// #include "hardware/timer.h"

// extern "C" {
//     #include "bno08x_driver.h"
// }

// #include "edge-impulse-sdk/classifier/ei_run_classifier.h"

// // =================================================================================
// // --- Tweakable Parameters ---
// // =================================================================================
// #define SAMPLING_FREQUENCY_HZ     74
// #define SENSOR_POLL_INTERVAL_US   (1000000 / SAMPLING_FREQUENCY_HZ)
// #define SENSOR_REPORT_INTERVAL_US (1000000 / SAMPLING_FREQUENCY_HZ)

// // --- MOVING WINDOW PARAMETERS ---
// // NOTE: Your Edge Impulse "Window Size" must be set to 5000ms in the Studio!
// #define MOVING_WINDOW_MS          2000 

// // How often to output a decision (e.g., every 250ms)
// // This creates the "overlap". 
// #define INFERENCE_INTERVAL_MS     10000  

// #define I2C_PORT i2c0
// #define I2C_SDA_PIN 16
// #define I2C_SCL_PIN 17
// #define I2C_BAUDRATE 400 * 1000
// #define BNO08X_RESET_PIN -1
// #define BNO08X_I2C_ADDR BNO08x_I2CADDR_DEFAULT

// // =================================================================================
// // --- Global Variables ---
// // =================================================================================
// static bno08x_driver_t bno08x;
// typedef struct { float yaw, pitch, roll; } euler_t;

// static volatile euler_t g_ypr = {0};
// static volatile sh2_Accelerometer_t g_acc = {0};
// static volatile bool g_sensor_data_updated = false;

// // The main buffer for the moving window
// static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

// // Prototypes
// void quaternion_to_euler(sh2_RotationVectorWAcc_t* rotational_vector, euler_t* ypr, bool degrees);
// void set_reports(bno08x_driver_t *driver);

// // =================================================================================
// // --- Edge Impulse Helpers ---
// // =================================================================================
// void ei_printf(const char *format, ...) {
//     va_list args;
//     va_start(args, format);
//     vprintf(format, args);
//     va_end(args);
// }

// void ei_printf_float(float f) { printf("%f", f); }
// void *ei_malloc(size_t size) { return malloc(size); }
// void *ei_calloc(size_t nitems, size_t size) { return calloc(nitems, size); }
// void ei_free(void *ptr) { free(ptr); }
// uint64_t ei_read_timer_ms() { return to_ms_since_boot(get_absolute_time()); }
// uint64_t ei_read_timer_us() { return to_us_since_boot(get_absolute_time()); }
// EI_IMPULSE_ERROR ei_sleep(int32_t time_ms) { sleep_ms(time_ms); return EI_IMPULSE_OK; }
// EI_IMPULSE_ERROR ei_run_impulse_check_canceled() { return EI_IMPULSE_OK; }

// static int get_signal_data(size_t offset, size_t length, float *out_ptr) {
//     for (size_t i = 0; i < length; i++) {
//         out_ptr[i] = features[offset + i];
//     }
//     return EIDSP_OK;
// }

// // =================================================================================
// // --- Interrupt Service Routine ---
// // =================================================================================
// bool sensor_poll_callback(repeating_timer_t *t) {
//     static sh2_SensorValue_t sensor_value;
//     if (bno08x_get_sensor_event(&bno08x, &sensor_value)) {
//         switch (sensor_value.sensorId) {
//             case SH2_ARVR_STABILIZED_RV:
//                 quaternion_to_euler(&sensor_value.un.arvrStabilizedRV, (euler_t*)&g_ypr, false);
//                 break;
//             case SH2_ACCELEROMETER:
//                 memcpy((void*)&g_acc, &sensor_value.un.accelerometer, sizeof(sh2_Accelerometer_t));
//                 break;
//             default: break;
//         }
//         g_sensor_data_updated = true;
//     }
//     return true;
// }

// // =================================================================================
// // --- Main ---
// // =================================================================================
// int main() {
//     stdio_init_all();
//     sleep_ms(2000);
//     ei_printf("--- BNO08x Edge Impulse Moving Window (5s) ---\n");

//     // Initialize I2C
//     i2c_init(I2C_PORT, I2C_BAUDRATE);
//     gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
//     gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
//     gpio_pull_up(I2C_SDA_PIN);
//     gpio_pull_up(I2C_SCL_PIN);

//     // Initialize Sensor
//     if (!bno08x_begin_i2c(&bno08x, I2C_PORT, BNO08X_I2C_ADDR, BNO08X_RESET_PIN)) {
//         ei_printf("ERROR: BNO08x not found\n");
//         while (1) { sleep_ms(10); }
//     }
//     set_reports(&bno08x);

//     // Validate Model Size vs Window Parameter
//     // EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE = (Window_MS / 1000) * Frequency * Axes
//     size_t expected_samples = (MOVING_WINDOW_MS * SAMPLING_FREQUENCY_HZ * EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME) / 1000;
    
//     ei_printf("Model Expects: %d features\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
//     ei_printf("Window of 5s needs: ~%d features\n", expected_samples);

//     // Allow small margin of error due to integer math rounding
//     if (abs((int)expected_samples - (int)EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) > 50) {
//         ei_printf("WARNING: Your Edge Impulse Model Window Size does not match 5 seconds!\n");
//         ei_printf("Please go to Impulse Design and set Window Size to 5000ms.\n");
//         sleep_ms(5000);
//     }

//     // Start Timer
//     static repeating_timer_t timer;
//     add_repeating_timer_us(SENSOR_POLL_INTERVAL_US, sensor_poll_callback, NULL, &timer);

//     ei_printf("Filling buffer (Warm up)...\n");

//     // Variables for moving window
//     int samples_collected = 0;
//     uint64_t last_inference_time = 0;
    
//     // Number of floats per single time-step (AccX, AccY, AccZ, R, P, Y)
//     const int AXIS_COUNT = EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME; 
//     // Index where the LAST sample sits in the array
//     const int LAST_INDEX = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - AXIS_COUNT;

//     while (1) {
//         if (g_sensor_data_updated) {
//             g_sensor_data_updated = false;

//             // 1. Shift the entire buffer to the left to remove the oldest sample
//             // We move everything starting from index 6, back to index 0.
//             // Size to move = Total Size - One Sample Set
//             memmove(features, features + AXIS_COUNT, LAST_INDEX * sizeof(float));

//             // 2. Insert the NEW data at the end of the buffer
//             features[LAST_INDEX + 0] = g_acc.x;
//             features[LAST_INDEX + 1] = g_acc.y;
//             features[LAST_INDEX + 2] = g_acc.z;
//             features[LAST_INDEX + 3] = g_ypr.roll;
//             features[LAST_INDEX + 4] = g_ypr.pitch;
//             features[LAST_INDEX + 5] = g_ypr.yaw;

//             // 3. Handle Startup Warmup
//             // We don't want to classify until the buffer actually has 5s of data
//             if (samples_collected < (EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE / AXIS_COUNT)) {
//                 samples_collected++;
//                 continue; // Skip inference, keep filling
//             }

//             // 4. Check if it is time to run inference
//             // We use INFERENCE_INTERVAL_MS to determine how "often" we ask the model
//             uint64_t now = ei_read_timer_ms();
//             if ((now - last_inference_time) >= INFERENCE_INTERVAL_MS) {
                
//                 // Create the signal object pointing to our moving window buffer
//                 signal_t signal;
//                 signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
//                 signal.get_data = &get_signal_data;

//                 ei_impulse_result_t result = { 0 };
                
//                 // Run classifier
//                 EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);
//                 last_inference_time = now;

//                 if (res != EI_IMPULSE_OK) {
//                     ei_printf("ERR: %d\n", res);
//                 } else {
//                     // Print results
//                     ei_printf("Inference (DSP: %d ms, NN: %d ms)\n", result.timing.dsp, result.timing.classification);
//                     for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
//                          // Simple threshold print
//                         if (result.classification[i].value > 0.7f) {
//                             ei_printf("  >> DETECTED: %s (%f)\n", ei_classifier_inferencing_categories[i], result.classification[i].value);
//                         }
//                     }
//                 }
//             }
//         }
//     }
//     return 0;
// }

// // =================================================================================
// // --- Helper Function Implementations ---
// // =================================================================================
// void set_reports(bno08x_driver_t *driver) {
//     bno08x_enable_report(driver, SH2_ARVR_STABILIZED_RV, SENSOR_REPORT_INTERVAL_US);
//     bno08x_enable_report(driver, SH2_ACCELEROMETER, SENSOR_REPORT_INTERVAL_US);
// }

// void quaternion_to_euler(sh2_RotationVectorWAcc_t* rotational_vector, euler_t* ypr, bool degrees) {
//     float qr = rotational_vector->real; float qi = rotational_vector->i;
//     float qj = rotational_vector->j; float qk = rotational_vector->k;
//     float sqr = qr * qr; float sqi = qi * qi;
//     float sqj = qj * qj; float sqk = qk * qk;
//     ypr->yaw = atan2f(2.0f * (qi * qj + qk * qr), (sqi - sqj - sqk + sqr));
//     ypr->pitch = asinf(-2.0f * (qi * qk - qj * qr) / (sqi + sqj + sqk + sqr));
//     ypr->roll = atan2f(2.0f * (qj * qk + qi * qr), (-sqi - sqj + sqk + sqr));
//     if (degrees) {
//         ypr->yaw *= (180.0f / M_PI); ypr->pitch *= (180.0f / M_PI); ypr->roll *= (180.0f / M_PI);
//     }
// }