/**
 * @file main.c
 * @brief BNO08x example for RP2350B using a non-blocking, interrupt-driven architecture.
 *
 * This application uses a hardware timer to poll the sensor in the background,
 * leaving the main loop free for other tasks. The display is updated only when
 * new data is available.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/timer.h" // Include the hardware timer library

#include "bno08x_driver.h"

// =================================================================================
// --- Tweakable Parameters ---
// =================================================================================

// How often the hardware timer interrupt will fire to poll the sensor.
// 5000us = 5ms = 200 Hz. This is our polling rate.
#define SENSOR_POLL_INTERVAL_US   5000

// How often the BNO08x should generate a new report internally.
// 4000us = 4ms = 250 Hz. Should be slightly faster than or equal to the poll rate.
#define SENSOR_REPORT_INTERVAL_US 4000

// --- Hardware Configuration ---
#define I2C_PORT i2c0
#define I2C_SDA_PIN 24
#define I2C_SCL_PIN 25
#define I2C_BAUDRATE 400 * 1000 // 400 kHz

#define BNO08X_RESET_PIN -1
#define BNO08X_I2C_ADDR BNO08x_I2CADDR_DEFAULT

// =================================================================================
// --- Global Variables for Interrupt and Main Loop Communication ---
// =================================================================================

// Main driver instance
static bno08x_driver_t bno08x;

// Struct to hold calculated Euler angles
typedef struct { float yaw, pitch, roll; } euler_t;

// Use 'volatile' for any variable shared between the main loop and an interrupt.
// This tells the compiler that the value can change at any time.
static volatile euler_t g_ypr = {0};
static volatile sh2_Accelerometer_t g_acc = {0};
static volatile uint16_t g_steps = 0;
static volatile uint8_t g_stability = 0;
static volatile uint8_t g_accuracy = 0;

// This flag is set by the interrupt to signal that new data is ready.
static volatile bool g_sensor_data_updated = false;

// =================================================================================
// --- Timer Interrupt Service Routine (ISR) ---
// =================================================================================

/**
 * @brief This function is called automatically by the hardware timer.
 *        It polls the sensor for new data.
 * @note  NEVER put slow code like printf() inside an ISR.
 */
bool sensor_poll_callback(repeating_timer_t *t) {
    // This struct is only used inside the ISR, so it can be static.
    static sh2_SensorValue_t sensor_value;

    // Poll the driver. If it returns true, a new event was received.
    if (bno08x_get_sensor_event(&bno08x, &sensor_value)) {
        // A new event is available, update our volatile global variables
        switch (sensor_value.sensorId) {
            case SH2_ARVR_STABILIZED_RV:
                quaternion_to_euler(&sensor_value.un.arvrStabilizedRV, (euler_t*)&g_ypr, true);
                g_accuracy = sensor_value.status;
                break;
            case SH2_ACCELEROMETER:
                g_acc = sensor_value.un.accelerometer;
                break;
            case SH2_STEP_COUNTER:
                g_steps = sensor_value.un.stepCounter.steps;
                break;
            case SH2_STABILITY_CLASSIFIER:
                g_stability = sensor_value.un.stabilityClassifier.classification;
                break;
            default:
                break;
        }
        // Set the flag to let the main loop know it can update the display
        g_sensor_data_updated = true;
    }

    return true; // Keep the timer repeating
}

// =================================================================================
// --- Main Application ---
// =================================================================================

// Forward declarations for helper functions
void print_product_ids(bno08x_driver_t *driver);
void set_reports(bno08x_driver_t *driver);
const char* get_stability_string(uint8_t classification);
void quaternion_to_euler(sh2_RotationVectorWAcc_t* rotational_vector, euler_t* ypr, bool degrees);


int main() {
    stdio_init_all();
    sleep_ms(2000);
    printf("--- BNO08x Interrupt-Driven Demo for RP2350B ---\n");

    // --- I2C and Sensor Initialization (same as before) ---
    i2c_init(I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    printf("Initializing BNO08x sensor...\n");
    if (!bno08x_begin_i2c(&bno08x, I2C_PORT, BNO08X_I2C_ADDR, BNO08X_RESET_PIN)) {
        printf("ERROR: Failed to find BNO08x chip. Check wiring.\n");
        while (1) { sleep_ms(10); }
    }
    printf("BNO08x found!\n");

    print_product_ids(&bno08x);
    set_reports(&bno08x);

    // --- SETUP THE REPEATING TIMER INTERRUPT ---
    printf("Starting sensor polling timer (%ld us interval)...\n", SENSOR_POLL_INTERVAL_US);
    static repeating_timer_t timer;
    add_repeating_timer_us(SENSOR_POLL_INTERVAL_US, sensor_poll_callback, NULL, &timer);

    printf("Reading events...\n");
    sleep_ms(100);
    printf("\033[2J\033[H"); // Clear screen and move to home

    // --- MAIN NON-BLOCKING LOOP ---
    while (1) {
        // Check if the interrupt has told us new data is ready
        if (g_sensor_data_updated) {
            // Immediately clear the flag
            g_sensor_data_updated = false;

            // --- In-Place Printing ---
            printf("\033[H"); // Move cursor to home
            printf("--- BNO08x Sensor Data (Interrupt Driven) ---\n\033[K");
            printf("Yaw: %-8.2f Pitch: %-8.2f Roll: %-8.2f (Accuracy: %d/3)\n\033[K", g_ypr.yaw, g_ypr.pitch, g_ypr.roll, g_accuracy);
            printf("Accel X: %-8.2f Y: %-8.2f Z: %-8.2f m/s^2\n\033[K", g_acc.x, g_acc.y, g_acc.z);
            printf("Steps: %-5u\n\033[K", g_steps);
            printf("Stability: %-15s\n\033[K", get_stability_string(g_stability));
            printf("-------------------------------------------\n\033[K");
        }

        // --- THIS IS WHERE YOUR OTHER CODE GOES ---
        // The main loop is now free to do other things without being blocked
        // by sensor polling. For example:
        // check_buttons();
        // update_display();
        // manage_wifi();
        //
        // Since this loop runs very fast, a small sleep can be good practice
        // to prevent it from consuming 100% CPU if there's nothing else to do.
        sleep_ms(10); 
    }

    return 0;
}

// =================================================================================
// --- Helper Function Implementations ---
// =================================================================================

void set_reports(bno08x_driver_t *driver) {
    printf("Setting desired reports (%.0f Hz)...\n", 1.0e6 / SENSOR_REPORT_INTERVAL_US);
    
    if (!bno08x_enable_report(driver, SH2_ARVR_STABILIZED_RV, SENSOR_REPORT_INTERVAL_US)) {
        printf("Could not enable AR/VR Stabilized Rotation Vector\n");
    }
    if (!bno08x_enable_report(driver, SH2_ACCELEROMETER, SENSOR_REPORT_INTERVAL_US)) {
        printf("Could not enable accelerometer\n");
    }
    if (!bno08x_enable_report(driver, SH2_STEP_COUNTER, 1000000)) {
        printf("Could not enable step counter\n");
    }
    if (!bno08x_enable_report(driver, SH2_STABILITY_CLASSIFIER, 1000000)) {
        printf("Could not enable stability classifier\n");
    }
}

// (The other helper functions are unchanged)
void print_product_ids(bno08x_driver_t *driver) {
    printf("Product IDs:\n");
    for (int n = 0; n < driver->prod_ids.numEntries; n++) {
        printf("  Part %lu: Version %d.%d.%d Build %lu\n",
               driver->prod_ids.entry[n].swPartNumber, driver->prod_ids.entry[n].swVersionMajor,
               driver->prod_ids.entry[n].swVersionMinor, driver->prod_ids.entry[n].swVersionPatch,
               driver->prod_ids.entry[n].swBuildNumber);
    }
}

const char* get_stability_string(uint8_t classification) {
    switch (classification) {
        case STABILITY_CLASSIFIER_UNKNOWN:    return "Unknown";
        case STABILITY_CLASSIFIER_ON_TABLE:   return "On Table";
        case STABILITY_CLASSIFIER_STATIONARY: return "Stationary";
        case STABILITY_CLASSIFIER_STABLE:     return "Stable";
        case STABILITY_CLASSIFIER_MOTION:     return "In Motion";
        default:                              return "Invalid";
    }
}

void quaternion_to_euler(sh2_RotationVectorWAcc_t* rotational_vector, euler_t* ypr, bool degrees) {
    float qr = rotational_vector->real; float qi = rotational_vector->i;
    float qj = rotational_vector->j; float qk = rotational_vector->k;
    float sqr = qr * qr; float sqi = qi * qi;
    float sqj = qj * qj; float sqk = qk * qk;
    ypr->yaw = atan2f(2.0f * (qi * qj + qk * qr), (sqi - sqj - sqk + sqr));
    ypr->pitch = asinf(-2.0f * (qi * qk - qj * qr) / (sqi + sqj + sqk + sqr));
    ypr->roll = atan2f(2.0f * (qj * qk + qi * qr), (-sqi - sqj + sqk + sqr));
    if (degrees) {
        ypr->yaw *= (180.0f / M_PI); ypr->pitch *= (180.0f / M_PI); ypr->roll *= (180.0f / M_PI);
    }
}