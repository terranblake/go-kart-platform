/* Automatically generated nanopb header */
/* Generated by nanopb-1.0.0-dev */

#ifndef PB_KART_NAVIGATION_NAVIGATION_PB_H_INCLUDED
#define PB_KART_NAVIGATION_NAVIGATION_PB_H_INCLUDED
#include <pb.h>
#include "common.pb.h"

#if PB_PROTO_HEADER_VERSION != 40
#error Regenerate this file with the current version of nanopb generator.
#endif

/* Enum definitions */
/* Navigation component IDs - Identifies the specific source or group */
typedef enum _kart_navigation_NavigationComponentId {
    kart_navigation_NavigationComponentId_IMU_PRIMARY = 0, /* Main Inertial Measurement Unit (e.g., GY-521) */
    kart_navigation_NavigationComponentId_GPS_PRIMARY = 1, /* Main GPS unit (e.g., ATGM336H) */
    kart_navigation_NavigationComponentId_ENVIRONMENT_PRIMARY = 2, /* Main environment sensor (e.g., AHT21) */
    /* Add IDs for secondary/redundant sensors if needed */
    kart_navigation_NavigationComponentId_ALL = 255 /* All navigation components (broadcast) */
} kart_navigation_NavigationComponentId;

/* Navigation command IDs - Specifies the data type being reported OR configured */
typedef enum _kart_navigation_NavigationCommandId {
    /* --- Sensor Readings --- */
    kart_navigation_NavigationCommandId_ACCEL_X = 0, /* Acceleration X-axis (m/s^2, float) */
    kart_navigation_NavigationCommandId_ACCEL_Y = 1, /* Acceleration Y-axis (m/s^2, float) */
    kart_navigation_NavigationCommandId_ACCEL_Z = 2, /* Acceleration Z-axis (m/s^2, float) */
    kart_navigation_NavigationCommandId_GYRO_X = 3, /* Gyroscope X-axis (rad/s, float) */
    kart_navigation_NavigationCommandId_GYRO_Y = 4, /* Gyroscope Y-axis (rad/s, float) */
    kart_navigation_NavigationCommandId_GYRO_Z = 5, /* Gyroscope Z-axis (rad/s, float) */
    kart_navigation_NavigationCommandId_IMU_TEMPERATURE = 6, /* Internal IMU temperature (°C, float) */
    kart_navigation_NavigationCommandId_LATITUDE = 10, /* Latitude (degrees, float) */
    kart_navigation_NavigationCommandId_LONGITUDE = 11, /* Longitude (degrees, float) */
    kart_navigation_NavigationCommandId_ALTITUDE = 12, /* Altitude (meters, float) */
    kart_navigation_NavigationCommandId_SPEED = 13, /* Speed over ground (km/h, float) */
    kart_navigation_NavigationCommandId_COURSE = 14, /* Course over ground (degrees, float) */
    kart_navigation_NavigationCommandId_SATELLITES = 15, /* Number of satellites in view (count, uint8) */
    kart_navigation_NavigationCommandId_HDOP = 16, /* Horizontal Dilution of Precision (dimensionless, float) */
    kart_navigation_NavigationCommandId_GPS_FIX_STATUS = 17, /* GPS Fix Status (enum GpsFixStatus, uint8) */
    kart_navigation_NavigationCommandId_TEMPERATURE_AMBIENT = 20, /* Ambient temperature (°C, int16 -> tenths of degree) */
    kart_navigation_NavigationCommandId_HUMIDITY_RELATIVE = 21, /* Relative humidity (%, uint8) */
    /* --- Configuration Commands --- 
 IMU Configuration */
    kart_navigation_NavigationCommandId_ACCELEROMETER_RANGE = 50, /* Set IMU Accelerometer Range (enum AccelerometerRangeValue, uint8) */
    kart_navigation_NavigationCommandId_GYROSCOPE_RANGE = 51, /* Set IMU Gyroscope Range (enum GyroscopeRangeValue, uint8) */
    kart_navigation_NavigationCommandId_FILTER_BANDWIDTH = 52, /* Set IMU DLPF Bandwidth (enum FilterBandwidthValue, uint8) */
    kart_navigation_NavigationCommandId_TRIGGER_CALIBRATION = 53, /* Trigger IMU Calibration (value ignored or specific type? TBD) */
    /* GPS Configuration */
    kart_navigation_NavigationCommandId_GPS_UPDATE_RATE = 60, /* Set GPS Update Rate (enum GpsUpdateRateValue, uint8) */
    kart_navigation_NavigationCommandId_GNSS_CONSTELLATION = 61, /* Set GNSS Constellations (enum GnssConstellationValue - bitmask? uint8) */
    kart_navigation_NavigationCommandId_NMEA_OUTPUT_CONFIG = 62, /* Set NMEA Output Sentences (value TBD - bitmask? uint8) */
    kart_navigation_NavigationCommandId_STATIC_NAVIGATION_MODE = 63 /* Set Static Navigation Mode (enum StaticNavigationValue, uint8) */
} kart_navigation_NavigationCommandId;

/* MPU-6050 Accelerometer Range Values (maps to Adafruit library defines) */
typedef enum _kart_navigation_AccelerometerRangeValue {
    kart_navigation_AccelerometerRangeValue_RANGE_2_G = 0, /* MPU6050_RANGE_2_G */
    kart_navigation_AccelerometerRangeValue_RANGE_4_G = 1, /* MPU6050_RANGE_4_G */
    kart_navigation_AccelerometerRangeValue_RANGE_8_G = 2, /* MPU6050_RANGE_8_G */
    kart_navigation_AccelerometerRangeValue_RANGE_16_G = 3 /* MPU6050_RANGE_16_G */
} kart_navigation_AccelerometerRangeValue;

/* MPU-6050 Gyroscope Range Values (maps to Adafruit library defines) */
typedef enum _kart_navigation_GyroscopeRangeValue {
    kart_navigation_GyroscopeRangeValue_RANGE_250_DEG = 0, /* MPU6050_RANGE_250_DEG */
    kart_navigation_GyroscopeRangeValue_RANGE_500_DEG = 1, /* MPU6050_RANGE_500_DEG */
    kart_navigation_GyroscopeRangeValue_RANGE_1000_DEG = 2, /* MPU6050_RANGE_1000_DEG */
    kart_navigation_GyroscopeRangeValue_RANGE_2000_DEG = 3 /* MPU6050_RANGE_2000_DEG */
} kart_navigation_GyroscopeRangeValue;

/* MPU-6050 Digital Low Pass Filter Bandwidth Values (maps to Adafruit library defines) */
typedef enum _kart_navigation_FilterBandwidthValue {
    kart_navigation_FilterBandwidthValue_BAND_260_HZ = 0, /* MPU6050_BAND_260_HZ (No filter or highest BW) */
    kart_navigation_FilterBandwidthValue_BAND_184_HZ = 1, /* MPU6050_BAND_184_HZ */
    kart_navigation_FilterBandwidthValue_BAND_94_HZ = 2, /* MPU6050_BAND_94_HZ */
    kart_navigation_FilterBandwidthValue_BAND_44_HZ = 3, /* MPU6050_BAND_44_HZ */
    kart_navigation_FilterBandwidthValue_BAND_21_HZ = 4, /* MPU6050_BAND_21_HZ */
    kart_navigation_FilterBandwidthValue_BAND_10_HZ = 5, /* MPU6050_BAND_10_HZ */
    kart_navigation_FilterBandwidthValue_BAND_5_HZ = 6 /* MPU6050_BAND_5_HZ */
} kart_navigation_FilterBandwidthValue;

/* GPS Update Rate Values */
typedef enum _kart_navigation_GpsUpdateRateValue {
    kart_navigation_GpsUpdateRateValue_RATE_UNKNOWN = 0, /* Default/unset value */
    kart_navigation_GpsUpdateRateValue_RATE_1_HZ = 1,
    kart_navigation_GpsUpdateRateValue_RATE_5_HZ = 5,
    kart_navigation_GpsUpdateRateValue_RATE_10_HZ = 10 /* Check ATGM336H datasheet for max supported rate */
} kart_navigation_GpsUpdateRateValue;

/* GNSS Constellation Selection (Example - Exact values depend on module commands)
 This might be better as individual enable/disable commands or a bitmask */
typedef enum _kart_navigation_GnssConstellationValue {
    kart_navigation_GnssConstellationValue_GPS_ONLY = 0,
    kart_navigation_GnssConstellationValue_GPS_GLONASS = 1, /* Example value */
    kart_navigation_GnssConstellationValue_GPS_BEIDOU = 2, /* Example value */
    kart_navigation_GnssConstellationValue_GPS_GALILEO = 3, /* Example value (If supported) */
    kart_navigation_GnssConstellationValue_ALL_SUPPORTED = 4 /* Example value */
} kart_navigation_GnssConstellationValue;

/* NMEA Output Configuration (Example - Detail depends on module commands)
 Often uses bitmasks to enable/disable specific sentences (GGA, RMC, VTG, etc.) */
typedef enum _kart_navigation_NmeaOutputValue {
    kart_navigation_NmeaOutputValue_DEFAULT_OUTPUT = 0,
    kart_navigation_NmeaOutputValue_ESSENTIAL_OUTPUT = 1, /* e.g., GGA, RMC only */
    kart_navigation_NmeaOutputValue_DEBUG_OUTPUT = 2 /* e.g., include GSV, GSA */
} kart_navigation_NmeaOutputValue;

/* GPS Static Navigation Mode */
typedef enum _kart_navigation_StaticNavigationValue {
    kart_navigation_StaticNavigationValue_STATIC_MODE_OFF = 0, /* Normal operation */
    kart_navigation_StaticNavigationValue_STATIC_MODE_ON = 1 /* Enable static navigation (for stationary use) */
} kart_navigation_StaticNavigationValue;

/* GPS Fix Status values */
typedef enum _kart_navigation_GpsFixStatus {
    kart_navigation_GpsFixStatus_NO_FIX = 0, /* No fix available */
    kart_navigation_GpsFixStatus_FIX_ACQUIRED = 1, /* Standard GPS/GNSS fix */
    kart_navigation_GpsFixStatus_DIFFERENTIAL_FIX = 2 /* Differential GPS fix (DGPS) */
} kart_navigation_GpsFixStatus;

#ifdef __cplusplus
extern "C" {
#endif

/* Helper constants for enums */
#define _kart_navigation_NavigationComponentId_MIN kart_navigation_NavigationComponentId_IMU_PRIMARY
#define _kart_navigation_NavigationComponentId_MAX kart_navigation_NavigationComponentId_ALL
#define _kart_navigation_NavigationComponentId_ARRAYSIZE ((kart_navigation_NavigationComponentId)(kart_navigation_NavigationComponentId_ALL+1))

#define _kart_navigation_NavigationCommandId_MIN kart_navigation_NavigationCommandId_ACCEL_X
#define _kart_navigation_NavigationCommandId_MAX kart_navigation_NavigationCommandId_STATIC_NAVIGATION_MODE
#define _kart_navigation_NavigationCommandId_ARRAYSIZE ((kart_navigation_NavigationCommandId)(kart_navigation_NavigationCommandId_STATIC_NAVIGATION_MODE+1))

#define _kart_navigation_AccelerometerRangeValue_MIN kart_navigation_AccelerometerRangeValue_RANGE_2_G
#define _kart_navigation_AccelerometerRangeValue_MAX kart_navigation_AccelerometerRangeValue_RANGE_16_G
#define _kart_navigation_AccelerometerRangeValue_ARRAYSIZE ((kart_navigation_AccelerometerRangeValue)(kart_navigation_AccelerometerRangeValue_RANGE_16_G+1))

#define _kart_navigation_GyroscopeRangeValue_MIN kart_navigation_GyroscopeRangeValue_RANGE_250_DEG
#define _kart_navigation_GyroscopeRangeValue_MAX kart_navigation_GyroscopeRangeValue_RANGE_2000_DEG
#define _kart_navigation_GyroscopeRangeValue_ARRAYSIZE ((kart_navigation_GyroscopeRangeValue)(kart_navigation_GyroscopeRangeValue_RANGE_2000_DEG+1))

#define _kart_navigation_FilterBandwidthValue_MIN kart_navigation_FilterBandwidthValue_BAND_260_HZ
#define _kart_navigation_FilterBandwidthValue_MAX kart_navigation_FilterBandwidthValue_BAND_5_HZ
#define _kart_navigation_FilterBandwidthValue_ARRAYSIZE ((kart_navigation_FilterBandwidthValue)(kart_navigation_FilterBandwidthValue_BAND_5_HZ+1))

#define _kart_navigation_GpsUpdateRateValue_MIN kart_navigation_GpsUpdateRateValue_RATE_UNKNOWN
#define _kart_navigation_GpsUpdateRateValue_MAX kart_navigation_GpsUpdateRateValue_RATE_10_HZ
#define _kart_navigation_GpsUpdateRateValue_ARRAYSIZE ((kart_navigation_GpsUpdateRateValue)(kart_navigation_GpsUpdateRateValue_RATE_10_HZ+1))

#define _kart_navigation_GnssConstellationValue_MIN kart_navigation_GnssConstellationValue_GPS_ONLY
#define _kart_navigation_GnssConstellationValue_MAX kart_navigation_GnssConstellationValue_ALL_SUPPORTED
#define _kart_navigation_GnssConstellationValue_ARRAYSIZE ((kart_navigation_GnssConstellationValue)(kart_navigation_GnssConstellationValue_ALL_SUPPORTED+1))

#define _kart_navigation_NmeaOutputValue_MIN kart_navigation_NmeaOutputValue_DEFAULT_OUTPUT
#define _kart_navigation_NmeaOutputValue_MAX kart_navigation_NmeaOutputValue_DEBUG_OUTPUT
#define _kart_navigation_NmeaOutputValue_ARRAYSIZE ((kart_navigation_NmeaOutputValue)(kart_navigation_NmeaOutputValue_DEBUG_OUTPUT+1))

#define _kart_navigation_StaticNavigationValue_MIN kart_navigation_StaticNavigationValue_STATIC_MODE_OFF
#define _kart_navigation_StaticNavigationValue_MAX kart_navigation_StaticNavigationValue_STATIC_MODE_ON
#define _kart_navigation_StaticNavigationValue_ARRAYSIZE ((kart_navigation_StaticNavigationValue)(kart_navigation_StaticNavigationValue_STATIC_MODE_ON+1))

#define _kart_navigation_GpsFixStatus_MIN kart_navigation_GpsFixStatus_NO_FIX
#define _kart_navigation_GpsFixStatus_MAX kart_navigation_GpsFixStatus_DIFFERENTIAL_FIX
#define _kart_navigation_GpsFixStatus_ARRAYSIZE ((kart_navigation_GpsFixStatus)(kart_navigation_GpsFixStatus_DIFFERENTIAL_FIX+1))


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
