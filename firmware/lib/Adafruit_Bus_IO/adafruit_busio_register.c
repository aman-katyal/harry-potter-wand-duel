/**
 * @file adafruit_busio_register.c
 * @brief C implementation for the I2C register abstraction layer.
 */

#include "adafruit_busio_register.h"

// --- Register Functions Implementation ---

void adafruit_busio_register_init(adafruit_busio_register_t *reg, adafruit_i2cdev_t *device,
                                  uint16_t reg_address, uint8_t width, uint8_t address_width, uint8_t byteorder) {
    reg->device = device;
    reg->reg_addr = reg_address;
    reg->width = width;
    reg->addr_width = address_width;
    reg->byteorder = byteorder;
}

bool adafruit_busio_register_write_buf(adafruit_busio_register_t *reg, uint8_t *buffer, uint8_t len) {
    if (!reg || !reg->device) return false;

    uint8_t addr_buffer[2];
    if (reg->addr_width == 2) {
        addr_buffer[0] = (uint8_t)(reg->reg_addr >> 8); // MSB first
        addr_buffer[1] = (uint8_t)(reg->reg_addr & 0xFF);
    } else {
        addr_buffer[0] = (uint8_t)(reg->reg_addr & 0xFF);
    }

    return adafruit_i2cdev_write(reg->device, buffer, len, true, addr_buffer, reg->addr_width);
}

bool adafruit_busio_register_write(adafruit_busio_register_t *reg, uint32_t value, uint8_t num_bytes) {
    if (num_bytes == 0) {
        num_bytes = reg->width;
    }
    if (num_bytes > 4) {
        return false;
    }

    uint8_t buffer[4];
    for (int i = 0; i < num_bytes; i++) {
        if (reg->byteorder == BUSIO_LSBFIRST) {
            buffer[i] = (value >> (i * 8)) & 0xFF;
        } else {
            buffer[num_bytes - 1 - i] = (value >> (i * 8)) & 0xFF;
        }
    }
    return adafruit_busio_register_write_buf(reg, buffer, num_bytes);
}

bool adafruit_busio_register_read_buf(adafruit_busio_register_t *reg, uint8_t *buffer, uint8_t len) {
    if (!reg || !reg->device) return false;

    uint8_t addr_buffer[2];
    if (reg->addr_width == 2) {
        addr_buffer[0] = (uint8_t)(reg->reg_addr >> 8); // MSB first
        addr_buffer[1] = (uint8_t)(reg->reg_addr & 0xFF);
    } else {
        addr_buffer[0] = (uint8_t)(reg->reg_addr & 0xFF);
    }

    return adafruit_i2cdev_write_then_read(reg->device, addr_buffer, reg->addr_width, buffer, len);
}

bool adafruit_busio_register_read(adafruit_busio_register_t *reg, uint32_t *result) {
    if (reg->width > 4) return false;

    uint8_t buffer[4];
    if (!adafruit_busio_register_read_buf(reg, buffer, reg->width)) {
        return false;
    }

    *result = 0;
    for (int i = 0; i < reg->width; i++) {
        if (reg->byteorder == BUSIO_LSBFIRST) {
            *result |= ((uint32_t)buffer[i]) << (i * 8);
        } else {
            *result = (*result << 8) | buffer[i];
        }
    }
    return true;
}


// --- Register Bits Functions Implementation ---

void adafruit_busio_register_bits_init(adafruit_busio_register_bits_t *reg_bits,
                                       adafruit_busio_register_t *reg, uint8_t num_bits, uint8_t shift) {
    reg_bits->reg = reg;
    reg_bits->bits = num_bits;
    reg_bits->shift = shift;
}

bool adafruit_busio_register_bits_read(adafruit_busio_register_bits_t *reg_bits, uint32_t *result) {
    if (!reg_bits || !reg_bits->reg) return false;

    uint32_t reg_val;
    if (!adafruit_busio_register_read(reg_bits->reg, &reg_val)) {
        return false;
    }

    uint32_t mask = (1 << reg_bits->bits) - 1;
    *result = (reg_val >> reg_bits->shift) & mask;
    return true;
}

bool adafruit_busio_register_bits_write(adafruit_busio_register_bits_t *reg_bits, uint32_t value) {
    if (!reg_bits || !reg_bits->reg) return false;

    uint32_t reg_val;
    if (!adafruit_busio_register_read(reg_bits->reg, &reg_val)) {
        return false;
    }

    uint32_t mask = (1 << reg_bits->bits) - 1;
    value &= mask; // Ensure value fits within the bit slice

    mask <<= reg_bits->shift;
    reg_val &= ~mask; // Clear the bits we're about to set
    reg_val |= (value << reg_bits->shift); // Set the new value

    return adafruit_busio_register_write(reg_bits->reg, reg_val, 0);
}