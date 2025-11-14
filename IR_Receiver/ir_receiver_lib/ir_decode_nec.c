/*
 * ir_decode_nec.c
 *
 * C implementation of the NEC IR protocol decoder.
 *
 * This file is part of a C conversion of the Arduino-IRremote library.
 */

#include "ir_decode_nec.h"

// --- NEC Protocol Timings (microseconds) ---
#define NEC_HEADER_MARK         9000
#define NEC_HEADER_SPACE        4500
#define NEC_BIT_MARK             560
#define NEC_ONE_SPACE           1690
#define NEC_ZERO_SPACE           560
#define NEC_REPEAT_HEADER_SPACE 2250

#define NEC_BITS 32

// --- Static helper function ---
static bool decode_pulse_distance_width(ir_params_t *ir_params,
                                        uint16_t *aStartOffset,
                                        uint8_t aNumberOfBits,
                                        uint32_t aOneMark, uint32_t aOneSpace,
                                        uint32_t aZeroMark, uint32_t aZeroSpace,
                                        bool aMSBfirst,
                                        uint32_t *aDecodedRawData);

// --- Static variables for repeat handling ---
static uint16_t s_last_decoded_address = 0;
static uint16_t s_last_decoded_command = 0;


/**
 * @brief Decodes the raw timing data buffer as an NEC protocol frame.
 */
bool decode_nec_protocol(ir_decoded_data_t *decoded_data, ir_params_t *ir_params) {
    
    // The ISR stores:
    // [0] = Header Mark
    // [1] = Header Space
    // [2] = Bit 0 Mark
    // [3] = Bit 0 Space
    // ...
    // [65] = Bit 31 Space
    // [66] = Stop Bit Mark
    // Total pulses for a full frame: 1 + 1 + (32 * 2) + 1 = 67
    uint16_t raw_len = ir_params->raw_len;

    // A repeat frame is:
    // [0] = Header Mark
    // [1] = Repeat Space
    // [2] = Stop Bit Mark
    // Total pulses for a repeat frame: 3
    
    if (raw_len != 67 && raw_len != 3) {
        return false; // Not an NEC frame length
    }

    // --- Check Header Mark ---
    if (!ir_match_mark(ir_params->raw_buf[0], NEC_HEADER_MARK)) {
        return false;
    }

    // --- Check for Repeat Frame ---
    if (raw_len == 3) {
        if (ir_match_space(ir_params->raw_buf[1], NEC_REPEAT_HEADER_SPACE) &&
            ir_match_mark(ir_params->raw_buf[2], NEC_BIT_MARK)) {
            
            decoded_data->protocol = IR_PROTOCOL_NEC;
            decoded_data->flags = IRDATA_FLAGS_IS_REPEAT;
            decoded_data->address = s_last_decoded_address;
            decoded_data->command = s_last_decoded_command;
            return true;
        }
        return false; // Failed repeat check
    }

    // --- Check Header Space (Full Frame) ---
    if (!ir_match_space(ir_params->raw_buf[1], NEC_HEADER_SPACE)) {
        return false;
    }

    // --- Decode 32 bits of data ---
    uint16_t offset = 2; // Start decoding from index 2
    uint32_t decoded_data_raw = 0;

    if (!decode_pulse_distance_width(ir_params, &offset, NEC_BITS,
                                    NEC_BIT_MARK, NEC_ONE_SPACE,
                                    NEC_BIT_MARK, NEC_ZERO_SPACE,
                                    false, // NEC is LSB first
                                    &decoded_data_raw)) {
        return false;
    }

    // --- Check Stop Bit ---
    // The offset should now be at the stop bit (index 2 + 32*2 = 66)
    if (offset != 66 || !ir_match_mark(ir_params->raw_buf[offset], NEC_BIT_MARK)) {
        return false;
    }

    // --- Success: Decode Address and Command ---
    decoded_data->protocol = IR_PROTOCOL_NEC;

    // NEC protocol is LSB first.
    // Raw data format: [A_LSB] [A_MSB] [C_LSB] [C_MSB]
    // A_MSB is the inverted LSB, C_MSB is the inverted LSB.
    
    uint8_t address_lsb = (decoded_data_raw >> 0) & 0xFF;
    uint8_t address_msb = (decoded_data_raw >> 8) & 0xFF;
    uint8_t command_lsb = (decoded_data_raw >> 16) & 0xFF;
    uint8_t command_msb = (decoded_data_raw >> 24) & 0xFF;

    // Standard NEC (8-bit address)
    if (address_lsb == (uint8_t)(~address_msb)) {
        decoded_data->address = address_lsb;
    } else {
        // Extended NEC (16-bit address)
        decoded_data->address = (address_msb << 8) | address_lsb;
    }

    // Standard NEC (8-bit command)
    if (command_lsb == (uint8_t)(~command_msb)) {
        decoded_data->command = command_lsb;
    } else {
        // This might be a non-standard NEC, but we'll just store
        // the 8-bit command. For a 16-bit command, you'd use:
        // decoded_data->command = (command_msb << 8) | command_lsb;
        decoded_data->command = command_lsb; 
    }

    // Save for repeat handling
    s_last_decoded_address = decoded_data->address;
    s_last_decoded_command = decoded_data->command;

    return true;
}


/**
 * @brief Generic pulse-distance-width decoder.
 * Ported from IRrecv::decodePulseDistanceWidthData
 *
 * @param aStartOffset    Pointer to the starting index in raw_buf. Will be incremented.
 * @param aMSBfirst       Set true for MSB-first, false for LSB-first.
 * @param aDecodedRawData Pointer to the 32-bit uint to store the result.
 * @return true on success, false on a timing mismatch.
 */
static bool decode_pulse_distance_width(ir_params_t *ir_params,
                                        uint16_t *aStartOffset,
                                        uint8_t aNumberOfBits,
                                        uint32_t aOneMark, uint32_t aOneSpace,
                                        uint32_t aZeroMark, uint32_t aZeroSpace,
                                        bool aMSBfirst,
                                        uint32_t *aDecodedRawData) {
    
    uint32_t tDecodedData = 0;
    uint32_t tMask = 1UL;

    for (uint_fast8_t i = 0; i < aNumberOfBits; i++) {
        // Get the mark and space for this bit
        uint32_t mark_us = ir_params->raw_buf[(*aStartOffset)++];
        uint32_t space_us = ir_params->raw_buf[(*aStartOffset)++];

        bool bit_value;

        // --- Check for a '1' bit ---
        if (ir_match_mark(mark_us, aOneMark) && ir_match_space(space_us, aOneSpace)) {
            bit_value = true;
        }
        // --- Check for a '0' bit ---
        else if (ir_match_mark(mark_us, aZeroMark) && ir_match_space(space_us, aZeroSpace)) {
            bit_value = false;
        }
        // --- No match ---
        else {
            return false;
        }

        // --- Store the bit ---
        if (aMSBfirst) {
            tDecodedData <<= 1;
            if (bit_value) {
                tDecodedData |= 1;
            }
        } else { // LSB first
            if (bit_value) {
                tDecodedData |= tMask;
            }
            tMask <<= 1;
        }
    }

    *aDecodedRawData = tDecodedData;
    return true;
}