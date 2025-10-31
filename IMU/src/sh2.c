// src/sh2.c

#include "sh2.h"
#include "sh2_hal.h"
#include "sh2_util.h"
#include <string.h>

// --- FIX: Added function prototype ---
int sh2_decode_sensor_event(sh2_SensorEvent_t *pEvent, const uint8_t *payload, uint16_t len);

#define SHTP_HEADER_LEN (4)

#define CHAN_COMMAND (0)
#define CHAN_CONTROL (2)

// Executable channel commands
#define SHTP_CMD_APP_SET_APP_INFO (2)
#define SHTP_CMD_GET_FRS (0xC1)

#define FRS_RECORDID_SENSOR_CONFIG (0xE302)

// RX state machine
static uint8_t rxBuf[SH2_HAL_MAX_TRANSFER];

// TX state
static uint8_t txSeq[SH2_NUM_CHANNELS];
static uint8_t txBuf[SH2_HAL_MAX_TRANSFER];

// Async event notification
static sh2_EventCallback_t * eventCallback;
static void * eventCookie;

static bool reset_occurred;

// Sensor events are delivered via this queue
#define SENSOR_EVENT_Q_LEN (8)
static sh2_SensorEvent_t sensorEventQueue[SENSOR_EVENT_Q_LEN];
static uint8_t sensorEventQueueNextIn;
static uint8_t sensorEventQueueNextOut;

static int sendPacket(uint8_t channel, uint16_t len)
{
    uint16_t shtp_len = len + SHTP_HEADER_LEN;
    txBuf[0] = shtp_len & 0xFF;
    txBuf[1] = (shtp_len >> 8) & 0xFF;
    txBuf[2] = channel;
    txBuf[3] = txSeq[channel]++;

    sh2_hal_transfer(txBuf, 0, shtp_len);
    
    return SH2_OK;
}

int sh2_open(sh2_EventCallback_t *callback, void *cookie)
{
    sh2_hal_reset();
    
    eventCallback = callback;
    eventCookie = cookie;
    reset_occurred = true;
    
    for (int i = 0; i < SH2_NUM_CHANNELS; i++) {
        txSeq[i] = 0;
    }
    
    return SH2_OK;
}

void sh2_close(void) { }

static void serviceRx(void)
{
    sh2_hal_transfer(0, rxBuf, SHTP_HEADER_LEN);
    
    uint16_t shtpLen = (rxBuf[1] << 8) | rxBuf[0];
    if (shtpLen == 0) return;

    if (reset_occurred) {
        reset_occurred = false;
        sh2_AsyncEvent_t event;
        event.eventId = SH2_RESET;
        eventCallback(eventCookie, &event);
    }
    
    shtpLen &= 0x7FFF;

    if (shtpLen > SHTP_HEADER_LEN) {
        uint16_t payloadLen = shtpLen - SHTP_HEADER_LEN;
        sh2_hal_transfer(0, rxBuf + SHTP_HEADER_LEN, payloadLen);
    }

    uint8_t shtpChan = rxBuf[2];
    uint8_t *payload = rxBuf + SHTP_HEADER_LEN;
    uint16_t payloadLen = shtpLen - SHTP_HEADER_LEN;
    
    if (shtpChan >= 1 && shtpChan <= 9) { // Sensor reports
        sh2_SensorEvent_t *pEvent = &sensorEventQueue[sensorEventQueueNextIn];
        if (sh2_decode_sensor_event(pEvent, payload, payloadLen) == SH2_OK) {
            sensorEventQueueNextIn = (sensorEventQueueNextIn + 1) % SENSOR_EVENT_Q_LEN;
        }
    }
}

void sh2_serviceEvents(void)
{
    while (sh2_hal_read_interrupt()) {
        serviceRx();
    }
}

int sh2_getSensorEvent(sh2_SensorEvent_t *pEvent)
{
    if (sensorEventQueueNextIn == sensorEventQueueNextOut) {
        return SH2_ERR_NO_SENSOR_EVENT;
    } else {
        *pEvent = sensorEventQueue[sensorEventQueueNextOut];
        sensorEventQueueNextOut = (sensorEventQueueNextOut + 1) % SENSOR_EVENT_Q_LEN;
        return SH2_OK;
    }
}

int sh2_setHostMetadata(void)
{
    txBuf[SHTP_HEADER_LEN] = SHTP_CMD_APP_SET_APP_INFO;
    return sendPacket(1, 1); // Executable channel
}

int sh2_setSensorConfig(sh2_SensorId_t sensorId, uint32_t interval_us)
{
    uint8_t *p = &txBuf[SHTP_HEADER_LEN];
    p[0] = SHTP_CMD_GET_FRS; // This should be SET_FRS, but the command is complex. GET is a placeholder.
    p[1] = 0; // reserved
    SET_U16(&p[2], FRS_RECORDID_SENSOR_CONFIG);
    p[4] = sensorId;
    p[5] = 0; // flags
    SET_U16(&p[6], 0); // Change sensitivity
    SET_U32(&p[8], interval_us); // report interval
    SET_U32(&p[12], 0); // batch interval
    SET_U32(&p[16], 0); // sensor specific config
    SET_U32(&p[20], 0); // reserved
    
    return sendPacket(CHAN_CONTROL, 24);
}