// -- C-language wrapper for a .pio program --
// (Manually generated)

#ifndef _PIO_IR_RECEIVER_H
#define _PIO_IR_RECEIVER_H

#include "hardware/pio.h"

#define ir_receiver_wrap_target 0
#define ir_receiver_wrap 18

#define ir_receiver_GAP_TIMEOUT 8000

static const uint16_t ir_receiver_program_instructions[] = {
    //     .wrap_target
    0x20a0, //  0: wait    0 pin, 0         
    0xa027, //  1: mov     osr, null        
    0xe0ff, //  2: set     x, 31            
    0x0083, //  3: jmp     pin, 3           
    0x0043, //  4: jmp     x--, 3           
    0xc020, //  5: irq     0                
    0x0000, //  6: jmp     0                
    0xa0c7, //  7: mov     isr, osr         
    0x0061, //  8: sub     isr, x           
    0x8000, //  9: push    noblock          
    0xe0ff, // 10: set     x, 31            
    0x000c, // 11: jmp     !pin, 12         
    0x004b, // 12: jmp     x--, 11          
    0xc020, // 13: irq     0                
    0x0000, // 14: jmp     0                
    0xa0c7, // 15: mov     isr, osr         
    0x0061, // 16: sub     isr, x           
    0x8000, // 17: push    noblock          
    0x0003, // 18: jmp     3                
    //     .wrap
};

#if !PICO_NO_HARDWARE
static const struct pio_program ir_receiver_program = {
    .instructions = ir_receiver_program_instructions,
    .length = 19,
    .origin = -1,
};

static inline pio_sm_config ir_receiver_program_get_default_config(uint offset) {
    pio_sm_config c = pio_get_default_sm_config();
    sm_config_set_wrap(&c, offset + ir_receiver_wrap_target, offset + ir_receiver_wrap);
    return c;
}
#endif

#endif