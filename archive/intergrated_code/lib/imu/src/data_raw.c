// /**
//  * @file main.c
//  * @brief BNO08x example for RP2350B, adapted for Edge Impulse data collection.
//  *
//  * This version uses the original, working interrupt-driven architecture and
//  * prints the existing accelerometer and Euler angle data as a 6-axis stream.
//  */

// #include <stdio.h>
// #include <string.h>
// #include <math.h>
// #include "pico/stdlib.h"
// #include "hardware/i2c.h"
// #include "hardware/timer.h" // Include the hardware timer library

// #include "bno08x_driver.h"

// // =================================================================================
// // --- Tweakable Parameters ---
// // =================================================================================

// // MODIFIED: Set to a consistent 100Hz for machine learning.
// #define SENSOR_POLL_INTERVAL_US   10000 // 10ms = 100 Hz
// #define SENSOR_REPORT_INTERVAL_US 10000 // 10ms = 100 Hz

// // --- Hardware Configuration ---
// #define I2C_PORT i2c0
// #define I2C_SDA_PIN 16
// #define I2C_SCL_PIN 17
// #define I2C_BAUDRATE 400 * 1000 // 400 kHz

// #define BNO08X_RESET_PIN -1
// #define BNO08X_I2C_ADDR BNO08x_I2CADDR_DEFAULT

// // =================================================================================
// // --- Global Variables for Interrupt and Main Loop Communication ---
// // =================================================================================

// // Main driver instance
// static bno08x_driver_t bno08x;

// // Struct to hold calculated Euler angles
// typedef struct { float yaw, pitch, roll; } euler_t;

// // YOUR ORIGINAL GLOBAL VARIABLES - UNCHANGED
// static volatile euler_t g_ypr = {0};
// static volatile sh2_Accelerometer_t g_acc = {0};
// static volatile uint16_t g_steps = 0;
// static volatile uint8_t g_stability = 0;
// static volatile uint8_t g_accuracy = 0;
// static volatile bool g_sensor_data_updated = false;

// // =================================================================================
// // --- Timer Interrupt Service Routine (ISR) ---
// // =================================================================================

// // YOUR ORIGINAL ISR - UNCHANGED
// bool sensor_poll_callback(repeating_timer_t *t) {
//     static sh2_SensorValue_t sensor_value;
//     if (bno08x_get_sensor_event(&bno08x, &sensor_value)) {
//         switch (sensor_value.sensorId) {
//             case SH2_ARVR_STABILIZED_RV:
//                 quaternion_to_euler(&sensor_value.un.arvrStabilizedRV, (euler_t*)&g_ypr, true);
//                 g_accuracy = sensor_value.status;
//                 break;
//             case SH2_ACCELEROMETER:
//                 g_acc = sensor_value.un.accelerometer;
//                 break;
//             case SH2_STEP_COUNTER:
//                 g_steps = sensor_value.un.stepCounter.steps;
//                 break;
//             case SH2_STABILITY_CLASSIFIER:
//                 g_stability = sensor_value.un.stabilityClassifier.classification;
//                 break;
//             default:
//                 break;
//         }
//         g_sensor_data_updated = true;
//     }
//     return true; // Keep the timer repeating
// }

// // =================================================================================
// // --- Main Application ---
// // =================================================================================

// // YOUR ORIGINAL HELPER FUNCTIONS - UNCHANGED
// void print_product_ids(bno08x_driver_t *driver);
// void set_reports(bno08x_driver_t *driver);
// const char* get_stability_string(uint8_t classification);
// void quaternion_to_euler(sh2_RotationVectorWAcc_t* rotational_vector, euler_t* ypr, bool degrees);


// int main() {
//     stdio_init_all();
//     sleep_ms(2000);
//     printf("--- BNO08x Edge Impulse Data Forwarder ---\n");

//     // --- I2C and Sensor Initialization (YOURS - UNCHANGED) ---
//     i2c_init(I2C_PORT, I2C_BAUDRATE);
//     gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
//     gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
//     gpio_pull_up(I2C_SDA_PIN);
//     gpio_pull_up(I2C_SCL_PIN);

//     printf("Initializing BNO08x sensor...\n");
//     if (!bno08x_begin_i2c(&bno08x, I2C_PORT, BNO08X_I2C_ADDR, BNO08X_RESET_PIN)) {
//         printf("ERROR: Failed to find BNO08x chip. Check wiring.\n");
//         while (1) { sleep_ms(10); }
//     }
//     printf("BNO08x found!\n");

//     print_product_ids(&bno08x);
//     set_reports(&bno08x);

//     // --- SETUP THE REPEATING TIMER INTERRUPT (YOURS - UNCHANGED) ---
//     printf("Starting sensor polling timer (%d us interval)...\n", SENSOR_POLL_INTERVAL_US);
//     static repeating_timer_t timer;
//     add_repeating_timer_us(SENSOR_POLL_INTERVAL_US, sensor_poll_callback, NULL, &timer);

//     printf("Starting data stream...\n");
//     sleep_ms(100);

//     // --- MAIN NON-BLOCKING LOOP ---
//     // THIS IS THE ONLY SECTION THAT HAS BEEN CHANGED
//     while (1) {
//         // Check if the interrupt has told us new data is ready
//         if (g_sensor_data_updated) {
//             // Immediately clear the flag
//             g_sensor_data_updated = false;

//             // Print the 6-axis data stream required by Edge Impulse.
//             // We are using the accelerometer and Euler angles you already have.
//             // This is the ONLY thing printed in the loop.
//             printf("%f,%f,%f,%f,%f,%f\n",
//                    g_acc.x, g_acc.y, g_acc.z,
//                    g_ypr.roll, g_ypr.pitch, g_ypr.yaw);
//         }
        
//         // A small sleep to prevent the loop from using 100% CPU
//         sleep_ms(10); 
//     }

//     return 0;
// }

// // =================================================================================
// // --- Helper Function Implementations ---
// // =================================================================================

// // YOUR ORIGINAL set_reports FUNCTION - UNCHANGED
// void set_reports(bno08x_driver_t *driver) {
//     printf("Setting desired reports (%.0f Hz)...\n", 1.0e6 / SENSOR_REPORT_INTERVAL_US);
    
//     if (!bno08x_enable_report(driver, SH2_ARVR_STABILIZED_RV, SENSOR_REPORT_INTERVAL_US)) {
//         printf("Could not enable AR/VR Stabilized Rotation Vector\n");
//     }
//     if (!bno08x_enable_report(driver, SH2_ACCELEROMETER, SENSOR_REPORT_INTERVAL_US)) {
//         printf("Could not enable accelerometer\n");
//     }
//     // These reports aren't used for the data stream, but leaving them enabled is harmless.
//     if (!bno08x_enable_report(driver, SH2_STEP_COUNTER, 1000000)) {
//         printf("Could not enable step counter\n");
//     }
//     if (!bno08x_enable_report(driver, SH2_STABILITY_CLASSIFIER, 1000000)) {
//         printf("Could not enable stability classifier\n");
//     }
// }

// // YOUR ORIGINAL HELPER FUNCTIONS - UNCHANGED
// void print_product_ids(bno08x_driver_t *driver) {
//     printf("Product IDs:\n");
//     for (int n = 0; n < driver->prod_ids.numEntries; n++) {
//         printf("  Part %lu: Version %d.%d.%d Build %lu\n",
//                driver->prod_ids.entry[n].swPartNumber, driver->prod_ids.entry[n].swVersionMajor,
//                driver->prod_ids.entry[n].swVersionMinor, driver->prod_ids.entry[n].swVersionPatch,
//                driver->prod_ids.entry[n].swBuildNumber);
//     }
// }

// const char* get_stability_string(uint8_t classification) {
//     switch (classification) {
//         case STABILITY_CLASSIFIER_UNKNOWN:    return "Unknown";
//         case STABILITY_CLASSIFIER_ON_TABLE:   return "On Table";
//         case STABILITY_CLASSIFIER_STATIONARY: return "Stationary";
//         case STABILITY_CLASSIFIER_STABLE:     return "Stable";
//         case STABILITY_CLASSIFIER_MOTION:     return "In Motion";
//         default:                              return "Invalid";
//     }
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