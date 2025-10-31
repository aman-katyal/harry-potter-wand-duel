// src/main.c

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/stdio.h" // <-- ADDED THIS HEADER for stdio_usb_init()
#include "sh2.h"
#include "sh2_hal.h"
#include "sh2_SensorValue.h"

// The BNO08x sensor object
static sh2_SensorValue_t sensorValue;

// This function will be called when a new sensor report is received
static void sensor_event_handler(void *cookie, sh2_AsyncEvent_t *pEvent) {
    if (pEvent->eventId == SH2_RESET) {
        printf("BNO08x RESET detected.\n");
        // Re-enable the rotation vector report with a 100Hz update rate (10,000 us)
        sh2_setSensorConfig(SH2_ROTATION_VECTOR, 10000);
    }
}

int main(void) {
    // Initialize USB serial for printing
    stdio_usb_init();

    // Give the serial monitor time to connect
    sleep_ms(2500);
    printf("\n--- BNO08x SPI Example for RP2350 ---\n");

    // Initialize the BNO08x hardware abstraction layer
    int status = sh2_hal_init();
    if (status != 0) {
        printf("HAL initialization failed. Check wiring.\n");
        while (1) { sleep_ms(100); }
    }
    printf("HAL Initialized.\n");

    // Initialize the SH2 interface
    status = sh2_open(sensor_event_handler, NULL);
    if (status != SH2_OK) {
        printf("SH2 interface failed to open.\n");
        while (1) { sleep_ms(100); }
    }
    printf("SH2 Interface Opened.\n");

    // Tell the BNO08x that the host is now fully booted.
    sh2_setHostMetadata();

    // Enable the Rotation Vector sensor report at 100Hz
    printf("Enabling Rotation Vector report...\n");
    if (sh2_setSensorConfig(SH2_ROTATION_VECTOR, 10000) != SH2_OK) {
        printf("Failed to enable Rotation Vector.\n");
        while (1) { sleep_ms(100); }
    }
    printf("Rotation Vector enabled.\n\n");

    while (1) {
        sh2_serviceEvents();

        if (sh2_getSensorValue(&sensorValue) == SH2_OK) {
            if (sensorValue.sensorId == SH2_ROTATION_VECTOR) {
                // --- THIS SECTION IS FIXED ---
                // The quaternion is one level deeper in the struct
                printf("Quat (i,j,k,real): % 6.2f, % 6.2f, % 6.2f, % 6.2f\r",
                       sensorValue.un.rotationVector.rotationVector.i,
                       sensorValue.un.rotationVector.rotationVector.j,
                       sensorValue.un.rotationVector.rotationVector.k,
                       sensorValue.un.rotationVector.rotationVector.real);
            }
        }
    }

    return 0;
}