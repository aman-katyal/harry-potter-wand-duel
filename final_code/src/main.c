/**
 * @file main.c
 * @brief Multi-core Wand Duel - Buffered & Smoothed Version
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/i2c.h"
#include "hardware/timer.h"

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
// --- USER CONFIGURATION ---
// ===========================================================================

// --- Spell Definitions ---
#define SPELL_LABEL_1             "aguamenti"
#define SPELL_ID_1                1
#define SPELL_LABEL_2             "stupefy"
#define SPELL_ID_2                2

// --- Recognition "Smoothing" Settings ---
// Scores go from 0 to 100.
#define SCORE_TRIGGER_LEVEL       80    // Score needed to trigger a cast
#define SCORE_INCREMENT           25    // Points gained per successful detection frame
#define SCORE_DECAY_IDLE          5     // Points lost when IDLE is seen
#define SCORE_DECAY_OPPOSITE      10    // Points lost when the OTHER spell is seen
#define CONFIDENCE_THRESHOLD      0.85f // AI confidence required to add points

// --- Game Mechanics ---
#define CAST_COOLDOWN_MS          1200  // Time between spells (prevents spam)
#define QUEUE_SIZE                4     // Max spells to buffer
#define PACKET_REPEATS            3
#define RESPAWN_DELAY_MS          3000
#define MY_PLAYER_ID              1
#define OPPONENT_PLAYER_ID        2

// --- Hardware Pins ---
#define I2C_PORT                  i2c0
#define I2C_SDA_PIN               16
#define I2C_SCL_PIN               17
#define TX_PIN                    36
#define RX_PIN                    15
#define BTN_A_PIN                 21
#define BTN_B_PIN                 26

// --- IMU Settings ---
#define SAMPLING_FREQ_HZ          74
#define SENSOR_POLL_US            (1000000 / SAMPLING_FREQ_HZ)
#define MOVING_WINDOW_MS          2000
#define INFERENCE_INTERVAL_MS     250
#define EI_YPR_IN_DEGREES         1

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// ===========================================================================
// --- SHARED GLOBALS ---
// ===========================================================================
static bno08x_driver_t bno08x;
static volatile struct { float y, p, r; } g_ypr;
static volatile sh2_Accelerometer_t g_acc;
static volatile bool g_sensor_updated = false;
static drv2605_t haptic;

// ===========================================================================
// --- SPELL QUEUE SYSTEM ---
// ===========================================================================
typedef struct {
    uint8_t buffer[QUEUE_SIZE];
    int head;
    int tail;
    int count;
} SpellQueue;

static SpellQueue tx_queue = { .head = 0, .tail = 0, .count = 0 };

void enqueue_spell(uint8_t spell_id) {
    if (tx_queue.count >= QUEUE_SIZE) {
        printf(">> Queue Full! Discarding spell %d\n", spell_id);
        return; 
    }
    tx_queue.buffer[tx_queue.tail] = spell_id;
    tx_queue.tail = (tx_queue.tail + 1) % QUEUE_SIZE;
    tx_queue.count++;
    printf(">> Queued Spell %d (Count: %d)\n", spell_id, tx_queue.count);
}

uint8_t dequeue_spell() {
    if (tx_queue.count == 0) return 0;
    uint8_t spell = tx_queue.buffer[tx_queue.head];
    tx_queue.head = (tx_queue.head + 1) % QUEUE_SIZE;
    tx_queue.count--;
    return spell;
}

// ===========================================================================
// --- CORE 1: SENSOR & AI WORKER ---
// ===========================================================================
bool sensor_timer_callback(repeating_timer_t *t) {
    static sh2_SensorValue_t val;
    if (bno08x_get_sensor_event(&bno08x, &val)) {
        if (val.sensorId == SH2_ARVR_STABILIZED_RV) {
            float qr = val.un.arvrStabilizedRV.real;
            float qi = val.un.arvrStabilizedRV.i;
            float qj = val.un.arvrStabilizedRV.j;
            float qk = val.un.arvrStabilizedRV.k;
            float sqr = qr*qr, sqi = qi*qi, sqj = qj*qj, sqk = qk*qk;

            g_ypr.y = atan2f(2.0f * (qi * qj + qk * qr), (sqi - sqj - sqk + sqr));
            g_ypr.p = asinf(-2.0f * (qi * qk - qj * qr) / (sqi + sqj + sqk + sqr));
            g_ypr.r = atan2f(2.0f * (qj * qk + qi * qr), (-sqi - sqj + sqk + sqr));

            if (EI_YPR_IN_DEGREES) {
                float r2d = 180.0f / M_PI;
                g_ypr.y *= r2d; g_ypr.p *= r2d; g_ypr.r *= r2d;
            }
        } else if (val.sensorId == SH2_ACCELEROMETER) {
            memcpy((void*)&g_acc, &val.un.accelerometer, sizeof(sh2_Accelerometer_t));
        }
        g_sensor_updated = true;
    }
    return true;
}

void core1_entry() {
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    if (!bno08x_begin_i2c(&bno08x, I2C_PORT, BNO08x_I2CADDR_DEFAULT, -1)) while(1);
    bno08x_enable_report(&bno08x, SH2_ARVR_STABILIZED_RV, SENSOR_POLL_US);
    bno08x_enable_report(&bno08x, SH2_ACCELEROMETER, SENSOR_POLL_US);

    size_t frame_size = ei_bridge_get_input_frame_size();
    int axes = ei_bridge_get_axis_count();
    float *features = (float*)calloc(frame_size, sizeof(float));
    
    repeating_timer_t timer;
    add_repeating_timer_us(SENSOR_POLL_US, sensor_timer_callback, NULL, &timer);

    int samples = 0;
    uint64_t last_inf = 0;
    int move_idx = frame_size - axes;

    while(true) {
        if (g_sensor_updated) {
            g_sensor_updated = false;
            memmove(features, features + axes, move_idx * sizeof(float));
            features[move_idx+0] = g_acc.x; features[move_idx+1] = g_acc.y; features[move_idx+2] = g_acc.z;
            features[move_idx+3] = g_ypr.r; features[move_idx+4] = g_ypr.p; features[move_idx+5] = g_ypr.y;

            if (samples < (frame_size/axes)) { samples++; continue; }

            uint64_t now = to_ms_since_boot(get_absolute_time());
            if ((now - last_inf) >= INFERENCE_INTERVAL_MS) {
                last_inf = now;
                spell_decision_t res = ei_bridge_run_inference(features, frame_size);
                
                if (multicore_fifo_wready()) {
                    multicore_fifo_push_blocking((uint32_t)&res);
                    multicore_fifo_pop_blocking();
                }
            }
        }
    }
}

// ===========================================================================
// --- CORE 0: GAME LOGIC ---
// ===========================================================================

void process_hit_effects(uint8_t spell_num) {
    int anim_id = 0;
    int damage = 0;
    switch(spell_num) {
        case SPELL_ID_1: printf("Hit: Aguamenti!\n"); anim_id = 3; damage = 1; break;
        case SPELL_ID_2: printf("Hit: Stupefy!\n"); anim_id = 0; damage = 1; break;
        default: return;
    }
    hb_update(-damage);
    ws2812_clear();
    controller(anim_id, 16, 16); 
    hb_draw();
}

// Helper to clamp scores 0-100
int clamp_score(int score) {
    if (score > 100) return 100;
    if (score < 0) return 0;
    return score;
}

int main() {
    stdio_init_all();
    sleep_ms(2000);
    printf("=== WAND DUEL: BUFFERED & SMOOTHED ===\n");

    ir_emitter_init(TX_PIN);
    ir_receiver_init(RX_PIN);
    ws2812_init();
    hb_init(16, 16);
    hb_draw();

    gpio_init(BTN_A_PIN); gpio_set_dir(BTN_A_PIN, GPIO_IN); gpio_pull_down(BTN_A_PIN);
    gpio_init(BTN_B_PIN); gpio_set_dir(BTN_B_PIN, GPIO_IN); gpio_pull_down(BTN_B_PIN);

    drv2605_init(&haptic, i2c0, I2C_SDA_PIN, I2C_SCL_PIN);
    drv2605_set_mode(&haptic, DRV2605_MODE_INTTRIG);
    drv2605_select_library(&haptic, 1);

    multicore_launch_core1(core1_entry);

    ir_decoded_data_t rx_data;
    uint32_t death_time = 0;
    bool is_dead = false;

    // --- Recognition State (Leaky Integrator) ---
    int score_s1 = 0;
    int score_s2 = 0;
    
    // --- Casting State ---
    uint32_t next_cast_allowed_time = 0;

    while (true) {
        uint32_t now = to_ms_since_boot(get_absolute_time());

        // ------------------------------------------------
        // 1. READ AI & UPDATE SCORES (SMOOTHING)
        // ------------------------------------------------
        if (multicore_fifo_rvalid()) {
            uint32_t ptr = multicore_fifo_pop_blocking();
            spell_decision_t *ai = (spell_decision_t*)ptr;

            // Is it Aguamenti?
            if (ai->confidence > CONFIDENCE_THRESHOLD && strcmp(ai->label, SPELL_LABEL_1) == 0) {
                score_s1 += SCORE_INCREMENT;
                score_s2 -= SCORE_DECAY_OPPOSITE; // Penalize the other
            }
            // Is it Stupefy?
            else if (ai->confidence > CONFIDENCE_THRESHOLD && strcmp(ai->label, SPELL_LABEL_2) == 0) {
                score_s2 += SCORE_INCREMENT;
                score_s1 -= SCORE_DECAY_OPPOSITE;
            }
            // Idle / Noise
            else {
                score_s1 -= SCORE_DECAY_IDLE;
                score_s2 -= SCORE_DECAY_IDLE;
            }

            // Clamp scores to keep them sane
            score_s1 = clamp_score(score_s1);
            score_s2 = clamp_score(score_s2);

            // printf("Scores: S1=%d, S2=%d\n", score_s1, score_s2);

            // Check Triggers
            if (score_s1 >= SCORE_TRIGGER_LEVEL) {
                enqueue_spell(SPELL_ID_1);
                // Reset BOTH to zero so we don't double-trigger
                score_s1 = 0; 
                score_s2 = 0;
                drv2605_play_cast_feedback(&haptic); // Quick buzz on queue
            } 
            else if (score_s2 >= SCORE_TRIGGER_LEVEL) {
                enqueue_spell(SPELL_ID_2);
                score_s1 = 0; 
                score_s2 = 0;
                drv2605_play_cast_feedback(&haptic);
            }

            multicore_fifo_push_blocking(1); // Release AI
        }

        // ------------------------------------------------
        // 2. RESPAWN HANDLER
        // ------------------------------------------------
        if (is_dead) {
            if (now - death_time > RESPAWN_DELAY_MS) {
                is_dead = false;
                hb_reset();
                hb_draw();
                printf("*** RESPAWNED ***\n");
            }
            continue; 
        }

        // ------------------------------------------------
        // 3. CAST FROM QUEUE (RATE LIMITING)
        // ------------------------------------------------
        if (now >= next_cast_allowed_time && tx_queue.count > 0) {
            
            uint8_t spell_to_cast = dequeue_spell();
            
            if (spell_to_cast > 0) {
                printf(">>> FIRING SPELL ID: %d\n", spell_to_cast);
                
                // Haptic Feedback for firing
                drv2605_play_cast_feedback(&haptic);
                
                // Send IR
                ir_emitter_start(MY_PLAYER_ID, spell_to_cast, PACKET_REPEATS);
                
                // Set Cooldown
                next_cast_allowed_time = now + CAST_COOLDOWN_MS;
            }
        }

        // ------------------------------------------------
        // 4. IR RECEIVER
        // ------------------------------------------------
        if (ir_receiver_decode(&rx_data)) {
            if (rx_data.protocol == IR_PROTOCOL_NEC && rx_data.address == OPPONENT_PLAYER_ID) {
                process_hit_effects(rx_data.command);
                drv2605_play_hit_feedback(&haptic);

                if (hb_current() <= 0) {
                    printf("*** YOU DIED ***\n");
                    is_dead = true;
                    death_time = now;
                    loser_screen(16, 16);
                }
            }
        }

        ir_emitter_update(); // Keep ticking IR
        sleep_ms(1); 
    }
}