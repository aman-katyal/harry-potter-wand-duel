#ifndef IR_EMITTER_H
#define IR_EMITTER_H

#include <stdint.h>
#include <stdbool.h>

//def constants for fsm:
#define NEC_UNIT 560
#define NEC_HDR_MARK (16 * NEC_UNIT)
#define NEC_HDR_SPACE (8  * NEC_UNIT)
#define NEC_BIT_MARK NEC_UNIT
#define NEC_ONE_SPACE (3 * NEC_UNIT)
#define NEC_ZERO_SPACE NEC_UNIT
#define NEC_STOP_BIT   NEC_UNIT
#define NEC_GAP_MS 110

//1. Initialize the IR emitter on the specified GPIO pin
    //tx_pin  = GPIO pin # for IR LED
void ir_emitter_init(uint32_t tx_pin);


//2. Start sending a packet with specified repeats
    //addr = NEC addr val
    //cmd = NEC cmd val
    //repeats = # times to send data packet
void ir_emitter_start(uint8_t addr, uint8_t cmd, uint8_t repeats);


//3. Update function to send mult frames based on given params
void ir_emitter_update(void);

/**
 * Check if sequence is complete
 * @return true if all packets sent, false if still transmitting
 */
bool ir_emitter_done(void);

#endif