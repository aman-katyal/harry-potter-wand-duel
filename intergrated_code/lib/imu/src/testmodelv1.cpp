// /**
//  * @file model_main.cpp
//  * @brief BNO08x spell classifier using Edge Impulse, on RP2350 (Pico 2).
//  *
//  * Impulse settings (must match Edge Impulse project):
//  *   - Input axes (6): x, y, z, roll, pitch, yaw
//  *   - Window size: 2000 ms
//  *   - Window increase (stride): 250 ms
//  *   - Frequency: 74 Hz
//  *   - Output classes: 3 (aguamenti, idle, stupefy)
//  *
//  * This code tries to behave as close as possible to the
//  * **Live Classification** view in Edge Impulse Studio.
//  */

// #include <stdio.h>
// #include <string.h>
// #include <math.h>
// #include <stdlib.h>
// #include <stdarg.h>         // for va_list (ei_printf)
// #include "pico/stdlib.h"
// #include "hardware/i2c.h"
// #include "hardware/timer.h"

// extern "C" {
//     #include "bno08x_driver.h"   // Your BNO08x C driver
// }

// // Edge Impulse inference API
// #include "edge-impulse-sdk/classifier/ei_run_classifier.h"

// // ---------------------------------------------------------------------------
// // USER CONFIG SECTION
// // ---------------------------------------------------------------------------

// // IMU sample rate (must match Edge Impulse frequency)
// #define SAMPLING_FREQUENCY_HZ       74

// // How often to poll BNO08x (us) – here we match the sample rate
// #define SENSOR_POLL_INTERVAL_US     (1000000 / SAMPLING_FREQUENCY_HZ)

// // How often BNO08x should generate a new report (us)
// #define SENSOR_REPORT_INTERVAL_US   (1000000 / SAMPLING_FREQUENCY_HZ)

// // Edge Impulse window settings (must match Impulse settings)
// #define MOVING_WINDOW_MS            2000    // 2 seconds
// #define INFERENCE_INTERVAL_MS       250     // 0.25 seconds (stride)

// // I2C + BNO08x configuration
// #define I2C_PORT                    i2c0
// #define I2C_SDA_PIN                 16
// #define I2C_SCL_PIN                 17
// #define I2C_BAUDRATE                (400 * 1000)
// #define BNO08X_RESET_PIN            -1
// #define BNO08X_I2C_ADDR             BNO08x_I2CADDR_DEFAULT

// // Did you train your model with roll/pitch/yaw in DEGREES or RADIANS?
// // - If your data forwarder printed euler angles from Adafruit's examples,
// //   it was almost certainly DEGREES → keep this as 1.
// // - Change to 0 only if you are sure you trained on radians.
// #define EI_YPR_IN_DEGREES           1

// // IMPULSE MUST HAVE 6 RAW AXES (x, y, z, roll, pitch, yaw)
// #if EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME != 6
// #error "Impulse must be configured for 6 input axes: x, y, z, roll, pitch, yaw"
// #endif

// // ---------------------------------------------------------------------------
// // Types & globals
// // ---------------------------------------------------------------------------

// // Simple struct for Euler angles
// typedef struct {
//     float yaw;
//     float pitch;
//     float roll;
// } euler_t;

// // BNO08x driver object
// static bno08x_driver_t bno08x;

// // Global latest sensor readings (updated in timer ISR)
// static volatile euler_t             g_ypr = {0};
// static volatile sh2_Accelerometer_t g_acc = {0};
// static volatile bool                g_sensor_data_updated = false;

// // Edge Impulse feature buffer (holds one full window)
// static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];

// // Forward declarations
// void quaternion_to_euler(sh2_RotationVectorWAcc_t* rv,
//                          euler_t* ypr,
//                          bool degrees);
// void set_reports(bno08x_driver_t *driver);

// // ---------------------------------------------------------------------------
// // Edge Impulse "porting layer" helpers
// // ---------------------------------------------------------------------------

// void ei_printf(const char *format, ...) {
//     va_list args;
//     va_start(args, format);
//     vprintf(format, args);
//     va_end(args);
// }

// void ei_printf_float(float f)               { printf("%f", f); }
// void *ei_malloc(size_t size)                { return malloc(size); }
// void *ei_calloc(size_t nitems, size_t size) { return calloc(nitems, size); }
// void ei_free(void *ptr)                     { free(ptr); }

// uint64_t ei_read_timer_ms()                 { return to_ms_since_boot(get_absolute_time()); }
// uint64_t ei_read_timer_us()                 { return to_us_since_boot(get_absolute_time()); }

// EI_IMPULSE_ERROR ei_sleep(int32_t time_ms) {
//     sleep_ms((uint32_t)time_ms);
//     return EI_IMPULSE_OK;
// }

// // We never cancel in this example
// EI_IMPULSE_ERROR ei_run_impulse_check_canceled() {
//     return EI_IMPULSE_OK;
// }

// // Edge Impulse will call this to read from our feature buffer
// static int get_signal_data(size_t offset, size_t length, float *out_ptr) {
//     memcpy(out_ptr, features + offset, length * sizeof(float));
//     return EIDSP_OK;
// }

// // ---------------------------------------------------------------------------
// // Timer ISR: poll BNO08x at fixed rate and update globals
// // ---------------------------------------------------------------------------

// bool sensor_poll_callback(repeating_timer_t *t) {
//     (void)t;
//     static sh2_SensorValue_t sensor_value;

//     // Non-blocking: returns true if there was an event
//     if (bno08x_get_sensor_event(&bno08x, &sensor_value)) {
//         switch (sensor_value.sensorId) {
//             case SH2_ARVR_STABILIZED_RV:
//                 // Convert quaternion to Euler angles (YPR)
//                 quaternion_to_euler(
//                     &sensor_value.un.arvrStabilizedRV,
//                     (euler_t*)&g_ypr,
//                     EI_YPR_IN_DEGREES ? true : false
//                 );
//                 break;

//             case SH2_ACCELEROMETER:
//                 // Copy the latest acceleration vector
//                 memcpy((void*)&g_acc,
//                        &sensor_value.un.accelerometer,
//                        sizeof(sh2_Accelerometer_t));
//                 break;

//             default:
//                 // Ignore any other events
//                 break;
//         }

//         // Tell main loop there is new data to consume
//         g_sensor_data_updated = true;
//     }

//     // Keep timer running
//     return true;
// }

// // ---------------------------------------------------------------------------
// // main()
// // ---------------------------------------------------------------------------

// int main() {
//     // Initialize USB serial
//     stdio_init_all();
//     sleep_ms(2000);   // allow time for terminal to connect

//     ei_printf("\n=============================================\n");
//     ei_printf("  BNO08x Spell Classifier (Edge Impulse)\n");
//     ei_printf("  Axes: x,y,z, roll,pitch,yaw (6)\n");
//     ei_printf("  Window: %d ms, Stride: %d ms, Freq: %d Hz\n",
//               MOVING_WINDOW_MS, INFERENCE_INTERVAL_MS, SAMPLING_FREQUENCY_HZ);
//     ei_printf("=============================================\n");

//     // -----------------------------------------------------------------------
//     // 1. Init I2C
//     // -----------------------------------------------------------------------
//     i2c_init(I2C_PORT, I2C_BAUDRATE);
//     gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
//     gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
//     gpio_pull_up(I2C_SDA_PIN);
//     gpio_pull_up(I2C_SCL_PIN);

//     // -----------------------------------------------------------------------
//     // 2. Init BNO08x
//     // -----------------------------------------------------------------------
//     if (!bno08x_begin_i2c(&bno08x, I2C_PORT, BNO08X_I2C_ADDR, BNO08X_RESET_PIN)) {
//         ei_printf("ERROR: BNO08x not found on I2C bus!\n");
//         while (1) { sleep_ms(1000); }
//     }

//     // Enable accelerometer + stabilized rotation vector
//     set_reports(&bno08x);

//     // -----------------------------------------------------------------------
//     // 3. Sanity-check: is window config close to model?
//     // -----------------------------------------------------------------------
//     size_t expected_features =
//         (MOVING_WINDOW_MS * SAMPLING_FREQUENCY_HZ *
//          EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME) / 1000;

//     ei_printf("Model DSP input size: %d\n", EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE);
//     ei_printf("Expected from window:  %d (approx)\n", (int)expected_features);

//     if (abs((int)expected_features - (int)EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) > 75) {
//         ei_printf("WARNING: window size / frequency may not match your impulse.\n");
//         ei_printf("Check Impulse: 74 Hz, 2000 ms, 6 axes.\n\n");
//     }

//     // -----------------------------------------------------------------------
//     // 4. Start timer for sensor polling
//     // -----------------------------------------------------------------------
//     static repeating_timer_t timer;
//     add_repeating_timer_us(SENSOR_POLL_INTERVAL_US,
//                            sensor_poll_callback,
//                            NULL,
//                            &timer);

//     ei_printf("Sensor polling timer started.\n");
//     ei_printf("Filling first 2-second buffer (warm-up)...\n");

//     // -----------------------------------------------------------------------
//     // 5. Moving-window state variables
//     // -----------------------------------------------------------------------
//     memset(features, 0, sizeof(features));       // start with zeros

//     // Number of floats per time step (raw samples per frame) = 6
//     const int AXIS_COUNT = EI_CLASSIFIER_RAW_SAMPLES_PER_FRAME;

//     // Where the newest sample gets written in the feature buffer
//     const int LAST_INDEX = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE - AXIS_COUNT;

//     // How many time steps fit in a full 2s window
//     const int STEPS_PER_WINDOW = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE / AXIS_COUNT;

//     int      samples_collected   = 0;            // warm-up counter
//     uint64_t last_inference_time = 0;            // when we last ran the model

//     // -----------------------------------------------------------------------
//     // 6. Main loop
//     // -----------------------------------------------------------------------
//     while (true) {

//         // Wait until the timer ISR tells us there's fresh sensor data
//         if (!g_sensor_data_updated) {
//             tight_loop_contents();
//             continue;
//         }
//         g_sensor_data_updated = false;

//         // ------------------ 6.1 Slide window ------------------------------
//         // Drop the oldest time step and shift everything left by 6 floats.
//         memmove(features,
//                 features + AXIS_COUNT,
//                 LAST_INDEX * sizeof(float));

//         // ------------------ 6.2 Insert newest sample ----------------------
//         // IMPORTANT: order must match the order sent to Edge Impulse
//         // when you recorded data: x,y,z,roll,pitch,yaw.
//         features[LAST_INDEX + 0] = g_acc.x;
//         features[LAST_INDEX + 1] = g_acc.y;
//         features[LAST_INDEX + 2] = g_acc.z;
//         features[LAST_INDEX + 3] = g_ypr.roll;
//         features[LAST_INDEX + 4] = g_ypr.pitch;
//         features[LAST_INDEX + 5] = g_ypr.yaw;

//         // ------------------ 6.3 Warm-up phase -----------------------------
//         // First, we need to fill an entire 2-second window before
//         // calling the classifier. After STEPS_PER_WINDOW samples,
//         // the buffer holds the last 2 seconds of data.
//         if (samples_collected < STEPS_PER_WINDOW) {
//             samples_collected++;
//             continue;
//         }

//         // ------------------ 6.4 Run inference every 250 ms ----------------
//         uint64_t now = ei_read_timer_ms();
//         if ((now - last_inference_time) < INFERENCE_INTERVAL_MS) {
//             continue;   // not time yet
//         }
//         last_inference_time = now;

//         // Wrap our feature buffer in a signal_t for Edge Impulse
//         signal_t signal;
//         signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
//         signal.get_data     = &get_signal_data;

//         ei_impulse_result_t result = { 0 };

//         EI_IMPULSE_ERROR r = run_classifier(&signal, &result, false);
//         if (r != EI_IMPULSE_OK) {
//             ei_printf("ERR: run_classifier returned %d\n", r);
//             continue;
//         }

//         // ------------------ 6.5 Print results -----------------------------
//         ei_printf("\nInference (DSP: %d ms, NN: %d ms, Anom: %d ms)\n",
//                   result.timing.dsp,
//                   result.timing.classification,
//                   result.timing.anomaly);

//         float    best_val = 0.0f;
//         uint16_t best_idx = 0;

//         for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
//             float       v     = result.classification[i].value;
//             const char *label = ei_classifier_inferencing_categories[i];

//             ei_printf("  %s: %.3f\n", label, v);

//             if (v > best_val) {
//                 best_val = v;
//                 best_idx = i;
//             }
//         }

//         // Optional simple threshold (tweak as needed)
//         const float DETECT_THRESHOLD = 0.7f;

//         if (best_val > DETECT_THRESHOLD) {
//             ei_printf("  >> DETECTED: %s (%.2f)\n",
//                       ei_classifier_inferencing_categories[best_idx],
//                       best_val);
//         }
//     }

//     return 0; // never reached
// }

// // ---------------------------------------------------------------------------
// // BNO08x helper functions
// // ---------------------------------------------------------------------------

// // Enable the reports we want from the BNO08x
// void set_reports(bno08x_driver_t *driver) {
//     // Orientation (quaternion) → AR/VR stabilized rotation vector
//     bno08x_enable_report(driver,
//                          SH2_ARVR_STABILIZED_RV,
//                          SENSOR_REPORT_INTERVAL_US);

//     // Accelerometer
//     bno08x_enable_report(driver,
//                          SH2_ACCELEROMETER,
//                          SENSOR_REPORT_INTERVAL_US);
// }

// // Convert quaternion to yaw/pitch/roll
// void quaternion_to_euler(sh2_RotationVectorWAcc_t* rv,
//                          euler_t* ypr,
//                          bool degrees) {
//     float qr = rv->real;
//     float qi = rv->i;
//     float qj = rv->j;
//     float qk = rv->k;

//     float sqr = qr * qr;
//     float sqi = qi * qi;
//     float sqj = qj * qj;
//     float sqk = qk * qk;

//     // Yaw (Z axis)
//     ypr->yaw = atan2f(
//         2.0f * (qi * qj + qk * qr),
//         (sqi - sqj - sqk + sqr)
//     );

//     // Pitch (Y axis)
//     ypr->pitch = asinf(
//         -2.0f * (qi * qk - qj * qr) /
//         (sqi + sqj + sqk + sqr)
//     );

//     // Roll (X axis)
//     ypr->roll = atan2f(
//         2.0f * (qj * qk + qi * qr),
//         (-sqi - sqj + sqk + sqr)
//     );

//     if (degrees) {
//         const float rad2deg = 180.0f / (float)M_PI;
//         ypr->yaw   *= rad2deg;
//         ypr->pitch *= rad2deg;
//         ypr->roll  *= rad2deg;
//     }
// }
