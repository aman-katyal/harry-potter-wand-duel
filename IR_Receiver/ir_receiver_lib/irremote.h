/*
 * irremote.h
 *
 * Core public API for the Pico C IR Receiver Library.
 *
 * This file is part of a C conversion of the Arduino-IRremote library,
 * stripped for NEC-only reception on the Raspberry Pi Pico/RP2350.
 */

#ifndef IRREMOTE_H
#define IRREMOTE_H

#include <stdint.h>
#include <stdbool.h>

/*
 * Supported subset of IR protocols.
 */
typedef enum {
    IR_PROTOCOL_UNKNOWN = 0,
    IR_PROTOCOL_NEC
} ir_protocol_t;

/*
 * Definitions for the flags field in ir_decoded_data_t
 */
#define IRDATA_FLAGS_IS_REPEAT      0x01
#define IRDATA_FLAGS_WAS_OVERFLOW   0x40

/*
 * The main data structure to hold the decoded IR result.
 * This is the C equivalent of IrReceiver.decodedIRData.
 */
typedef struct {
    ir_protocol_t protocol;
    uint16_t address;
    uint16_t command;
    uint8_t flags;
} ir_decoded_data_t;


/**
 * @brief Initializes the IR receiver.
 * Configures the specified GPIO pin for input and enables the IRQ handler.
 *
 * @param receive_pin The GPIO pin number to use for receiving IR signals.
 */
void ir_receiver_init(uint8_t receive_pin);

/**
 * @brief Attempts to decode the most recently received IR frame.
 * This function should be called after an IR frame has been received
 * (e.g., in the main loop, checking ir_receiver_is_idle()).
 *
 * @param decoded_data A pointer to a struct that will be filled with the
 * decoded data (protocol, address, command).
 * @return true if a complete frame was in the buffer and an attempt to
 * decode was made, false if the receiver is not in the STOP state.
 */
bool ir_receiver_decode(ir_decoded_data_t *decoded_data);

/**
 * @brief Resets the receiver state machine to be ready for the next frame.
 * This must be called after processing a decoded frame.
 */
void ir_receiver_resume(void);

/**
 * @brief Checks if the receiver state machine is idle or stopped.
 *
 * @return true if the receiver is not actively recording a frame.
 * (State is IDLE or STOP).
 */
bool ir_receiver_is_idle(void);

#endif // IRREMOTE_H