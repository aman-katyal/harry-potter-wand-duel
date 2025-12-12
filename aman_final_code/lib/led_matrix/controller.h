#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>

// Initialize the LED strip (call once in setup)
void controller_init(void);

// Start an animation (Non-blocking)
// This sets the internal state but returns immediately.
void controller_start_spell(uint8_t spell_num);

// Advance the animation by one frame (Call this in your main loop)
void controller_update(void);

#endif