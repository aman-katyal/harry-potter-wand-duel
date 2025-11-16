#include "ir_emitter.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include <stdio.h>

// === State machine for transmission ===
typedef enum {
    STATE_IDLE,
    STATE_HDR_MARK,
    STATE_HDR_SPACE,
    STATE_BIT_MARK,
    STATE_BIT_SPACE,
    STATE_STOP_BIT,
    STATE_DONE
} tx_state_t;

// === Sequence state ===
typedef struct {
    uint8_t addr;
    uint8_t cmd;
    uint8_t repeats;
    uint8_t current_repeat;
    bool active;
    uint32_t last_send_time;
} sequence_state_t;

// === Private global variables ===
static uint g_pwm_slice;
static volatile tx_state_t g_state = STATE_IDLE;
static volatile uint32_t g_frame = 0;
static volatile int g_bit_index = 0;
static volatile bool g_tx_busy = false;
static sequence_state_t g_sequence = {0};

// === Private helper functions ===
static inline void pwm_on(uint slice)  { pwm_set_enabled(slice, true); }
static inline void pwm_off(uint slice) { pwm_set_enabled(slice, false); }

static uint32_t nec_build(uint8_t addr, uint8_t cmd) {
    uint32_t data = 0;
    data  = addr;
    data |= ((uint32_t)(~addr) & 0xFF) << 8;
    data |= ((uint32_t)cmd & 0xFF) << 16;
    data |= ((uint32_t)(~cmd) & 0xFF) << 24;
    return data;
}

static bool ir_emitter_send(uint8_t addr, uint8_t cmd) {
    if(g_tx_busy) {
        return false;
    }
    
    g_frame = nec_build(addr, cmd);
    g_tx_busy = true;
    g_state = STATE_HDR_MARK;
    g_bit_index = 0;
    
    pwm_on(g_pwm_slice);
    timer_hw->alarm[0] = timer_hw->timerawl + NEC_HDR_MARK;
    
    return true;
}

static bool ir_emitter_ready(void) {
    return !g_tx_busy;
}

// === Timer ISR ===
static void timer_isr(void) {
    hw_clear_bits(&timer_hw->intr, 1u << 0);
    
    uint32_t next_delay = 0;
    
    switch(g_state) {
        case STATE_HDR_MARK:
            pwm_off(g_pwm_slice);
            g_state = STATE_HDR_SPACE;
            next_delay = NEC_HDR_SPACE;
            break;
            
        case STATE_HDR_SPACE:
            pwm_on(g_pwm_slice);
            g_state = STATE_BIT_MARK;
            g_bit_index = 0;
            next_delay = NEC_BIT_MARK;
            break;
            
        case STATE_BIT_MARK:
            pwm_off(g_pwm_slice);
            g_state = STATE_BIT_SPACE;
            if (g_frame & (1u << g_bit_index))
                next_delay = NEC_ONE_SPACE;
            else
                next_delay = NEC_ZERO_SPACE;
            break;
            
        case STATE_BIT_SPACE:
            g_bit_index++;
            if (g_bit_index < 32) {
                pwm_on(g_pwm_slice);
                g_state = STATE_BIT_MARK;
                next_delay = NEC_BIT_MARK;
            } else {
                pwm_on(g_pwm_slice);
                g_state = STATE_STOP_BIT;
                next_delay = NEC_STOP_BIT;
            }
            break;
            
        case STATE_STOP_BIT:
            pwm_off(g_pwm_slice);
            g_state = STATE_DONE;
            g_tx_busy = false;
            return;
            
        default:
            g_tx_busy = false;
            return;
    }
    
    timer_hw->alarm[0] = timer_hw->timerawl + next_delay;
}

// === Public API implementation ===
void ir_emitter_init(uint32_t tx_pin) {
    gpio_set_function(tx_pin, GPIO_FUNC_PWM);
    g_pwm_slice = pwm_gpio_to_slice_num(tx_pin);
    float div = 125000000.0f / (38000.0f * 64.0f);
    pwm_set_clkdiv(g_pwm_slice, div);
    pwm_set_wrap(g_pwm_slice, 63);
    pwm_set_chan_level(g_pwm_slice, PWM_CHAN_A, 21);
    pwm_set_enabled(g_pwm_slice, false);
    
    hw_set_bits(&timer_hw->inte, 1u << 0);
    irq_set_exclusive_handler(TIMER0_IRQ_0, timer_isr);
    irq_set_enabled(TIMER0_IRQ_0, true);
}

void ir_emitter_start(uint8_t addr, uint8_t cmd, uint8_t repeats) {
    g_sequence.addr = addr;
    g_sequence.cmd = cmd;
    g_sequence.repeats = repeats;
    g_sequence.current_repeat = 0;
    g_sequence.active = true;
    g_sequence.last_send_time = 0;
}

void ir_emitter_update(void) {
    if (!g_sequence.active) {
        return;
    }
    
    if (g_sequence.current_repeat >= g_sequence.repeats) {
        g_sequence.active = false;
        return;
    }
    
    if (ir_emitter_ready() && 
        (time_us_32() - g_sequence.last_send_time) >= (NEC_GAP_MS * 1000)) {
        
        printf("Sending: addr=%d, cmd=%d (repeat %d/%d)\n", 
               g_sequence.addr, 
               g_sequence.cmd,
               g_sequence.current_repeat + 1,
               g_sequence.repeats);
        
        if(ir_emitter_send(g_sequence.addr, g_sequence.cmd)) {
            g_sequence.last_send_time = time_us_32();
            g_sequence.current_repeat++;
        }
    }
}

bool ir_emitter_done(void) {
    return !g_sequence.active;
}