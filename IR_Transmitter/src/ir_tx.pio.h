// -----------------------------------------------------------------------------
// ir_tx.pio.h — manually created PIO header for RP2350 NEC IR Transmitter
// Matches the PIO assembly from ir_tx.pio
// -----------------------------------------------------------------------------

#ifndef _IR_TX_PIO_H_
#define _IR_TX_PIO_H_

#include "hardware/pio.h"

// -----------------------------------------------------------------------------
// Program: ir_tx
// Equivalent to .pio file:
//
// .program ir_tx
// .side_set 1 opt
// pull block
// out x, 16       ; mark_us
// out y, 16       ; space_us
// mark_loop:
//     set pins, 1 [1]
//     nop [6]
//     set pins, 0 [1]
//     nop [6]
//     jmp x-- mark_loop
// space_loop:
//     set pins, 0
//     jmp y-- space_loop
// jmp 0
// -----------------------------------------------------------------------------

// Each instruction is a 16-bit encoded PIO instruction
static const uint16_t ir_tx_program_instructions[] = {
    0x80a0, //  0: pull block
    0xa027, //  1: out x, 16
    0xa047, //  2: out y, 16
    0xe081, //  3: set pins, 1 [1]
    0x0006, //  4: nop [6]
    0xe001, //  5: set pins, 0 [1]
    0x0006, //  6: nop [6]
    0x0045, //  7: jmp x-- mark_loop
    0xe000, //  8: set pins, 0
    0x0049, //  9: jmp y-- space_loop
    0x0000  // 10: jmp 0
};

// Program struct for use with pio_add_program()
static const struct pio_program ir_tx_program = {
    .instructions = ir_tx_program_instructions,
    .length = sizeof(ir_tx_program_instructions) / sizeof(ir_tx_program_instructions[0]),
    .origin = -1
};

// Helper: return default config for state machine setup
static inline pio_sm_config ir_tx_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + 0, offset + 10);
    sm_config_set_sideset(&c, 1, true, false);
    return c;
}

#endif  // _IR_TX_PIO_H_
