#ifndef NAVIGATION_CONFIG_H
#define NAVIGATION_CONFIG_H

// Define the Node ID for this component on the CAN bus
// #define NODE_ID kart_common_ComponentType_NAVIGATION // Defined via platformio.ini build_flags instead

// Include Protobuf definitions
#include "common.pb.h"

// CAN bus configuration
#define CAN_CS_PIN 5   // Chip Select for MCP2515 (Matches Motors Test)
#define CAN_INT_PIN 4 // Interrupt pin for MCP2515 (Matches Motors Test - Changed from 17)
#define CAN_BAUDRATE 500000 // Set CAN speed (Reverted from CAN_1000KBPS)

// Debug settings
#define DEBUG_MODE 1 // Set to 1 to enable Serial output, 0 to disable
#define CAN_LOGGING_ENABLED 0 // Set to 1 to enable detailed CAN send/receive logs, 0 to disable

// Sensor Pin Configuration
// I2C pins (for AHT21, GY-521) - Assuming standard ESP32 I2C pins
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// UART pins (for ATGM336H GPS)
// Using Serial2 defaults (GPIO 16=RX2, GPIO 17=TX2).
#define GPS_RX_PIN 16 // Connect GPS TX to ESP32 RX2 (GPIO 16)
#define GPS_TX_PIN 17 // Connect GPS RX to ESP32 TX2 (GPIO 17 - Default Serial2 TX)

// Function Prototypes
void setupPins();
void setupSensors();
void handleImuConfigCommand(
    kart_common_MessageType msg_type,
    kart_common_ComponentType comp_type,
    uint8_t component_id,
    uint8_t command_id, kart_common_ValueType value_type, int32_t value);

void handleGpsConfigCommand(
    kart_common_MessageType msg_type,
    kart_common_ComponentType comp_type,
    uint8_t component_id,
    uint8_t command_id, kart_common_ValueType value_type, int32_t value);

#if DEBUG_MODE == 1
void parseSerialCommands();
#endif

#endif // NAVIGATION_CONFIG_H
