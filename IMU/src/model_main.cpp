/**
 * @file main.cpp
 * @brief BNO08x gesture recognition with Edge Impulse on RP2350B using Pico SDK.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h> // For malloc/free
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"

// C vs C++ linkage for the C driver
extern "C" {
    #include "bno08x_driver.h"
}

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

// =================================================================================
// --- Tweakable Parameters ---
// =================================================================================
#define SAMPLING_FREQUENCY_HZ     74
#define SENSOR_POLL_INTERVAL_US   (1000000 / SAMPLING_FREQUENCY_HZ)
#define SENSOR_REPORT_INTERVAL_US (1000000 / SAMPLING_FREQUENCY_HZ)

#define I2C_PORT i2c0
#define I2C_SDA_PIN 16
#define I2C_SCL_PIN 17
#define I2C_BAUDRATE 400 * 1000
#define BNO08X_RESET_PIN -1
#define BNO08X_I2C_ADDR BNO08x_I2CADDR_DEFAULT

// =================================================================================
// --- Global Variables & Prototypes ---
// =================================================================================
static bno08x_driver_t bno08x;
typedef struct { float yaw, pitch, roll; } euler_t;

static volatile euler_t g_ypr = {0};
static volatile sh2_Accelerometer_t g_acc = {0};
static volatile bool g_sensor_data_updated = false;

static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE];
static int feature_ix = 0;

void quaternion_to_euler(sh2_RotationVectorWAcc_t* rotational_vector, euler_t* ypr, bool degrees);
void set_reports(bno08x_driver_t *driver);

// =================================================================================
// --- Edge Impulse Porting Layer (FIXED SIGNATURES) ---
// =================================================================================

// The `__attribute__((weak))` is not strictly necessary but good practice
// if you were to ever link another file with these definitions.

// FIXED: Return type is void, matches the header
void ei_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

// FIXED: Return type is void, matches the header
void ei_printf_float(float f) {
    printf("%f", f);
}

// These are correct as-is
void *ei_malloc(size_t size) { return malloc(size); }
void *ei_calloc(size_t nitems, size_t size) { return calloc(nitems, size); }
void ei_free(void *ptr) { free(ptr); }

uint64_t ei_read_timer_ms() { return to_ms_since_boot(get_absolute_time()); }
uint64_t ei_read_timer_us() { return to_us_since_boot(get_absolute_time()); }

// FIXED: Return type is EI_IMPULSE_ERROR, matches the header
EI_IMPULSE_ERROR ei_sleep(int32_t time_ms) {
    sleep_ms(time_ms);
    return EI_IMPULSE_OK;
}

// FIXED: Return type is EI_IMPULSE_ERROR, matches the header
EI_IMPULSE_ERROR ei_run_impulse_check_canceled() {
    return EI_IMPULSE_OK; // Return OK to indicate not cancelled
}

// =================================================================================
// --- Edge Impulse Data Callback ---
// =================================================================================
static int get_signal_data(size_t offset, size_t length, float *out_ptr) {
    for (size_t i = 0; i < length; i++) {
        out_ptr[i] = features[offset + i];
    }
    return EIDSP_OK;
}

// =================================================================================
// --- Interrupt Service Routine ---
// =================================================================================
bool sensor_poll_callback(repeating_timer_t *t) {
    static sh2_SensorValue_t sensor_value;
    if (bno08x_get_sensor_event(&bno08x, &sensor_value)) {
        switch (sensor_value.sensorId) {
            case SH2_ARVR_STABILIZED_RV:
                quaternion_to_euler(&sensor_value.un.arvrStabilizedRV, (euler_t*)&g_ypr, false);
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

// =================================================================================
// --- Main ---
// =================================================================================
int main() {
    stdio_init_all();
    sleep_ms(2000);
    ei_printf("--- BNO08x Edge Impulse Gesture Recognizer ---\n");

    i2c_init(I2C_PORT, I2C_BAUDRATE);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    if (!bno08x_begin_i2c(&bno08x, I2C_PORT, BNO08X_I2C_ADDR, BNO08X_RESET_PIN)) {
        ei_printf("ERROR: BNO08x not found\n");
        while (1) { sleep_ms(10); }
    }
    ei_printf("BNO08x found!\n");
    set_reports(&bno08x);

    static repeating_timer_t timer;
    add_repeating_timer_us(SENSOR_POLL_INTERVAL_US, sensor_poll_callback, NULL, &timer);

    ei_printf("Starting sampling...\n");

    while (1) {
        if (g_sensor_data_updated) {
            g_sensor_data_updated = false;

            features[feature_ix++] = g_acc.x;
            features[feature_ix++] = g_acc.y;
            features[feature_ix++] = g_acc.z;
            features[feature_ix++] = g_ypr.roll;
            features[feature_ix++] = g_ypr.pitch;
            features[feature_ix++] = g_ypr.yaw;

            if (feature_ix >= EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE) {
                signal_t signal;
                signal.total_length = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;
                signal.get_data = &get_signal_data;

                ei_impulse_result_t result = { 0 };
                EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false);

                if (res != EI_IMPULSE_OK) {
                    ei_printf("ERR: Failed to run classifier (%d)\n", res);
                } else {
                    ei_printf("Predictions (DSP: %d ms, Classification: %d ms)\n", result.timing.dsp, result.timing.classification);
                    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
                        ei_printf("  %s: ", ei_classifier_inferencing_categories[i]);
                        ei_printf_float(result.classification[i].value);
                        ei_printf("\n");
                    }
                }
                feature_ix = 0;
            }
        }
    }
    return 0;
}

// =================================================================================
// --- Helper Function Implementations ---
// =================================================================================

void set_reports(bno08x_driver_t *driver) {
    bno08x_enable_report(driver, SH2_ARVR_STABILIZED_RV, SENSOR_REPORT_INTERVAL_US);
    bno08x_enable_report(driver, SH2_ACCELEROMETER, SENSOR_REPORT_INTERVAL_US);
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