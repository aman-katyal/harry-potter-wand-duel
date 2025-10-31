// src/sh2.h

#ifndef SH2_H
#define SH2_H

#include <stdint.h>
#include <stdbool.h>
#include "sh2_err.h"
#include "sh2_SensorValue.h" // Include the sensor value definitions

// Advertisement record
typedef struct sh2_Advertisement_s {
    uint8_t channel;
    uint8_t reportId;
    uint16_t reserved;
} sh2_Advertisement_t;

// Sensor Hub Reset event
#define SH2_RESET 0x8000

// Async Event IDs
typedef enum sh2_AsyncEventId_e {
    SH2_ASYNC_EVENT_ID_SELF_TEST_RESULT = 1,
    SH2_ASYNC_EVENT_ID_ERROR = 2,
    SH2_ASYNC_EVENT_ID_SENSOR_HUB_STATUS = 3,
} sh2_AsyncEventId_t;

typedef struct sh2_ErrorRecord_s {
    uint8_t severity;
    uint8_t sequence;
    uint8_t source;
    uint8_t error;
    uint8_t module;
    uint8_t code;
} sh2_ErrorRecord_t;

typedef struct sh2_SelfTestResult_s {
    uint8_t sensorId;
    uint8_t status; // 0:pass, 1:fail
} sh2_SelfTestResult_t;

typedef struct sh2_SensorhubStatus_s {
    uint8_t sensorId;
    uint8_t status;
} sh2_SensorhubStatus_t;

typedef struct sh2_AsyncEvent_s {
    uint16_t eventId; // SH2_RESET or sh2_AsyncEventId_t
    union {
        sh2_ErrorRecord_t errorRecord;
        sh2_SelfTestResult_t selfTestResult;
        sh2_SensorhubStatus_t sensorhubStatus;
    } un;
} sh2_AsyncEvent_t;

// Callback with event from sensor hub
typedef void(sh2_EventCallback_t)(void * cookie, sh2_AsyncEvent_t *pEvent);

// Open an SH-2 session.
int sh2_open(sh2_EventCallback_t *eventCallback, void *callbackCookie);

// Close an SH-2 session.
void sh2_close(void);

// Get a report from the sensor hub.
int sh2_getSensorEvent(sh2_SensorEvent_t *pEvent);

// Set a sensor configuration
int sh2_setSensorConfig(sh2_SensorId_t sensorId, uint32_t interval_us);

// Send Host Metadata to sensor hub
int sh2_setHostMetadata(void);

// Service sensor hub events.
void sh2_serviceEvents(void);

#endif // SH2_H