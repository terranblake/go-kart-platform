#ifndef NAVIGATION_CONFIG_H
#define NAVIGATION_CONFIG_H

#include <Arduino.h>

// Define the Node ID for this component on the CAN bus
// #define NODE_ID kart_common_ComponentType_NAVIGATION // Defined via platformio.ini build_flags instead

// CAN bus configuration
#define CAN_CS_PIN 5   // Chip Select for MCP2515 (Matches Motors Test)
#define CAN_INT_PIN 4 // Interrupt pin for MCP2515 (Matches Motors Test - Changed from 17)
#define CAN_BAUDRATE CAN_1000KBPS // Set CAN speed

// Debug settings
#define DEBUG_MODE 1 // Set to 1 to enable Serial output, 0 to disable

// Sensor Pin Configuration
// I2C pins (for AHT21, GY-521) - Assuming standard ESP32 I2C pins
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// UART pins (for ATGM336H GPS)
// Using Serial2 defaults (GPIO 16=RX2, GPIO 17=TX2).
#define GPS_RX_PIN 16 // Connect GPS TX to ESP32 RX2 (GPIO 16)
#define GPS_TX_PIN 17 // Connect GPS RX to ESP32 TX2 (GPIO 17 - Default Serial2 TX)

#endif // NAVIGATION_CONFIG_H
