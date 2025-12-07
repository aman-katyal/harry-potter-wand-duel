#ifndef IR_EMITTER_H
#define IR_EMITTER_H

#include <stdint.h>
#include <stdbool.h>

// === NEC Timing (microseconds) ===
#define NEC_UNIT       560
#define NEC_HDR_MARK   (16 * NEC_UNIT)
#define NEC_HDR_SPACE  (8  * NEC_UNIT)
#define NEC_BIT_MARK   NEC_UNIT
#define NEC_ONE_SPACE  (3 * NEC_UNIT)
#define NEC_ZERO_SPACE NEC_UNIT
#define NEC_STOP_BIT   NEC_UNIT
#define NEC_GAP_MS     110

/**
 * Initialize the IR emitter on the specified GPIO pin
 * @param tx_pin GPIO pin number for IR LED (must support PWM)
 */
void ir_emitter_init(uint32_t tx_pin);

/**
 * Start sending a packet with specified repeats
 * @param addr NEC address value
 * @param cmd NEC command value
 * @param repeats Number of times to send this packet
 */
void ir_emitter_start(uint8_t addr, uint8_t cmd, uint8_t repeats);

/**
 * Update function - call this repeatedly from main loop
 * Handles all timing and packet transmission automatically
 */
void ir_emitter_update(void);

/**
 * Check if sequence is complete
 * @return true if all packets sent, false if still transmitting
 */
bool ir_emitter_done(void);

#endif // IR_EMITTER_H