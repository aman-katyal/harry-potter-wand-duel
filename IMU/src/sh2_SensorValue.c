// src/sh2_SensorValue.c

#include "sh2_SensorValue.h"
#include "sh2_util.h"
#include <string.h>
#include <stdbool.h> // <--- FIX: Added this for bool, true, false
#include "sh2_err.h"   // <--- FIX: Added this for SH2_OK, SH2_ERR, etc.

// Storage for the last sensor value received
static sh2_SensorValue_t sensorValue;
static bool valueReady = false;

// This is an internal function used by sh2.c
// It decodes a sensor report from the raw payload
int sh2_decode_sensor_event(sh2_SensorValue_t *value,
                            const uint8_t *payload, uint16_t len)
{
    uint8_t reportId = payload[0];
    
    value->sensorId = reportId;
    value->timestamp = TO_U32(&payload[2]);
    value->status = payload[1] & 0x03;

    switch(reportId) {
        case SH2_ROTATION_VECTOR:
        case SH2_GAME_ROTATION_VECTOR:
        case SH2_GEOMAGNETIC_ROTATION_VECTOR:
            value->un.rotationVector.rotationVector.i = (float)((int16_t)TO_U16(&payload[6])) * SENSOR_Q14_2_FLOAT;
            value->un.rotationVector.rotationVector.j = (float)((int16_t)TO_U16(&payload[8])) * SENSOR_Q14_2_FLOAT;
            value->un.rotationVector.rotationVector.k = (float)((int16_t)TO_U16(&payload[10])) * SENSOR_Q14_2_FLOAT;
            value->un.rotationVector.rotationVector.real = (float)((int16_t)TO_U16(&payload[12])) * SENSOR_Q14_2_FLOAT;
            value->un.rotationVector.rotationVector.accuracy = (float)((int16_t)TO_U16(&payload[14])) * SENSOR_Q14_2_FLOAT;
            break;
        
        default:
            return SH2_ERR_BAD_PARAM;
    }

    memcpy(&sensorValue, value, sizeof(sh2_SensorValue_t));
    valueReady = true;

    return SH2_OK;
}

// Public function to get the latest sensor value
int sh2_getSensorValue(sh2_SensorValue_t *value)
{
    if (valueReady) {
        memcpy(value, &sensorValue, sizeof(sh2_SensorValue_t));
        valueReady = false;
        return SH2_OK;
    }
    else {
        return SH2_ERR;
    }
}