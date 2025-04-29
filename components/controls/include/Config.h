#ifndef CONTROLS_CONFIG_H
#define CONTROLS_CONFIG_H

#include <Arduino.h>

// --- Component Identification ---
#define NODE_ID 40 // Example CAN Node ID for Controls

// --- Hardware Pins (ESP32 Dev Module Example - ADJUST THESE!) ---

// Analog Inputs
#define THROTTLE_PIN 34   // Example ADC1_CH6
#define BRAKE_PIN    35   // Example ADC1_CH7
#define STEERING_PIN 32   // Example ADC1_CH4 (If using analog steering sensor)

// Digital Inputs (Assuming pull-ups might be needed, configure in setupPins)
#define TURN_SIGNAL_LEFT_PIN  13
#define TURN_SIGNAL_RIGHT_PIN 12
#define HAZARD_SWITCH_PIN     14
#define LIGHTS_MODE_OFF_PIN   27 // Example: One pin per mode if using separate wires
#define LIGHTS_MODE_ON_PIN    26 // Example: Or could be multi-position switch on one analog pin
#define LIGHTS_MODE_LOW_PIN   25 // Example
#define LIGHTS_MODE_HIGH_PIN  33 // Example

// Digital Outputs (If controls component drives anything directly)
// #define EXAMPLE_OUTPUT_PIN 15

// --- CAN Bus Configuration ---
// Assumes standard SPI pins for ESP32 with MCP2515
#define CAN_CS_PIN   5  // SPI CS
#define CAN_INT_PIN  17 // Interrupt pin (Optional, check library)
// SPI SCK = 18, MISO = 19, MOSI = 23 (Standard VSPI on ESP32 Dev Module)
#define CAN_BAUDRATE 500000 // 500 kbps

// --- ADC Configuration ---
// Reference voltage for internal ADC (ESP32: typically 3.3V, check board)
#define ADC_VREF_MV 3300 
// ADC Resolution (ESP32: 12-bit)
#define ADC_MAX_VALUE 4095 
// Attenuation for ADC pins (ESP32 specific) - affects voltage reading range
// Options: ADC_ATTEN_DB_0 (100mV ~ 950mV), ADC_ATTEN_DB_2_5 (100mV ~ 1250mV)
//          ADC_ATTEN_DB_6 (150mV ~ 1750mV), ADC_ATTEN_DB_12 (150mV ~ 2450mV)
#define THROTTLE_ATTEN ADC_ATTEN_DB_12 // Example: Allows up to ~2.45V input
#define BRAKE_ATTEN    ADC_ATTEN_DB_12 // Example
#define STEERING_ATTEN ADC_ATTEN_DB_12 // Example

// --- Sensor Configuration ---
#define THROTTLE_MIN_ADC 1000   // Calibrated min ADC value for throttle released
#define THROTTLE_MAX_ADC 3000  // Calibrated max ADC value for throttle fully pressed
#define BRAKE_MIN_ADC 100      // Example raw ADC value for brake released
#define BRAKE_MAX_ADC 4000     // Example raw ADC value for brake fully pressed

// --- Update Intervals (ms) ---
#define THROTTLE_UPDATE_INTERVAL 50
#define BRAKE_UPDATE_INTERVAL    50
#define STEERING_UPDATE_INTERVAL 100
#define SWITCH_UPDATE_INTERVAL   200 // How often to check digital inputs

#endif // CONTROLS_CONFIG_H
