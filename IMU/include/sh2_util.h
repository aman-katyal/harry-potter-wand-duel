// src/sh2_util.h

#ifndef SH2_UTIL_H
#define SH2_UTIL_H

// --- FIX: Replaced broken macros with safer ones ---

// Read a 16-bit little-endian value from a buffer
#define TO_U16(p) ((uint16_t)(p)[0] | ((uint16_t)(p)[1] << 8))

// Read a 32-bit little-endian value from a buffer
#define TO_U32(p) ((uint32_t)(p)[0] | ((uint32_t)(p)[1] << 8) | \
                   ((uint32_t)(p)[2] << 16) | ((uint32_t)(p)[3] << 24))

// Write a 16-bit little-endian value to a buffer
#define SET_U16(p, v) do { (p)[0] = (v); (p)[1] = (v) >> 8; } while(0)

// Write a 32-bit little-endian value to a buffer
#define SET_U32(p, v) do { (p)[0] = (v); (p)[1] = (v) >> 8; (p)[2] = (v) >> 16; (p)[3] = (v) >> 24; } while(0)


#define SENSOR_Q14_2_FLOAT (1.0 / (1<<14))

#endif // SH2_UTIL_H