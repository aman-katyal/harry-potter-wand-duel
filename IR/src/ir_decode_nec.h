/*
 * ir_decode_nec.h
 *
 * Public header for the NEC protocol decoder.
 *
 * This file is part of a C conversion of the Arduino-IRremote library.
 */

#ifndef IR_DECODE_NEC_H
#define IR_DECODE_NEC_H

#include "irremote.h"
#include "ir_rp_hal.h" // Needs ir_params_t

/**
 * @brief Decodes the raw timing data buffer as an NEC protocol frame.
 *
 * @param decoded_data Pointer to the output struct to be filled.
 * @param ir_params    Pointer to the captured IR data and state.
 * @return true if the frame was successfully decoded as NEC, false otherwise.
 */
bool decode_nec_protocol(ir_decoded_data_t *decoded_data, ir_params_t *ir_params);


#endif // IR_DECODE_NEC_H