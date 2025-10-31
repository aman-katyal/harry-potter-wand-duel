// src/sh2_SensorValue.h

#ifndef SH2_SENSOR_VALUE_H
#define SH2_SENSOR_VALUE_H

#include <stdint.h>

// Sensor Hub Report IDs
typedef enum sh2_SensorId_e {
    SH2_RAW_ACCELEROMETER = 0x14,
    SH2_ACCELEROMETER = 0x01,
    SH2_LINEAR_ACCELERATION = 0x04,
    SH2_GRAVITY = 0x06,
    SH2_RAW_GYROSCOPE = 0x15,
    SH2_GYROSCOPE_CALIBRATED = 0x02,
    SH2_GYROSCOPE_UNCALIBRATED = 0x07,
    SH2_RAW_MAGNETOMETER = 0x16,
    SH2_MAGNETIC_FIELD_CALIBRATED = 0x03,
    SH2_MAGNETIC_FIELD_UNCALIBRATED = 0x0F,
    SH2_ROTATION_VECTOR = 0x05,
    SH2_GAME_ROTATION_VECTOR = 0x08,
    SH2_GEOMAGNETIC_ROTATION_VECTOR = 0x09,
    SH2_PRESSURE = 0x0A,
    SH2_AMBIENT_LIGHT = 0x0B,
    SH2_HUMIDITY = 0x0C,
    SH2_PROXIMITY = 0x0D,
    SH2_TEMPERATURE = 0x0E,
    SH2_TAP_DETECTOR = 0x10,
    SH2_STEP_DETECTOR = 0x11,
    SH2_STEP_COUNTER = 0x12,
    SH2_SIGNIFICANT_MOTION = 0x13,
    SH2_SHAKE_DETECTOR = 0x17,
    // SH2_MAX_SENSOR_ID was removed to avoid conflict
} sh2_SensorId_t;

// Measurement status values
typedef enum sh2_SensorStatus_e {
    SH2_STATUS_UNRELIABLE = 0,
    SH2_STATUS_ACCURACY_LOW = 1,
    SH2_STATUS_ACCURACY_MEDIUM = 2,
    SH2_STATUS_ACCURACY_HIGH = 3,
} sh2_SensorStatus_t;

typedef struct {
    float i;
    float j;
    float k;
    float real;
    float accuracy;
} sh2_Quaternion_t;

// ... other structs from the original file ...

typedef struct {
    uint16_t sequence;
    sh2_Quaternion_t rotationVector;
} sh2_RotationVector_t;


typedef struct sh2_SensorValue_s {
    uint8_t sensorId;
    uint8_t status;
    uint32_t timestamp;
    union {
        sh2_RotationVector_t rotationVector;
        // ... all other sensor value types ...
    } un;
} sh2_SensorValue_t;

// This is the fix for the 'unknown type name' error
typedef sh2_SensorValue_t sh2_SensorEvent_t;

// Function prototype
int sh2_getSensorValue(sh2_SensorValue_t *value);

#endif // SH2_SENSOR_VALUE_H