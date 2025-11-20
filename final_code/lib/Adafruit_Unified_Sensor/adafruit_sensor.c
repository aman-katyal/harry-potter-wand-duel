/**
 * @file adafruit_sensor.c
 * @brief C implementation for the Adafruit unified sensor abstraction layer.
 */

#include "adafruit_sensor.h"
#include <stdio.h> // Used for printf. Ensure your target's stdio is initialized.

/**************************************************************************/
/*!
    @brief  Prints sensor information to the standard output.
    @param  sensor A pointer to the initialized sensor object.
*/
/**************************************************************************/
void adafruit_sensor_print_details(adafruit_sensor_t *sensor) {
    // Validate that the sensor object and its function pointer are valid
    if (sensor == NULL || sensor->get_sensor == NULL) {
        printf("ERROR: Invalid sensor object provided.\n");
        return;
    }

    sensor_t details;
    // Call the function pointer to populate the details struct
    sensor->get_sensor(sensor, &details);

    printf("------------------------------------\n");
    printf("Sensor:       %s\n", details.name);
    printf("Type:         ");

    switch ((sensors_type_t)details.type) {
        case SENSOR_TYPE_ACCELEROMETER:
            printf("Acceleration (m/s2)");
            break;
        case SENSOR_TYPE_MAGNETIC_FIELD:
            printf("Magnetic (uT)");
            break;
        case SENSOR_TYPE_ORIENTATION:
            printf("Orientation (degrees)");
            break;
        case SENSOR_TYPE_GYROSCOPE:
            printf("Gyroscopic (rad/s)");
            break;
        case SENSOR_TYPE_LIGHT:
            printf("Light (lux)");
            break;
        case SENSOR_TYPE_PRESSURE:
            printf("Pressure (hPa)");
            break;
        case SENSOR_TYPE_PROXIMITY:
            printf("Distance (cm)");
            break;
        case SENSOR_TYPE_GRAVITY:
            printf("Gravity (m/s2)");
            break;
        case SENSOR_TYPE_LINEAR_ACCELERATION:
            printf("Linear Acceleration (m/s2)");
            break;
        case SENSOR_TYPE_ROTATION_VECTOR:
            printf("Rotation vector");
            break;
        case SENSOR_TYPE_RELATIVE_HUMIDITY:
            printf("Relative Humidity (%%)");
            break;
        case SENSOR_TYPE_AMBIENT_TEMPERATURE:
            printf("Ambient Temp (C)");
            break;
        // NOTE: SENSOR_TYPE_OBJECT_TEMPERATURE was missing from the original .cpp file
        case SENSOR_TYPE_VOLTAGE:
            printf("Voltage (V)");
            break;
        case SENSOR_TYPE_CURRENT:
            printf("Current (mA)");
            break;
        case SENSOR_TYPE_COLOR:
            printf("Color (RGBA)");
            break;
        case SENSOR_TYPE_TVOC:
            printf("Total Volatile Organic Compounds (ppb)");
            break;
        case SENSOR_TYPE_VOC_INDEX:
            printf("Volatile Organic Compounds (Index)");
            break;
        case SENSOR_TYPE_NOX_INDEX:
            printf("Nitrogen Oxides (Index)");
            break;
        case SENSOR_TYPE_CO2:
            printf("Carbon Dioxide (ppm)");
            break;
        case SENSOR_TYPE_ECO2:
            printf("Equivalent/estimated CO2 (ppm)");
            break;
        case SENSOR_TYPE_PM10_STD:
            printf("Standard Particulate Matter 1.0 (ug/m3)");
            break;
        case SENSOR_TYPE_PM25_STD:
            printf("Standard Particulate Matter 2.5 (ug/m3)");
            break;
        case SENSOR_TYPE_PM100_STD:
            printf("Standard Particulate Matter 10.0 (ug/m3)");
            break;
        case SENSOR_TYPE_PM10_ENV:
            printf("Environmental Particulate Matter 1.0 (ug/m3)");
            break;
        case SENSOR_TYPE_PM25_ENV:
            printf("Environmental Particulate Matter 2.5 (ug/m3)");
            break;
        case SENSOR_TYPE_PM100_ENV:
            printf("Environmental Particulate Matter 10.0 (ug/m3)");
            break;
        case SENSOR_TYPE_GAS_RESISTANCE:
            printf("Gas Resistance (ohms)");
            break;
        case SENSOR_TYPE_UNITLESS_PERCENT:
            printf("Unitless Percent (%%)");
            break;
        case SENSOR_TYPE_ALTITUDE:
            printf("Altitude (m)");
            break;
        default:
            printf("Unknown Type");
            break;
    }

    printf("\n");
    printf("Driver Ver:   %ld\n", details.version);
    printf("Unique ID:    %ld\n", details.sensor_id);
    printf("Min Value:    %f\n", details.min_value);
    printf("Max Value:    %f\n", details.max_value);
    printf("Resolution:   %f\n", details.resolution);
    printf("------------------------------------\n\n");
}