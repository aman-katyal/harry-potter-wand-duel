/**
 * @file main.c
 * @brief Dual-Core Wand Duel - Tunable Master Version (Split I2C)
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "hardware/i2c.h"

// --- Drivers ---
#include "bno08x_driver.h"
#include "ei_bridge.h" 
#include "ir_emitter.h"
#include "irremote.h"
#include "drv2605.h"
#include "ws2812.h"
#include "controller.h"
#include "healthbar.h"

// ===========================================================================
// --- 1. THE "FEEL" TUNING SECTION ---
// ===========================================================================

#define AI_CONFIDENCE_THRESHOLD   0.70f  
#define AI_ANOMALY_THRESHOLD      2.5f   
#define CONSECUTIVE_MATCHES_REQ   2      
#define AI_CHECK_INTERVAL_MS      200    
#define POST_CAST_LOCKOUT_MS      2000   
#define GAME_COOLDOWN_MS          2000   

// ===========================================================================
// --- 2. SYSTEM CONSTANTS & PINS ---
// ===========================================================================
#define SPELL_ID_NONE             0
#define SPELL_ID_AGUAMENTI        1
#define SPELL_ID_STUPEFY          2

#define SAMPLING_FREQ_HZ          74
#define SENSOR_POLL_US            (1000000 / SAMPLING_FREQ_HZ)

// Window Calculation (1.65s)
#define WINDOW_MS                 1650
#define SAMPLES_PER_WINDOW        ((WINDOW_MS * SAMPLING_FREQ_HZ) / 1000)
#define AXES_COUNT                6 
#define EI_FEATURE_SIZE           (SAMPLES_PER_WINDOW * AXES_COUNT)
#define RING_BUFFER_SIZE          (3 * SAMPLING_FREQ_HZ * AXES_COUNT)

// --- PINOUT CONFIGURATION ---

// I2C Bus 0: IMU (BNO08x)
#define I2C_IMU_PORT              i2c0
#define I2C_IMU_SDA_PIN           4
#define I2C_IMU_SCL_PIN           5

// I2C Bus 1: Haptics (DRV2605)
#define I2C_HAPTIC_PORT           i2c1
#define I2C_HAPTIC_SDA_PIN        2
#define I2C_HAPTIC_SCL_PIN        3

#define TX_PIN                    6
#define RX_PIN                    16
#define BTN_A_PIN                 21
#define BTN_B_PIN                 26

// Core 1 Stack
#define CORE1_STACK_WORDS         4096 
static uint32_t core1_stack[CORE1_STACK_WORDS] __attribute__((aligned(8)));

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// ===========================================================================
// --- 3. SHARED MEMORY ---
// ===========================================================================
static float sensor_ring_buffer[RING_BUFFER_SIZE];
static volatile int rb_head = 0; 
static float exchange_buffer[EI_FEATURE_SIZE];
static volatile bool ai_is_busy = false;

// Driver Instances
static bno08x_driver_t bno08x;
static drv2605_t haptic;

// ===========================================================================
// --- 4. DATA HELPERS ---
// ===========================================================================
void rb_push(float x, float y, float z, float r, float p, float yaw) {
    sensor_ring_buffer[rb_head++] = x; sensor_ring_buffer[rb_head++] = y;
    sensor_ring_buffer[rb_head++] = z; sensor_ring_buffer[rb_head++] = r;
    sensor_ring_buffer[rb_head++] = p; sensor_ring_buffer[rb_head++] = yaw;
    if (rb_head >= RING_BUFFER_SIZE) rb_head = 0;
}

void rb_unroll_to_exchange() {
    int count = EI_FEATURE_SIZE;
    int start_idx = rb_head - count;
    if (start_idx < 0) {
        start_idx += RING_BUFFER_SIZE;
        int part_a = RING_BUFFER_SIZE - start_idx;
        memcpy(exchange_buffer, &sensor_ring_buffer[start_idx], part_a * sizeof(float));
        memcpy(&exchange_buffer[part_a], &sensor_ring_buffer[0], (count - part_a) * sizeof(float));
    } else {
        memcpy(exchange_buffer, &sensor_ring_buffer[start_idx], count * sizeof(float));
    }
}

// ===========================================================================
// --- 5. CORE 1: THE BRAIN ---
// ===========================================================================
typedef struct {
    uint32_t spell_id;
    float confidence;
    float anomaly;
    bool detected;
} ai_result_t;

void core1_entry() {
    while (true) {
        uint32_t cmd = multicore_fifo_pop_blocking();
        if (cmd == 1) {
            spell_decision_t res = ei_bridge_run_inference(exchange_buffer, EI_FEATURE_SIZE);
            
            ai_result_t out;
            out.confidence = res.confidence;
            out.anomaly = res.anomaly_score;
            out.spell_id = SPELL_ID_NONE;

            // Mapping strings to IDs
            if (strcmp(res.label, "aguamenti") == 0) out.spell_id = SPELL_ID_AGUAMENTI;
            else if (strcmp(res.label, "stupefy") == 0) out.spell_id = SPELL_ID_STUPEFY;

            multicore_fifo_push_blocking(out.spell_id);
            multicore_fifo_push_blocking((uint32_t)(out.confidence * 100));
            multicore_fifo_push_blocking((uint32_t)(out.anomaly * 100));
        }
    }
}

// ===========================================================================
// --- 6. CORE 0: THE BODY (Init & Loop) ---
// ===========================================================================
int main() {
    stdio_init_all();
    sleep_ms(3000); 
    printf("=== WAND SYSTEM BOOT ===\n");
    printf("IMU: I2C0 (4,5) | Haptic: I2C1 (2,3) | IR: 6,16\n");

    // --- 1. Init Main I2C (IMU) ---
    printf("Init: IMU I2C...");
    i2c_init(I2C_IMU_PORT, 400 * 1000);
    gpio_set_function(I2C_IMU_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_IMU_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_IMU_SDA_PIN);
    gpio_pull_up(I2C_IMU_SCL_PIN);
    printf("OK\n");

    // --- 2. Init Haptic I2C (Secondary) ---
    printf("Init: Haptic I2C...");
    i2c_init(I2C_HAPTIC_PORT, 400 * 1000);
    gpio_set_function(I2C_HAPTIC_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_HAPTIC_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_HAPTIC_SDA_PIN);
    gpio_pull_up(I2C_HAPTIC_SCL_PIN);
    printf("OK\n");

    // --- 3. Init Haptics (Soft Fail) ---
    printf("Init: Haptics Driver...\n");
    // Pass the SECOND I2C port (I2C_HAPTIC_PORT) here
    if (drv2605_init(&haptic, I2C_HAPTIC_PORT, I2C_HAPTIC_SDA_PIN, I2C_HAPTIC_SCL_PIN)) {
        drv2605_set_mode(&haptic, DRV2605_MODE_INTTRIG);
        drv2605_select_library(&haptic, 1);
        printf("Haptics OK\n");
    } else {
        printf("Haptics SKIP (Not Connected)\n");
    }
    
    // --- 4. Init LEDs ---
    printf("Init: LEDs...\n");
    controller_init(); 
    printf("LEDs OK\n");

    // --- 5. Init IMU (Critical) ---
    printf("Init: IMU Driver...\n");
    if (!bno08x_begin_i2c(&bno08x, I2C_IMU_PORT, BNO08x_I2CADDR_DEFAULT, -1)) {
        printf("IMU FAIL!\n");
        while(1) { sleep_ms(100); } 
    }
    printf("IMU OK\n");
    
    bno08x_enable_report(&bno08x, SH2_ARVR_STABILIZED_RV, SENSOR_POLL_US);
    bno08x_enable_report(&bno08x, SH2_ACCELEROMETER, SENSOR_POLL_US);

    // --- 6. Init IR ---
    printf("Init: IR...\n");
    ir_emitter_init(TX_PIN); 
    ir_receiver_init(RX_PIN);
    printf("IR OK\n");

    // --- 7. Init Core 1 ---
    printf("Launching Core 1...\n");
    multicore_launch_core1_with_stack(core1_entry, core1_stack, sizeof(core1_stack));
    printf("Core 1 Launched\n");

    hb_init(16, 16); 
    hb_draw();

    // --- State Variables ---
    uint64_t last_poll_time = 0;
    uint64_t last_ai_check = 0;
    uint64_t next_valid_cast_time = 0; 
    
    uint32_t pending_spell_id = SPELL_ID_NONE;
    int      consecutive_matches = 0;

    sh2_SensorValue_t val;
    sh2_Accelerometer_t acc = {0};
    struct { float y, p, r; } ypr = {0};

    printf("=== SYSTEM READY ===\n");

    while (true) {
        uint64_t now_us = to_us_since_boot(get_absolute_time());
        uint64_t now_ms = now_us / 1000;

        // -----------------------------------------------------------
        // 1. SENSOR POLLING (74Hz)
        // -----------------------------------------------------------
        if (now_us - last_poll_time >= SENSOR_POLL_US) {
            last_poll_time = now_us;
            while (bno08x_get_sensor_event(&bno08x, &val)) {
                if (val.sensorId == SH2_ARVR_STABILIZED_RV) {
                    float qr = val.un.arvrStabilizedRV.real;
                    float qi = val.un.arvrStabilizedRV.i;
                    float qj = val.un.arvrStabilizedRV.j;
                    float qk = val.un.arvrStabilizedRV.k;
                    float sqr = qr*qr, sqi = qi*qi, sqj = qj*qj, sqk = qk*qk;
                    ypr.y = atan2f(2.0f * (qi * qj + qk * qr), (sqi - sqj - sqk + sqr));
                    ypr.p = asinf(-2.0f * (qi * qk - qj * qr) / (sqi + sqj + sqk + sqr));
                    ypr.r = atan2f(2.0f * (qj * qk + qi * qr), (-sqi - sqj + sqk + sqr));
                    float r2d = 180.0f / M_PI;
                    ypr.y *= r2d; ypr.p *= r2d; ypr.r *= r2d;
                } else if (val.sensorId == SH2_ACCELEROMETER) {
                    acc = val.un.accelerometer;
                }
            }
            rb_push(acc.x, acc.y, acc.z, ypr.r, ypr.p, ypr.y);
        }

        // -----------------------------------------------------------
        // 2. AI TRIGGER
        // -----------------------------------------------------------
        if (!ai_is_busy && (now_ms - last_ai_check >= AI_CHECK_INTERVAL_MS)) {
            if (now_ms >= next_valid_cast_time) {
                rb_unroll_to_exchange();
                ai_is_busy = true;
                last_ai_check = now_ms;
                multicore_fifo_push_blocking(1); 
            }
        }

        // -----------------------------------------------------------
        // 3. AI RESULT
        // -----------------------------------------------------------
        if (multicore_fifo_rvalid()) {
            uint32_t raw_id = multicore_fifo_pop_blocking();
            uint32_t raw_conf = multicore_fifo_pop_blocking();
            uint32_t raw_anom = multicore_fifo_pop_blocking();
            ai_is_busy = false;

            float confidence = (float)raw_conf / 100.0f;
            float anomaly = (float)raw_anom / 100.0f;

            printf("AI: ID=%lu | Conf=%.2f | Anom=%.2f ", raw_id, confidence, anomaly);

            bool match = false;
            if (confidence >= AI_CONFIDENCE_THRESHOLD && anomaly <= AI_ANOMALY_THRESHOLD) {
                if (raw_id != SPELL_ID_NONE) {
                    match = true;
                    if (raw_id == pending_spell_id) {
                        consecutive_matches++;
                        printf("[MATCH %d/%d]\n", consecutive_matches, CONSECUTIVE_MATCHES_REQ);
                    } else {
                        pending_spell_id = raw_id;
                        consecutive_matches = 1;
                        printf("[NEW DETECT]\n");
                    }
                }
            } 
            
            if (!match) {
                consecutive_matches = 0;
                pending_spell_id = SPELL_ID_NONE;
                printf("[NOPE]\n");
            }

            if (consecutive_matches >= CONSECUTIVE_MATCHES_REQ) {
                printf(">>> FIRE SPELL %lu!\n", pending_spell_id);
                if (drv2605_is_playing(&haptic)) drv2605_play_cast_feedback(&haptic); 
                ir_emitter_start(2, (uint8_t)pending_spell_id, 3);
                controller_start_spell((uint8_t)pending_spell_id);

                consecutive_matches = 0;
                pending_spell_id = SPELL_ID_NONE;
                next_valid_cast_time = now_ms + POST_CAST_LOCKOUT_MS;
            }
        }

        // -----------------------------------------------------------
        // 4. NON-BLOCKING IO
        // -----------------------------------------------------------
        ir_decoded_data_t rx_data;
        if (ir_receiver_decode(&rx_data)) {
            if (rx_data.protocol == IR_PROTOCOL_NEC && rx_data.address == 1) { 
                printf(">>> HIT! Cmd: %d\n", rx_data.command);
                if (drv2605_is_playing(&haptic)) drv2605_play_hit_feedback(&haptic);
                hb_update(-1); hb_draw();
                ws2812_fill(255, 0, 0); ws2812_update(); 
                sleep_ms(50); ws2812_clear();
            }
        }
        
        ir_emitter_update();
        controller_update(); 
        sleep_ms(1);
    }
}