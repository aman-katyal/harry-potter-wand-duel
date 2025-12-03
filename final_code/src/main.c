/**
 * @file main.c
 * @brief Multi-core Wand Duel - Fully Parameterized
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "hardware/i2c.h"
#include "hardware/watchdog.h"

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
// --- 1. SPELL CONFIGURATION ---
// ===========================================================================
#define SPELL_LABEL_1             "aguamenti"
#define SPELL_ID_1                1
#define SPELL_LABEL_2             "stupefy"
#define SPELL_ID_2                2

// ===========================================================================
// --- 2. SENSITIVITY & TRIGGER TUNING ---
// ===========================================================================

// [CRITICAL] How many successful AI frames are needed to cast?
// The AI runs approx 4 times a second (every 250ms).
// 4 frames = ~1 second of holding the gesture.
// Increase this to make it harder/less sensitive. Decrease for instant casting.
#define FRAMES_TO_TRIGGER         6     

// How fast the "charge" decays if you stop doing the gesture (0-100 scale)
#define DECAY_RATE_IDLE           5     
#define DECAY_RATE_WRONG_SPELL    10    

// Internal Math (Do not edit)
#define MAX_SCORE                 100
#define SCORE_INCREMENT           (MAX_SCORE / FRAMES_TO_TRIGGER)
#define SCORE_TRIGGER_LEVEL       (MAX_SCORE - 5) // Trigger when nearly full

// ===========================================================================
// --- 3. TIMING PARAMETERS (milliseconds) ---
// ===========================================================================

// Delay between "Spell Recognized" and "IR Packet Sent"
// (Gives time for sound effect buildup or animation wind-up)
#define CAST_PRE_DELAY_MS         600   

// How long you must wait AFTER firing before firing again
#define CAST_COOLDOWN_MS          1500  

// How long to ignore the AI after a successful trigger 
// (Prevents the wand from machine-gunning the same spell)
#define AI_IGNORE_WINDOW_MS       2500  

// Time to stay dead before respawning
#define RESPAWN_DELAY_MS          7000

// ===========================================================================
// --- 4. HARDWARE CONFIGURATION ---
// ===========================================================================
#define MY_PLAYER_ID              2
#define OPPONENT_PLAYER_ID        1

#define I2C_PORT                  i2c0
#define I2C_SDA_PIN               16
#define I2C_SCL_PIN               17
#define TX_PIN                    36
#define RX_PIN                    15
#define BTN_A_PIN                 21
#define BTN_B_PIN                 26

// Queue Buffer Size (Max pending spells)
#define QUEUE_SIZE                4     
// How many IR packets to send per cast (Redundancy)
#define PACKET_REPEATS            3

// Sensor Settings
#define SAMPLING_FREQ_HZ          74
#define SENSOR_POLL_US            (1000000 / SAMPLING_FREQ_HZ)
#define INFERENCE_INTERVAL_MS     250
#define EI_YPR_IN_DEGREES         1

// Motion Gating: Minimum movement (m/s^2 deviation from gravity) required to run AI
#define MOTION_THRESHOLD          3.0f 

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// ===========================================================================
// --- QUEUES & STRUCTS ---
// ===========================================================================
static queue_t result_queue;
typedef enum { HAPTIC_NONE, HAPTIC_CAST, HAPTIC_HIT } haptic_cmd_t;
static queue_t haptic_queue;

typedef struct {
    uint8_t buffer[QUEUE_SIZE];
    int head;
    int tail;
    int count;
} SpellQueue;

static SpellQueue tx_queue = { .head = 0, .tail = 0, .count = 0 };

void enqueue_spell(uint8_t spell_id) {
    if (tx_queue.count >= QUEUE_SIZE) return;
    tx_queue.buffer[tx_queue.tail] = spell_id;
    tx_queue.tail = (tx_queue.tail + 1) % QUEUE_SIZE;
    tx_queue.count++;
    printf(">> Spell %d Queued (Firing in %d ms)...\n", spell_id, CAST_PRE_DELAY_MS);
}

uint8_t dequeue_spell() {
    if (tx_queue.count == 0) return 0;
    uint8_t spell = tx_queue.buffer[tx_queue.head];
    tx_queue.head = (tx_queue.head + 1) % QUEUE_SIZE;
    tx_queue.count--;
    return spell;
}

// ===========================================================================
// --- CORE 1: SENSOR WORKER ---
// ===========================================================================
static bno08x_driver_t bno08x;
static drv2605_t haptic;

void core1_entry() {
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    drv2605_init(&haptic, I2C_PORT, I2C_SDA_PIN, I2C_SCL_PIN);
    drv2605_set_mode(&haptic, DRV2605_MODE_INTTRIG);
    drv2605_select_library(&haptic, 1);

    if (!bno08x_begin_i2c(&bno08x, I2C_PORT, BNO08x_I2CADDR_DEFAULT, -1)) {
        while(1) { sleep_ms(100); } 
    }
    bno08x_enable_report(&bno08x, SH2_ARVR_STABILIZED_RV, SENSOR_POLL_US);
    bno08x_enable_report(&bno08x, SH2_ACCELEROMETER, SENSOR_POLL_US);

    size_t frame_size = ei_bridge_get_input_frame_size();
    int axes = ei_bridge_get_axis_count();
    float *features = (float*)calloc(frame_size, sizeof(float));
    
    sh2_SensorValue_t val;
    sh2_Accelerometer_t acc = {0};
    struct { float y, p, r; } ypr = {0};
    
    int samples = 0;
    uint64_t last_poll_time = 0;
    uint64_t last_inf_time = 0;
    int move_idx = frame_size - axes;

    while(true) {
        uint64_t now = to_us_since_boot(get_absolute_time());

        haptic_cmd_t h_cmd;
        if (queue_try_remove(&haptic_queue, &h_cmd)) {
            if (h_cmd == HAPTIC_CAST) drv2605_play_cast_feedback(&haptic);
            if (h_cmd == HAPTIC_HIT)  drv2605_play_hit_feedback(&haptic);
        }

        if (now - last_poll_time >= SENSOR_POLL_US) {
            last_poll_time = now;
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
                    if (EI_YPR_IN_DEGREES) {
                        float r2d = 180.0f / M_PI;
                        ypr.y *= r2d; ypr.p *= r2d; ypr.r *= r2d;
                    }
                } else if (val.sensorId == SH2_ACCELEROMETER) {
                    acc = val.un.accelerometer;
                }
            }
            memmove(features, features + axes, move_idx * sizeof(float));
            features[move_idx+0] = acc.x; features[move_idx+1] = acc.y; features[move_idx+2] = acc.z;
            features[move_idx+3] = ypr.r; features[move_idx+4] = ypr.p; features[move_idx+5] = ypr.y;
            samples++;
        }

        float mag = sqrtf(acc.x*acc.x + acc.y*acc.y + acc.z*acc.z);
        if (fabsf(mag - 9.8f) > MOTION_THRESHOLD) {
            if (samples >= (frame_size/axes)) {
                uint64_t now_ms = now / 1000;
                if ((now_ms - last_inf_time) >= INFERENCE_INTERVAL_MS) {
                    last_inf_time = now_ms;
                    spell_decision_t res = ei_bridge_run_inference(features, frame_size);
                    if (!queue_try_add(&result_queue, &res)) {}
                }
            }
        }
    }
}

// ===========================================================================
// --- CORE 0: GAME LOGIC ---
// ===========================================================================
void request_haptic(haptic_cmd_t type) {
    queue_try_add(&haptic_queue, &type);
}

void process_hit_effects(uint8_t spell_num) {
    int anim_id = 0;
    int damage = 0;
    switch(spell_num) {
        case 1: printf("Hit: Aguamenti!\n"); anim_id = 3; damage = 1; break;
        case 2: printf("Hit: Stupefy!\n"); anim_id = 0; damage = 1; break;
        default: return;
    }
    hb_update(-damage);
    ws2812_clear();
    controller(anim_id, 16, 16);
    hb_draw();
}

int clamp_score(int score) {
    if (score > MAX_SCORE) return MAX_SCORE;
    if (score < 0) return 0;
    return score;
}

int main() {
    stdio_init_all();
    sleep_ms(2000);
    printf("=== WAND DUEL: PARAMETERIZED ===\n");
    printf("Frames to Trigger: %d (Score/Frame: %d)\n", FRAMES_TO_TRIGGER, SCORE_INCREMENT);

    watchdog_enable(2000, 1);

    queue_init(&result_queue, sizeof(spell_decision_t), 4);
    queue_init(&haptic_queue, sizeof(haptic_cmd_t), 4);

    ir_emitter_init(TX_PIN);
    ir_receiver_init(RX_PIN);
    ws2812_init();
    hb_init(16, 16);
    hb_draw();

    gpio_init(BTN_A_PIN); gpio_set_dir(BTN_A_PIN, GPIO_IN); gpio_pull_down(BTN_A_PIN);
    gpio_init(BTN_B_PIN); gpio_set_dir(BTN_B_PIN, GPIO_IN); gpio_pull_down(BTN_B_PIN);

    multicore_launch_core1(core1_entry);

    ir_decoded_data_t rx_data;
    uint32_t death_time = 0;
    bool is_dead = false;
    int score_s1 = 0, score_s2 = 0;
    
    uint32_t last_trigger_time = 0;
    uint32_t next_cast_allowed_time = 0; 
    spell_decision_t ai_result;

    while (true) {
        watchdog_update();
        uint32_t now = to_ms_since_boot(get_absolute_time());

        // 1. AI Processing
        if (queue_try_remove(&result_queue, &ai_result)) {
            // If inside the "Ignore Window", do nothing
            if (now - last_trigger_time < AI_IGNORE_WINDOW_MS) {
                score_s1 = 0; score_s2 = 0;
            } else {
                if (ai_result.detected) {
                    if (strcmp(ai_result.label, SPELL_LABEL_1) == 0) {
                        score_s1 += SCORE_INCREMENT; score_s2 -= DECAY_RATE_WRONG_SPELL;
                    } else if (strcmp(ai_result.label, SPELL_LABEL_2) == 0) {
                        score_s2 += SCORE_INCREMENT; score_s1 -= DECAY_RATE_WRONG_SPELL;
                    }
                } else {
                    score_s1 -= DECAY_RATE_IDLE; score_s2 -= DECAY_RATE_IDLE;
                }
                score_s1 = clamp_score(score_s1); score_s2 = clamp_score(score_s2);

                // Trigger Logic
                if (score_s1 >= SCORE_TRIGGER_LEVEL || score_s2 >= SCORE_TRIGGER_LEVEL) {
                    uint8_t spell = (score_s1 >= SCORE_TRIGGER_LEVEL) ? SPELL_ID_1 : SPELL_ID_2;
                    
                    enqueue_spell(spell);
                    request_haptic(HAPTIC_CAST); 

                    // Schedule Firing
                    uint32_t earliest_fire_time = now + CAST_PRE_DELAY_MS;
                    if (next_cast_allowed_time < earliest_fire_time) {
                        next_cast_allowed_time = earliest_fire_time;
                    }

                    // Reset Score & Cooldown
                    score_s1 = 0; score_s2 = 0;
                    last_trigger_time = now;
                }
            }
        }

        // 2. Respawn
        if (is_dead) {
            if (now - death_time > RESPAWN_DELAY_MS) {
                is_dead = false;
                hb_reset();
                hb_draw();
                printf(">>> Respawned\n");
            }
        } 
        // 3. Manual Buttons (Test)
        else {
            uint8_t btn_spell = 0;
            if (gpio_get(BTN_A_PIN)) btn_spell = SPELL_ID_1;
            if (gpio_get(BTN_B_PIN)) btn_spell = SPELL_ID_2;
            
            if (btn_spell > 0) {
                enqueue_spell(btn_spell);
                request_haptic(HAPTIC_CAST);
                
                uint32_t fire_time = now + CAST_PRE_DELAY_MS;
                if (next_cast_allowed_time < fire_time) {
                    next_cast_allowed_time = fire_time;
                }
                sleep_ms(200);
            }
        }

        // 4. Firing Logic
        if (!is_dead && tx_queue.count > 0) {
            if (now >= next_cast_allowed_time) {
                uint8_t spell_to_cast = dequeue_spell();
                if (spell_to_cast > 0) {
                    ir_emitter_start(MY_PLAYER_ID, spell_to_cast, PACKET_REPEATS);
                    printf(">>> FIRING SPELL %d (t=%lu)\n", spell_to_cast, now);
                    
                    // Apply Cooldown AFTER firing
                    next_cast_allowed_time = now + CAST_COOLDOWN_MS;
                }
            }
        }

        // 5. Receiver
        if (ir_receiver_decode(&rx_data)) {
            if (rx_data.protocol == IR_PROTOCOL_NEC && rx_data.address == OPPONENT_PLAYER_ID) {
                request_haptic(HAPTIC_HIT);
                process_hit_effects(rx_data.command);
                if (hb_current() <= 0) {
                    is_dead = true;
                    death_time = now;
                    loser_screen(16, 16);
                }
            }
        }

        ir_emitter_update();
        sleep_ms(1);
    }
}