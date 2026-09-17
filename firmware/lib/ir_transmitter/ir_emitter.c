#include "ir_emitter.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include <stdio.h>


typedef enum {
    //define fsm vars/each phase of NEC frame transmission
    STATE_IDLE,
    STATE_HDR_MARK, //send header mark
    STATE_HDR_SPACE, //space after
    STATE_BIT_MARK, //mark @ start of each bit
    STATE_BIT_SPACE, //space? either 0 or 1
    STATE_STOP_BIT, //final mark after all bits transmitted
    STATE_DONE
} tx_state_t;

typedef struct {
    //define packet/frame sequence to send multiple/repeat frames at a time
    uint8_t addr;
    uint8_t cmd;
    uint8_t repeats; //# of times to send one frame
    uint8_t current_repeat; //keep track of how many frames have alr been sent
    bool active; //sequence curr running?
    uint32_t last_send_time;
} sequence_state_t;

static uint get_pwm_slice; //get slice from pin#
static volatile tx_state_t state = STATE_IDLE; //set initial state
static volatile uint32_t frame = 0; //32b NEC frame
static volatile int bit_index = 0; //index of curr bit: ISR uses to shift bits out of frame!
static volatile bool is_tx_busy = false; //is a frame being sent rn?
static sequence_state_t sequence = {0}; //init all struct vars or seq = 0

//1. build frame for NEC encoding
//NEC format = byte0 (addr) byte1 (~addr) byte2 (cmd) byte3 (~cmd)
static uint32_t nec_build(uint8_t addr, uint8_t cmd) {
    uint32_t data = 0;
    data  = addr;
    data |= ((uint32_t)(~addr) & 0xFF) << 8; //shift 8bits into byte 1
    data |= ((uint32_t)cmd & 0xFF) << 16;
    data |= ((uint32_t)(~cmd) & 0xFF) << 24;
    return data;
}

//2. start NEC frame transmit
static bool ir_emitter_send(uint8_t addr, uint8_t cmd) {
    if(is_tx_busy) {
        return false;
        //DO NOT start new frame transmit if another frame curr being sent
    }
    
    frame = nec_build(addr, cmd); 
    is_tx_busy = true;
    state = STATE_HDR_MARK; //set to first state of NEC transmission
    bit_index = 0;
    
    //en PWM on slice
    pwm_set_enabled(get_pwm_slice, true);
    //call timer isr when alarm 0 reaches the correct nec mark
    timer_hw->alarm[0] = timer_hw->timerawl + NEC_HDR_MARK;
    
    return true;
}

//3. emitter ready when no frame in progress; done when no seq runing
static bool ir_emitter_ready(void) {
    return !is_tx_busy;
}
bool ir_emitter_done(void) 
{
    return !sequence.active;
}

//4. ISR
static void timer_isr(void) {
    hw_clear_bits(&timer_hw->intr, 1u << 0); //clear int on alarm0
    uint32_t next_delay = 0; //delay between alarms/trigger next emit
    
    switch(state) {
        //if header mark fin: turn pwm off + move to start header space
        case STATE_HDR_MARK:
            pwm_set_enabled(get_pwm_slice, false);
            state = STATE_HDR_SPACE;
            next_delay = NEC_HDR_SPACE;
            break;
        //if header space fin: turn pwm on + move to next state
        case STATE_HDR_SPACE:
            pwm_set_enabled(get_pwm_slice, true);
            state = STATE_BIT_MARK;
            bit_index = 0;
            next_delay = NEC_BIT_MARK;
            break;
        case STATE_BIT_MARK:
            pwm_set_enabled(get_pwm_slice, false);
            state = STATE_BIT_SPACE;
            //det delay by checking curr bit
            if(frame & (1u << bit_index))
                next_delay = NEC_ONE_SPACE; //1
            else
                next_delay = NEC_ZERO_SPACE; //0
            break;
        //incr bit to prep for next one 
        case STATE_BIT_SPACE:
            bit_index++;
            //if haven't fin all bits, turn pwm ON + start next bit's mark
            if(bit_index<32) 
            {
                pwm_set_enabled(get_pwm_slice, true);
                state = STATE_BIT_MARK;
                next_delay = NEC_BIT_MARK;
            } 
            //fin all bits in frame
            else 
            {
                pwm_set_enabled(get_pwm_slice, true);
                state = STATE_STOP_BIT; //last state = stop
                next_delay = NEC_STOP_BIT;
            }
            break;
        //final state: pwm off + frame transmit done  
        case STATE_STOP_BIT:
            pwm_set_enabled(get_pwm_slice, false);
            state = STATE_DONE;
            is_tx_busy = false;
            return;
        default: //assume ready
            is_tx_busy = false;
            return;
    }
    
    timer_hw->alarm[0] = timer_hw->timerawl + next_delay;
}

//5. init emitter for pin given
void ir_emitter_init(uint32_t tx_pin) {
    gpio_set_function(tx_pin, GPIO_FUNC_PWM);
    //pwm inits
    get_pwm_slice = pwm_gpio_to_slice_num(tx_pin);
    float freq = 125000000.0f/(38000.0f*64.0f);
    pwm_set_clkdiv(get_pwm_slice, freq); //set freq to 38 kHz!!!
    pwm_set_wrap(get_pwm_slice, 63);
    pwm_set_chan_level(get_pwm_slice, PWM_CHAN_A, 21);
    pwm_set_enabled(get_pwm_slice, false);
    //timer 
    hw_set_bits(&timer_hw->inte, 1u << 0); //en int for alarm0
    irq_set_exclusive_handler(TIMER0_IRQ_0, timer_isr);
    irq_set_enabled(TIMER0_IRQ_0, true);
}

//6. start seq by init struct (called when wand recognizes a spell)
void ir_emitter_start(uint8_t addr, uint8_t cmd, uint8_t repeats) {
    sequence.addr = addr;
    sequence.cmd = cmd;
    sequence.repeats = repeats;
    sequence.current_repeat = 0;
    sequence.active = true;
    sequence.last_send_time = 0;
}

//7. send mult frames w/delay
void ir_emitter_update(void) {
    if(!sequence.active) 
    {
        return;
    }
    //end seq transmit if curr repeat exceeds the # specified
    if(sequence.current_repeat >= sequence.repeats) 
    {
        sequence.active = false;
        return;
    }
    
    if(ir_emitter_ready() && (time_us_32() - sequence.last_send_time) >= (NEC_GAP_MS * 1000)) 
    {
        //for testing - can del later!!!!!!!!!
        printf("Sending: addr=%d, cmd=%d (repeat %d/%d)\n", sequence.addr, sequence.cmd, sequence.current_repeat + 1, sequence.repeats);
        //send repeated frames when ready
        if(ir_emitter_send(sequence.addr, sequence.cmd)) 
        {
            sequence.last_send_time = time_us_32();
            sequence.current_repeat++;
        }
    }
}

