// src/sh2_hal.h

#ifndef SH2_HAL_H
#define SH2_HAL_H

#include <stdint.h>
#include <stdbool.h>

// --- Added Definitions ---
#define SH2_HAL_MAX_TRANSFER (256)  // Max SPI transfer size
#define SH2_NUM_CHANNELS (16)       // Number of SHTP channels

// Initialize the hardware (SPI, GPIOs)
int sh2_hal_init(void);

// Perform a hardware reset of the BNO08x
void sh2_hal_reset(void);

// Read/write data over the SPI bus
int sh2_hal_transfer(const uint8_t *pSend, uint8_t *pRecv, uint32_t len);

// Read the state of the INT pin
bool sh2_hal_read_interrupt(void);

// Get a timestamp in microseconds
uint64_t sh2_hal_get_timestamp_us(void);

#endif // SH2_HAL_H