/**
 * @file adafruit_busio_register.h
 * @brief C-based abstraction for I2C registers for the RP2350B.
 *
 * This is a C translation of the Adafruit_BusIO_Register and
 * Adafruit_BusIO_RegisterBits classes, designed for an I2C-only workflow.
 */

#ifndef _ADAFRUIT_BUSIO_REGISTER_H
#define _ADAFRUIT_BUSIO_REGISTER_H

#include "adafruit_i2cdev.h"

// Define byte order constants for clarity
#define BUSIO_LSBFIRST 0
#define BUSIO_MSBFIRST 1

/**
 * @brief Structure representing a single register on an I2C device.
 */
typedef struct {
    adafruit_i2cdev_t *device;      ///< The I2C device this register belongs to.
    uint16_t          reg_addr;     ///< The 8- or 16-bit address of the register.
    uint8_t           width;        ///< The width of the register in bytes (1-4).
    uint8_t           addr_width;   ///< The width of the register address in bytes (1 or 2).
    uint8_t           byteorder;    ///< The byte order of the data (BUSIO_LSBFIRST or BUSIO_MSBFIRST).
} adafruit_busio_register_t;

/**
 * @brief Structure representing a slice of bits within a register.
 */
typedef struct {
    adafruit_busio_register_t *reg; ///< The parent register.
    uint8_t                   bits; ///< The number of bits in the slice.
    uint8_t                   shift;///< The bit position (from LSB) of the slice.
} adafruit_busio_register_bits_t;


// --- Register Functions ---

/**
 * @brief Initializes a register object.
 *
 * @param reg Pointer to the register structure to initialize.
 * @param device The parent I2C device.
 * @param reg_address The memory address of the register.
 * @param width The width of the register in bytes (e.g., 1 for a uint8_t register).
 * @param address_width The width of the register address itself (usually 1).
 * @param byteorder The byte order of multi-byte registers (BUSIO_LSBFIRST or BUSIO_MSBFIRST).
 */
void adafruit_busio_register_init(adafruit_busio_register_t *reg, adafruit_i2cdev_t *device,
                                  uint16_t reg_address, uint8_t width, uint8_t address_width, uint8_t byteorder);

/**
 * @brief Writes a buffer of bytes to the register.
 *
 * @param reg Pointer to the initialized register.
 * @param buffer The data buffer to write.
 * @param len The number of bytes to write.
 * @return true on success, false on failure.
 */
bool adafruit_busio_register_write_buf(adafruit_busio_register_t *reg, uint8_t *buffer, uint8_t len);

/**
 * @brief Writes a value (up to 32 bits) to the register.
 *
 * @param reg Pointer to the initialized register.
 * @param value The value to write.
 * @param num_bytes The number of bytes to write from the value (defaults to register width if 0).
 * @return true on success, false on failure.
 */
bool adafruit_busio_register_write(adafruit_busio_register_t *reg, uint32_t value, uint8_t num_bytes);

/**
 * @brief Reads a buffer of bytes from the register.
 *
 * @param reg Pointer to the initialized register.
 * @param buffer The buffer to store the read data.
 * @param len The number of bytes to read.
 * @return true on success, false on failure.
 */
bool adafruit_busio_register_read_buf(adafruit_busio_register_t *reg, uint8_t *buffer, uint8_t len);

/**
 * @brief Reads the register and returns its value as a 32-bit integer.
 *
 * @param reg Pointer to the initialized register.
 * @param result Pointer to a uint32_t to store the result.
 * @return true on success, false on failure.
 */
bool adafruit_busio_register_read(adafruit_busio_register_t *reg, uint32_t *result);

// --- Register Bits Functions ---

/**
 * @brief Initializes a register bits object.
 *
 * @param reg_bits Pointer to the register bits structure to initialize.
 * @param reg The parent register object.
 * @param num_bits The number of bits in the slice.
 * @param shift The bit shift from LSB.
 */
void adafruit_busio_register_bits_init(adafruit_busio_register_bits_t *reg_bits,
                                       adafruit_busio_register_t *reg, uint8_t num_bits, uint8_t shift);

/**
 * @brief Reads the value from the bit slice.
 *
 * @param reg_bits Pointer to the initialized register bits object.
 * @param result Pointer to a uint32_t to store the result.
 * @return true on success, false on failure.
 */
bool adafruit_busio_register_bits_read(adafruit_busio_register_bits_t *reg_bits, uint32_t *result);

/**
 * @brief Writes a value to the bit slice, leaving other bits in the register untouched.
 *
 * @param reg_bits Pointer to the initialized register bits object.
 * @param value The value to write to the bit slice.
 * @return true on success, false on failure.
 */
bool adafruit_busio_register_bits_write(adafruit_busio_register_bits_t *reg_bits, uint32_t value);

#endif // _ADAFRUIT_BUSIO_REGISTER_H