#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "hardware/i2c.h"
#include "hardware/watchdog.h" // Still included for hardware header definitions, but functions are removed.
#include "pico/multicore_lockout.h"
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
// Define the machine learning labels and their corresponding integer IDs.
#define SPELL_LABEL_1             "aguamenti"
#define SPELL_ID_1                1
#define SPELL_LABEL_2             "stupefy"
#define SPELL_ID_2                2

// ===========================================================================
// --- 2. SENSITIVITY & TRIGGER TUNING ---
// ===========================================================================

// [CRITICAL] How many successful AI frames are needed to cast?
// The AI runs approx 4 times a second (every 250ms).
// 6 frames = ~1.5 seconds of holding the gesture.
// Increase this to make it harder/less sensitive. Decrease for instant casting.
#define FRAMES_TO_TRIGGER         6    

// How fast the "charge" decays if you stop doing the gesture (0-100 scale)
#define DECAY_RATE_IDLE           5    // Decay when no gesture is detected.
#define DECAY_RATE_WRONG_SPELL    10    // Faster decay when the wrong spell is detected.

// Internal Math (Do not edit)
#define MAX_SCORE                 100 // Full charge score
#define SCORE_INCREMENT           (MAX_SCORE / FRAMES_TO_TRIGGER) // Score gained per successful AI frame
#define SCORE_TRIGGER_LEVEL       (MAX_SCORE - 5) // Trigger when nearly full

// ===========================================================================
// --- 3. TIMING PARAMETERS (milliseconds) ---
// ===========================================================================

// Delay between "Spell Recognized" and "IR Packet Sent"
// (Gives time for sound effect buildup or animation wind-up)
#define CAST_PRE_DELAY_MS         600  

// How long you must wait AFTER firing before firing again
#define CAST_COOLDOWN_MS          1500  

// How long to ignore the AI after a successful trigger 
// (Prevents the wand from machine-gunning the same spell)
#define AI_IGNORE_WINDOW_MS       2500  

// Time to stay dead before respawning
#define RESPAWN_DELAY_MS          7000

// ===========================================================================
// --- 4. HARDWARE CONFIGURATION ---
// ===========================================================================
// Unique IDs for IR communication
#define MY_PLAYER_ID              2
#define OPPONENT_PLAYER_ID        1

// I2C pins for BNO08x (IMU) and DRV2605 (Haptic)
#define I2C_PORT                  i2c0
#define I2C_SDA_PIN               16
#define I2C_SCL_PIN               17
// IR pins
#define TX_PIN                    36 // Example, actual pin varies by board
#define RX_PIN                    15
// Debug/Manual Trigger Buttons
#define BTN_A_PIN                 21
#define BTN_B_PIN                 26

// Queue Buffer Size (Max pending spells to fire)
#define QUEUE_SIZE                4    
// How many IR packets to send per cast (Redundancy for reliable comms)
#define PACKET_REPEATS            3

// Sensor Settings
#define SAMPLING_FREQ_HZ          74 // BNO08x poll rate
#define SENSOR_POLL_US            (1000000 / SAMPLING_FREQ_HZ)
#define INFERENCE_INTERVAL_MS     250 // Edge Impulse inference rate
#define EI_YPR_IN_DEGREES         1 // Output yaw, pitch, roll in degrees

// Motion Gating: Minimum movement (m/s^2 deviation from gravity) required to run AI
// Prevents running AI when the wand is stationary.
#define MOTION_THRESHOLD          3.0f 

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// ===========================================================================
// --- QUEUES & STRUCTS ---
// ===========================================================================
// Queue for passing AI results (spell decisions) from Core 1 to Core 0
static queue_t result_queue;
// Enum for haptic feedback commands
typedef enum { HAPTIC_NONE, HAPTIC_CAST, HAPTIC_HIT } haptic_cmd_t;
// Queue for passing haptic commands from Core 0 to Core 1
static queue_t haptic_queue;

// Circular buffer implementation for spells waiting to be transmitted (IR TX)
typedef struct {
    uint8_t buffer[QUEUE_SIZE];
    int head; // Index of the next spell to dequeue (send)
    int tail; // Index where the next spell will be enqueued
    int count; // Current number of spells in the queue
} SpellQueue;

static SpellQueue tx_queue = { .head = 0, .tail = 0, .count = 0 };

/**
 * @brief Adds a spell ID to the transmission queue.
 * @param spell_id The ID of the spell to enqueue.
 */
void enqueue_spell(uint8_t spell_id) {
    if (tx_queue.count >= QUEUE_SIZE) return; // Queue is full
    tx_queue.buffer[tx_queue.tail] = spell_id;
    tx_queue.tail = (tx_queue.tail + 1) % QUEUE_SIZE;
    tx_queue.count++;
    printf(">> Spell %d Queued (Firing in %d ms)...\n", spell_id, CAST_PRE_DELAY_MS);
}

/**
 * @brief Removes and returns the next spell ID from the transmission queue.
 * @return The spell ID, or 0 if the queue is empty.
 */
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

/**
 * @brief Core 1 main execution function. Handles sensors, haptics, and ML inference.
 */
void core1_entry() {
    // 1. I2C and Haptic Setup
    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_pull_up(I2C_SCL_PIN);

    drv2605_init(&haptic, I2C_PORT, I2C_SDA_PIN, I2C_SCL_PIN);
    drv2605_set_mode(&haptic, DRV2605_MODE_INTTRIG);
    drv2605_select_library(&haptic, 1);

    // 2. BNO08x IMU Setup
    if (!bno08x_begin_i2c(&bno08x, I2C_PORT, BNO08x_I2CADDR_DEFAULT, -1)) {
        // IMU failed to initialize, halt Core 1
        while(1) { sleep_ms(100); } 
    }
    // Enable the required sensor reports for the ML model
    bno08x_enable_report(&bno08x, SH2_ARVR_STABILIZED_RV, SENSOR_POLL_US); // Orientation (for YPR)
    bno08x_enable_report(&bno08x, SH2_ACCELEROMETER, SENSOR_POLL_US); // Acceleration

    // 3. Edge Impulse Data Buffer Setup
    size_t frame_size = ei_bridge_get_input_frame_size();
    int axes = ei_bridge_get_axis_count();
    float *features = (float*)calloc(frame_size, sizeof(float));
    
    // Sensor data structures
    sh2_SensorValue_t val;
    sh2_Accelerometer_t acc = {0};
    struct { float y, p, r; } ypr = {0}; // Yaw, Pitch, Roll
    
    int samples = 0;
    uint64_t last_poll_time = 0;
    uint64_t last_inf_time = 0;
    // Index where the next sample should be written (end of the buffer)
    int move_idx = frame_size - axes;

    // 4. Main Sensor Loop
    while(true) {
        uint64_t now = to_us_since_boot(get_absolute_time());

        // Haptic feedback processing
        haptic_cmd_t h_cmd;
        if (queue_try_remove(&haptic_queue, &h_cmd)) {
            if (h_cmd == HAPTIC_CAST) drv2605_play_cast_feedback(&haptic);
            if (h_cmd == HAPTIC_HIT)  drv2605_play_hit_feedback(&haptic);
        }

        // Sensor Polling
        if (now - last_poll_time >= SENSOR_POLL_US) {
            last_poll_time = now;
            // Read all available sensor events
            while (bno08x_get_sensor_event(&bno08x, &val)) {
                if (val.sensorId == SH2_ARVR_STABILIZED_RV) {
                    // Convert Rotation Vector Quaternion to Yaw, Pitch, Roll
                    float qr = val.un.arvrStabilizedRV.real;
                    float qi = val.un.arvrStabilizedRV.i;
                    float qj = val.un.arvrStabilizedRV.j;
                    float qk = val.un.arvrStabilizedRV.k;
                    float sqr = qr*qr, sqi = qi*qi, sqj = qj*qj, sqk = qk*qk;
                    // Yaw calculation
                    ypr.y = atan2f(2.0f * (qi * qj + qk * qr), (sqi - sqj - sqk + sqr));
                    // Pitch calculation
                    ypr.p = asinf(-2.0f * (qi * qk - qj * qr) / (sqi + sqj + sqk + sqr));
                    // Roll calculation
                    ypr.r = atan2f(2.0f * (qj * qk + qi * qr), (-sqi - sqj + sqk + sqr));
                    if (EI_YPR_IN_DEGREES) {
                        float r2d = 180.0f / M_PI;
                        ypr.y *= r2d; ypr.p *= r2d; ypr.r *= r2d;
                    }
                } else if (val.sensorId == SH2_ACCELEROMETER) {
                    acc = val.un.accelerometer;
                }
            }
            // Shift the existing data one sample window back
            memmove(features, features + axes, move_idx * sizeof(float));
            // Add the new sensor data to the end of the buffer
            features[move_idx+0] = acc.x; features[move_idx+1] = acc.y; features[move_idx+2] = acc.z;
            features[move_idx+3] = ypr.r; features[move_idx+4] = ypr.p; features[move_idx+5] = ypr.y;
            samples++;
        }

        // Motion Gating and Inference Logic
        float mag = sqrtf(acc.x*acc.x + acc.y*acc.y + acc.z*acc.z);
        // Check if the acceleration magnitude is significantly different from gravity (9.8m/s^2)
        if (fabsf(mag - 9.8f) > MOTION_THRESHOLD) {
            // Check if the buffer is full (ready for inference)
            if (samples >= (frame_size/axes)) {
                uint64_t now_ms = now / 1000;
                // Check inference interval
                if ((now_ms - last_inf_time) >= INFERENCE_INTERVAL_MS) {
                    last_inf_time = now_ms;
                    // Run the trained ML model
                    spell_decision_t res = ei_bridge_run_inference(features, frame_size);
                    // Send result to Core 0
                    if (!queue_try_add(&result_queue, &res)) {}
                }
            }
        }
    }
}

// ===========================================================================
// --- CORE 0: GAME LOGIC ---
// ===========================================================================

/**
 * @brief Requests haptic feedback to be played by Core 1.
 * @param type The haptic command (HAPTIC_CAST or HAPTIC_HIT).
 */
void request_haptic(haptic_cmd_t type) {
    queue_try_add(&haptic_queue, &type);
}

/**
 * @brief Processes the effects when the player is hit by an opponent's spell.
 * @param spell_num The ID of the incoming spell.
 */
void process_hit_effects(uint8_t spell_num) {
    int anim_id = 0;
    int damage = 0;
    switch(spell_num) {
        case SPELL_ID_1: printf("Hit: Aguamenti!\n"); anim_id = 3; damage = 3; break;
        case SPELL_ID_2: printf("Hit: Stupefy!\n"); anim_id = 0; damage = 5; break;
        default: return;
    }
    hb_update(-damage); // Reduce health
    // Lock out Core 1 to safely update the shared WS2812 resource
    ws2812_clear();
    multicore_lockout_start(); 
    controller(anim_id, 16, 16); // Play a hit animation (e.g., on the health bar LEDs)
    multicore_lockout_end();
    hb_draw(); // Redraw the updated health bar
}

/**
 * @brief Clamps the spell score between 0 and MAX_SCORE.
 * @param score The current score.
 * @return The clamped score.
 */
int clamp_score(int score) {
    if (score > MAX_SCORE) return MAX_SCORE;
    if (score < 0) return 0;
    return score;
}

int main() {
    // Initialisation
    stdio_init_all();
    sleep_ms(2000);
    printf("=== WAND DUEL: PARAMETERIZED ===\n");
    printf("Frames to Trigger: %d (Score/Frame: %d)\n", FRAMES_TO_TRIGGER, SCORE_INCREMENT);

    // Watchdog update has been removed to prevent random resets.
    // watchdog_enable(2000, 1);

    // Inter-core communication queues
    queue_init(&result_queue, sizeof(spell_decision_t), 4);
    queue_init(&haptic_queue, sizeof(haptic_cmd_t), 4);

    // Driver initialisation
    ir_emitter_init(TX_PIN);
    ir_receiver_init(RX_PIN);
    ws2812_init(); // Health bar LEDs
    hb_init(16, 16); // Initialise health bar (max health, current health)
    hb_draw(); // Draw initial health bar

    // Button setup
    gpio_init(BTN_A_PIN); gpio_set_dir(BTN_A_PIN, GPIO_IN); gpio_pull_down(BTN_A_PIN);
    gpio_init(BTN_B_PIN); gpio_set_dir(BTN_B_PIN, GPIO_IN); gpio_pull_down(BTN_B_PIN);

    // Launch Core 1
    multicore_launch_core1(core1_entry);

    // Game State Variables
    ir_decoded_data_t rx_data;
    uint32_t death_time = 0;
    bool is_dead = false;
    int score_s1 = 0, score_s2 = 0; // Spell charge scores
    
    uint32_t last_trigger_time = 0; // Last time a spell was triggered/cast
    uint32_t next_cast_allowed_time = 0; // Earliest time a spell can physically fire
    spell_decision_t ai_result;

    // Main Game Loop
    while (true) {
        // Watchdog update has been removed.
        // watchdog_update();

        uint32_t now = to_ms_since_boot(get_absolute_time());

        // 1. AI Processing (Reading results from Core 1)
        if (queue_try_remove(&result_queue, &ai_result)) {
            // If inside the "Ignore Window" after a cast, reset score and ignore the AI result
            if (now - last_trigger_time < AI_IGNORE_WINDOW_MS) {
                score_s1 = 0; score_s2 = 0;
            } else {
                if (ai_result.detected) {
                    // Successful detection
                    if (strcmp(ai_result.label, SPELL_LABEL_1) == 0) {
                        score_s1 += SCORE_INCREMENT; 
                        score_s2 -= DECAY_RATE_WRONG_SPELL; // Decay other spell's score
                    } else if (strcmp(ai_result.label, SPELL_LABEL_2) == 0) {
                        score_s2 += SCORE_INCREMENT; 
                        score_s1 -= DECAY_RATE_WRONG_SPELL; // Decay other spell's score
                    }
                } else {
                    // No spell detected (idle)
                    score_s1 -= DECAY_RATE_IDLE; 
                    score_s2 -= DECAY_RATE_IDLE;
                }
                // Clamp the scores
                score_s1 = clamp_score(score_s1); score_s2 = clamp_score(score_s2);

                // Trigger Logic
                if (score_s1 >= SCORE_TRIGGER_LEVEL || score_s2 >= SCORE_TRIGGER_LEVEL) {
                    // Determine which spell to fire
                    uint8_t spell = (score_s1 >= SCORE_TRIGGER_LEVEL) ? SPELL_ID_1 : SPELL_ID_2;
                    
                    // Enqueue the spell for a delayed fire
                    enqueue_spell(spell);
                    request_haptic(HAPTIC_CAST); 

                    // Calculate the earliest time the IR packet can be sent (pre-delay)
                    uint32_t earliest_fire_time = now + CAST_PRE_DELAY_MS;
                    // Ensure the pre-delay doesn't override an existing cooldown
                    if (next_cast_allowed_time < earliest_fire_time) {
                        next_cast_allowed_time = earliest_fire_time;
                    }

                    // Reset Score & Cooldown window
                    score_s1 = 0; score_s2 = 0;
                    last_trigger_time = now;
                }
            }
        }

        // 2. Respawn Logic
        if (is_dead) {
            if (now - death_time > RESPAWN_DELAY_MS) {
                is_dead = false;
                hb_reset(); // Restore full health
                hb_draw();
                printf(">>> Respawned\n");
            }
        } 
        // 3. Manual Buttons (Test/Alternative Input)
        else {
            uint8_t btn_spell = 0;
            if (gpio_get(BTN_A_PIN)) btn_spell = SPELL_ID_1;
            if (gpio_get(BTN_B_PIN)) btn_spell = SPELL_ID_2;
            
            if (btn_spell > 0) {
                // Manual triggers skip the AI score logic, but still respect pre-delay
                enqueue_spell(btn_spell);
                request_haptic(HAPTIC_CAST);
                
                uint32_t fire_time = now + CAST_PRE_DELAY_MS;
                if (next_cast_allowed_time < fire_time) {
                    next_cast_allowed_time = fire_time;
                }
                // Simple debounce/prevent spam
                sleep_ms(200);
            }
        }

        // 4. Firing Logic (IR Transmission)
        if (!is_dead && tx_queue.count > 0) {
            if (now >= next_cast_allowed_time) {
                uint8_t spell_to_cast = dequeue_spell();
                if (spell_to_cast > 0) {
                    // Transmit the IR packet
                    ir_emitter_start(MY_PLAYER_ID, spell_to_cast, PACKET_REPEATS);
                    printf(">>> FIRING SPELL %d (t=%lu)\n", spell_to_cast, now);
                    
                    // Apply Cooldown AFTER firing
                    next_cast_allowed_time = now + CAST_COOLDOWN_MS;
                }
            }
        }

        // 5. Receiver Logic (IR Reception)
        if (ir_receiver_decode(&rx_data)) {
            // Check if the IR packet is for this player (address = OPPONENT_PLAYER_ID)
            if (rx_data.protocol == IR_PROTOCOL_NEC && rx_data.address == OPPONENT_PLAYER_ID) {
                request_haptic(HAPTIC_HIT);
                process_hit_effects(rx_data.command);
                // Check for death condition
                if (hb_current() <= 0) {
                    is_dead = true;
                    death_time = now;
                    // Play a death animation
                    loser_screen(16, 16);
                }
            }
        }

        // Update the IR transmitter state machine
        ir_emitter_update();
        // Sleep briefly to save power and yield to other processes
        sleep_ms(1);
    }
}