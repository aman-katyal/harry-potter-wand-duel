/**
 * @file main.c
 * @brief Dual-Core Wand Duel - Self-Hit Toggle Added
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "hardware/i2c.h"

// --- Configuration ---
#include "sys_config.h"

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
// --- AI DATA BUFFERS ---
// ===========================================================================
#define WINDOW_MS 1650
#define SAMPLES_PER_WINDOW ((WINDOW_MS * SENSOR_POLL_RATE_HZ) / 1000)
#define AXES_COUNT 6 
#define EI_FEATURE_SIZE (SAMPLES_PER_WINDOW * AXES_COUNT)
#define RING_BUFFER_SIZE (3 * SENSOR_POLL_RATE_HZ * AXES_COUNT)

static float sensor_ring_buffer[RING_BUFFER_SIZE];
static volatile int rb_head = 0; 
static float exchange_buffer[EI_FEATURE_SIZE];
static volatile bool ai_is_busy = false;

// Driver Instances
static bno08x_driver_t bno08x;
static drv2605_t haptic;

// Core 1 Stack
static uint32_t core1_stack[4096] __attribute__((aligned(8)));

// ===========================================================================
// --- DATA HELPERS ---
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
// --- CORE 1: THE BRAIN (AI) ---
// ===========================================================================
void core1_entry() {
    while (true) {
        uint32_t cmd = multicore_fifo_pop_blocking();
        if (cmd == 1) {
            spell_decision_t res = ei_bridge_run_inference(exchange_buffer, EI_FEATURE_SIZE);
            
            ai_result_t result;
            result.parts.spell_id = SPELL_LOGIC_NONE;
            
            if (strcmp(res.label, "aguamenti") == 0) result.parts.spell_id = SPELL_LOGIC_AGUAMENTI;
            else if (strcmp(res.label, "stupefy") == 0) result.parts.spell_id = SPELL_LOGIC_STUPEFY;

            result.parts.confidence = (uint8_t)(res.confidence * 100);
            
            float anom_clamped = (res.anomaly_score > 10.0f) ? 10.0f : res.anomaly_score;
            result.parts.anomaly = (uint8_t)(anom_clamped * 10); 

            multicore_fifo_push_blocking(result.packed);
        }
    }
}

// ===========================================================================
// --- CORE 0: THE BODY (Game Loop) ---
// ===========================================================================
int main() {
    stdio_init_all();
    sleep_ms(2000); 
    printf("=== WAND SYSTEM BOOT (ID: %d) ===\n", PLAYER_ID);

    // --- 1. Init Hardware ---
    i2c_init(I2C_IMU_PORT, 400 * 1000);
    gpio_set_function(PIN_IMU_SDA, GPIO_FUNC_I2C);
    gpio_set_function(PIN_IMU_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(PIN_IMU_SDA);
    gpio_pull_up(PIN_IMU_SCL);

    // Haptics share I2C0
    
    if (drv2605_init(&haptic, I2C_HAPTIC_PORT, PIN_HAPTIC_SDA, PIN_HAPTIC_SCL)) {
        drv2605_set_mode(&haptic, DRV2605_MODE_INTTRIG);
        drv2605_select_library(&haptic, 1);
    }

    ws2812_init(); 
    ws2812_clear();
    hb_init(MATRIX_WIDTH, MATRIX_HEIGHT);
    hb_draw(); 

    // Buttons (Active High)
    gpio_init(PIN_BTN_SHIELD);
    gpio_set_dir(PIN_BTN_SHIELD, GPIO_IN);
    gpio_disable_pulls(PIN_BTN_SHIELD); 

    if (!bno08x_begin_i2c(&bno08x, I2C_IMU_PORT, BNO08x_I2CADDR_DEFAULT, -1)) {
        printf("IMU FAIL!\n");
        ws2812_fill(50, 0, 0); ws2812_update();
        while(1) { sleep_ms(100); } 
    }
    bno08x_enable_report(&bno08x, SH2_ARVR_STABILIZED_RV, SENSOR_POLL_US);
    bno08x_enable_report(&bno08x, SH2_ACCELEROMETER, SENSOR_POLL_US);

    ir_emitter_init(PIN_IR_TX); 
    ir_receiver_init(PIN_IR_RX);

    multicore_launch_core1_with_stack(core1_entry, core1_stack, sizeof(core1_stack));

    // --- Game State ---
    uint64_t last_poll_time = 0;
    uint64_t last_ai_check = 0;
    uint64_t next_valid_cast_time = 0; 
    uint64_t shield_cooldown_end = 0;

    uint32_t candidate_spell = SPELL_LOGIC_NONE;
    int      spell_score = 0;

    sh2_SensorValue_t val;
    sh2_Accelerometer_t acc = {0};
    struct { float y, p, r; } ypr = {0};

    printf("=== READY ===\n");

    while (true) {
        uint64_t now_us = to_us_since_boot(get_absolute_time());
        uint64_t now_ms = now_us / 1000;

        // ----------------------------------------
        // 0. SHIELD BUTTON LOGIC
        // ----------------------------------------
        if (gpio_get(PIN_BTN_SHIELD)) {
            if (now_ms > shield_cooldown_end) {
                printf(">>> SHIELD ACTIVE! <<<\n");
                
                controller(0, MATRIX_WIDTH, MATRIX_HEIGHT);
                
                ws2812_clear();
                hb_draw();
                
                ir_decoded_data_t trash;
                while(ir_receiver_decode(&trash)) {
                    ir_receiver_resume();
                }

                shield_cooldown_end = now_ms + SHIELD_COOLDOWN_MS;
                printf(">>> Shield Down. Cooldown active.\n");
            } 
        }

        // ----------------------------------------
        // 1. POLLING
        // ----------------------------------------
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
                     ypr.y *= 57.29578f; ypr.p *= 57.29578f; ypr.r *= 57.29578f;
                } else if (val.sensorId == SH2_ACCELEROMETER) {
                    acc = val.un.accelerometer;
                }
            }
            rb_push(acc.x, acc.y, acc.z, ypr.r, ypr.p, ypr.y);
        }

        // ----------------------------------------
        // 2. AI TRIGGER
        // ----------------------------------------
        if (!ai_is_busy && (now_ms - last_ai_check >= AI_CHECK_INTERVAL_MS)) {
            if (now_ms >= next_valid_cast_time) {
                rb_unroll_to_exchange();
                ai_is_busy = true;
                last_ai_check = now_ms;
                multicore_fifo_push_blocking(1); 
            }
        }

        // ----------------------------------------
        // 3. CASTING LOGIC
        // ----------------------------------------
        if (multicore_fifo_rvalid()) {
            uint32_t packed = multicore_fifo_pop_blocking();
            ai_is_busy = false;

            ai_result_t res;
            res.packed = packed;
            
            float confidence = res.parts.confidence / 100.0f;
            float anomaly    = res.parts.anomaly / 10.0f;
            uint8_t detected_id = res.parts.spell_id;

            float anomaly_limit = 100.0f; 
            if (detected_id == SPELL_LOGIC_AGUAMENTI) {
                anomaly_limit = ANOMALY_THRESH_AGUAMENTI;
            } else if (detected_id == SPELL_LOGIC_STUPEFY) {
                anomaly_limit = ANOMALY_THRESH_STUPEFY;
            }

            bool is_valid = (confidence >= AI_CONFIDENCE_THRESHOLD) && (anomaly <= anomaly_limit);

            if (is_valid && detected_id != SPELL_LOGIC_NONE) {
                if (detected_id == candidate_spell) {
                    spell_score++;
                } else {
                    candidate_spell = detected_id;
                    spell_score = 1;
                }
            } else {
                if (spell_score > 0) spell_score -= SPELL_DECAY_RATE;
            }

            // FIRE SPELL
            if (spell_score >= SPELL_TRIGGER_TARGET) {
                printf(">>> CAST SPELL %d! <<<\n", (int)candidate_spell);
                
                if (drv2605_is_playing(&haptic)) drv2605_play_cast_feedback(&haptic);
                
                uint8_t ir_cmd = (candidate_spell == SPELL_LOGIC_AGUAMENTI) ? 10 : 20; 
                ir_emitter_start(PLAYER_ID, ir_cmd, 3); 
                
                spell_score = 0;
                candidate_spell = SPELL_LOGIC_NONE;
                next_valid_cast_time = now_ms + POST_CAST_LOCKOUT_MS;
            }
        }

        // ----------------------------------------
        // 4. HIT LOGIC
        // ----------------------------------------
        ir_decoded_data_t rx_data;
        if (ir_receiver_decode(&rx_data)) {
            if (rx_data.protocol == IR_PROTOCOL_NEC) {
                
                // --- MODIFIED SELF-HIT LOGIC ---
                // Only ignore if it is ME AND Self-Hit is disabled
                if (!ALLOW_SELF_HIT && rx_data.address == PLAYER_ID) {
                    printf("Ignored self-hit (ID: %d)\n", rx_data.address);
                } 
                else {
                    // Valid Hit (Either Enemy or Self-Hit Allowed)
                    printf("HIT by Player %d! Cmd: %d\n", rx_data.address, rx_data.command);
                    
                    if (drv2605_is_playing(&haptic)) drv2605_play_hit_feedback(&haptic);
                    
                    hb_update(-2); 
                    
                    ws2812_clear();
                    
                    if (rx_data.command == 10) {
                        controller(2, MATRIX_WIDTH, MATRIX_HEIGHT);
                    } 
                    else if (rx_data.command == 20) {
                        controller(1, MATRIX_WIDTH, MATRIX_HEIGHT);
                    }
                    
                    if (hb_current() <= 0) {
                        loser_screen(MATRIX_WIDTH, MATRIX_HEIGHT);
                        sleep_ms(2000);
                        hb_reset();
                    }
                    
                    hb_draw();
                }
            }
            ir_receiver_resume();
        }

        ir_emitter_update();
    }
}