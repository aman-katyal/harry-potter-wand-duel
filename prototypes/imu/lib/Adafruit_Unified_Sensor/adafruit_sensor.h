/**
 * @file adafruit_sensor.h
 * @brief A C-based unified sensor abstraction layer.
 *
 * This is a C translation of the Adafruit Unified Sensor Driver library.
 * It provides a common interface for different types of sensors.
 *
 * Originally based on the Android Open Source Project sensor library.
 * Updated by K. Townsend (Adafruit Industries).
 * Translated to C for embedded systems by AI assistant.
 */

#ifndef _ADAFRUIT_SENSOR_H
#define _ADAFRUIT_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

/* Constants from the original library */
#define SENSORS_GRAVITY_EARTH (9.80665F)      /**< Earth's gravity in m/s^2 */
#define SENSORS_GRAVITY_MOON (1.6F)           /**< The moon's gravity in m/s^2 */
#define SENSORS_GRAVITY_SUN (275.0F)          /**< The sun's gravity in m/s^2 */
#define SENSORS_GRAVITY_STANDARD (SENSORS_GRAVITY_EARTH)
#define SENSORS_MAGFIELD_EARTH_MAX (60.0F)    /**< Maximum magnetic field on Earth's surface */
#define SENSORS_MAGFIELD_EARTH_MIN (30.0F)    /**< Minimum magnetic field on Earth's surface */
#define SENSORS_PRESSURE_SEALEVELHPA (1013.25F) /**< Average sea level pressure is 1013.25 hPa */
#define SENSORS_DPS_TO_RADS (0.017453293F)    /**< Degrees/s to rad/s multiplier */
#define SENSORS_RADS_TO_DPS (57.29577793F)    /**< Rad/s to degrees/s multiplier */
#define SENSORS_GAUSS_TO_MICROTESLA (100)     /**< Gauss to micro-Tesla multiplier */

/**
 * @brief Enum for all possible sensor types.
 */
typedef enum
{
    SENSOR_TYPE_ACCELEROMETER       = (1),
    SENSOR_TYPE_MAGNETIC_FIELD      = (2),
    SENSOR_TYPE_ORIENTATION         = (3),
    SENSOR_TYPE_GYROSCOPE           = (4),
    SENSOR_TYPE_LIGHT               = (5),
    SENSOR_TYPE_PRESSURE            = (6),
    SENSOR_TYPE_PROXIMITY           = (8),
    SENSOR_TYPE_GRAVITY             = (9),
    SENSOR_TYPE_LINEAR_ACCELERATION = (10),
    SENSOR_TYPE_ROTATION_VECTOR     = (11),
    SENSOR_TYPE_RELATIVE_HUMIDITY   = (12),
    SENSOR_TYPE_AMBIENT_TEMPERATURE = (13),
    SENSOR_TYPE_VOLTAGE             = (15),
    SENSOR_TYPE_CURRENT             = (16),
    SENSOR_TYPE_COLOR               = (17),
    SENSOR_TYPE_TVOC                = (18),
    SENSOR_TYPE_VOC_INDEX           = (19),
    SENSOR_TYPE_NOX_INDEX           = (20),
    SENSOR_TYPE_CO2                 = (21),
    SENSOR_TYPE_ECO2                = (22),
    SENSOR_TYPE_PM10_STD            = (23),
    SENSOR_TYPE_PM25_STD            = (24),
    SENSOR_TYPE_PM100_STD           = (25),
    SENSOR_TYPE_PM10_ENV            = (26),
    SENSOR_TYPE_PM25_ENV            = (27),
    SENSOR_TYPE_PM100_ENV           = (28),
    SENSOR_TYPE_GAS_RESISTANCE      = (29),
    SENSOR_TYPE_UNITLESS_PERCENT    = (30),
    SENSOR_TYPE_ALTITUDE            = (31),
} sensors_type_t;

/**
 * @brief A structure to return vector data in a common format.
 */
typedef struct {
    union {
        float v[3];
        struct {
            float x;
            float y;
            float z;
        };
        /* Orientation sensors */
        struct {
            float roll;
            float pitch;
            float heading;
        };
    };
    int8_t status;
    uint8_t reserved[3];
} sensors_vec_t;

/**
 * @brief A structure to return color data in a common format.
 */
typedef struct {
    union {
        float c[3];
        struct {
            float r;
            float g;
            float b;
        };
    };
    uint32_t rgba;
} sensors_color_t;

/**
 * @brief A structure to provide a single sensor event in a common format.
 */
typedef struct
{
    int32_t version;
    int32_t sensor_id;
    int32_t type;
    int32_t reserved0;
    int32_t timestamp;
    union {
        float           data[4];
        sensors_vec_t   acceleration;
        sensors_vec_t   magnetic;
        sensors_vec_t   orientation;
        sensors_vec_t   gyro;
        float           temperature;
        float           distance;
        float           light;
        float           pressure;
        float           relative_humidity;
        float           current;
        float           voltage;
        float           tvoc;
        float           voc_index;
        float           nox_index;
        float           co2;
        float           eco2;
        float           pm10_std;
        float           pm25_std;
        float           pm100_std;
        float           pm10_env;
        float           pm25_env;
        float           pm100_env;
        float           gas_resistance;
        float           unitless_percent;
        float           altitude;
        sensors_color_t color;
    };
} sensors_event_t;

/**
 * @brief A structure to describe basic information about a specific sensor.
 */
typedef struct
{
    char     name[12];
    int32_t  version;
    int32_t  sensor_id;
    int32_t  type;
    float    max_value;
    float    min_value;
    float    resolution;
    int32_t  min_delay;
} sensor_t;


/* Forward declaration of the main sensor structure. */
struct adafruit_sensor_s;

/**
 * @brief The main sensor object, used to represent a sensor instance in C.
 * This replaces the C++ Adafruit_Sensor class. It uses function pointers
 * to achieve polymorphism, allowing different underlying sensor drivers
 to
 * be controlled by the same interface.
 */
typedef struct adafruit_sensor_s {
    /**
     * @brief A generic pointer to the specific sensor's instance data.
     * This allows a driver to associate its own state (like I2C address,
     * calibration data, etc.) with this abstraction layer.
     */
    void* user_data;

    /**
     * @brief Function pointer to get the latest sensor event.
     * @param self A pointer to this sensor instance.
     * @param event A pointer to a sensors_event_t struct to be populated.
     * @return true on success, false on failure.
     */
    bool (*get_event)(struct adafruit_sensor_s *self, sensors_event_t *event);

    /**
     * @brief Function pointer to get information about the sensor.
     * @param self A pointer to this sensor instance.
     * @param sensor A pointer to a sensor_t struct to be populated.
     */
    void (*get_sensor)(struct adafruit_sensor_s *self, sensor_t *sensor);

} adafruit_sensor_t;


/**
 * @brief Prints detailed information about a sensor to the console.
 * This is a standalone C function that replaces the original C++ class method.
 * @param sensor A pointer to the sensor instance to query.
 */
void adafruit_sensor_print_details(adafruit_sensor_t *sensor);

#endif /* _ADAFRUIT_SENSOR_H */