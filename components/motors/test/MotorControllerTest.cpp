#include <Arduino.h>
#include <unity.h>
#include "ProtobufCANInterface.h"
#include "../include/Config.h"
#include "common.pb.h"
#include "motors.pb.h"

// Mock for testing without actual hardware
#define MOCK_HARDWARE_FOR_TESTING

// Global variables for testing
ProtobufCANInterface* canInterface;
volatile unsigned int lastThrottleValue = 0;
volatile bool lastDirectionValue = true; // true = forward, false = reverse
volatile uint8_t lastSpeedModeValue = 0; // 0 = OFF, 1 = LOW, 2 = HIGH
volatile bool lastLowBrakeValue = false;
volatile bool lastHighBrakeValue = false;
volatile unsigned int currentRpm = 0;
volatile uint8_t errorState = 0;

// Mock functions that will be replaced by actual implementation
void mockSetThrottle(uint8_t value) {
  lastThrottleValue = value;
}

void mockSetDirection(bool forward) {
  lastDirectionValue = forward;
}

void mockSetSpeedMode(uint8_t mode) {
  lastSpeedModeValue = mode;
}

void mockSetLowBrake(bool engaged) {
  lastLowBrakeValue = engaged;
}

void mockSetHighBrake(bool engaged) {
  lastHighBrakeValue = engaged;
}

// External functions that will be implemented in the actual code
extern void handleSpeedCommand(kart_common_MessageType msg_type,
                          kart_common_ComponentType comp_type,
                          uint8_t component_id,
                          uint8_t command_id,
                          kart_common_ValueType value_type,
                          int32_t value);

extern void handleDirectionCommand(kart_common_MessageType msg_type,
                             kart_common_ComponentType comp_type,
                             uint8_t component_id,
                             uint8_t command_id,
                             kart_common_ValueType value_type,
                             int32_t value);

extern void handleBrakeCommand(kart_common_MessageType msg_type,
                          kart_common_ComponentType comp_type,
                          uint8_t component_id,
                          uint8_t command_id,
                          kart_common_ValueType value_type,
                          int32_t value);

extern void handleModeCommand(kart_common_MessageType msg_type,
                         kart_common_ComponentType comp_type,
                         uint8_t component_id,
                         uint8_t command_id,
                         kart_common_ValueType value_type,
                         int32_t value);

extern void handleEmergencyCommand(kart_common_MessageType msg_type,
                              kart_common_ComponentType comp_type,
                              uint8_t component_id,
                              uint8_t command_id,
                              kart_common_ValueType value_type,
                              int32_t value);

extern void handleStatusCommand(kart_common_MessageType msg_type,
                           kart_common_ComponentType comp_type,
                           uint8_t component_id,
                           uint8_t command_id,
                           kart_common_ValueType value_type,
                           int32_t value);

// These functions will be called by the handlers in our implementation
// We provide mock versions for testing
extern void setThrottle(uint8_t value) {
  mockSetThrottle(value);
}

extern void setDirection(bool forward) {
  mockSetDirection(forward);
}

extern void setSpeedMode(uint8_t mode) {
  mockSetSpeedMode(mode);
}

extern void setLowBrake(bool engaged) {
  mockSetLowBrake(engaged);
}

extern void setHighBrake(bool engaged) {
  mockSetHighBrake(engaged);
}

extern unsigned int calculateRPM() {
  return currentRpm; // Mock value for testing
}

void setUp(void) {
  // Initialize for each test
  canInterface = new ProtobufCANInterface(NODE_ID);
  lastThrottleValue = 0;
  lastDirectionValue = true;
  lastSpeedModeValue = 0;
  lastLowBrakeValue = false;
  lastHighBrakeValue = false;
  currentRpm = 0;
  errorState = 0;
}

void tearDown(void) {
  // Clean up after each test
  delete canInterface;
}

// Test Cases

void test_speed_command(void) {
  // Test normal throttle value 
  handleSpeedCommand(kart_common_MessageType_COMMAND,
                kart_common_ComponentType_MOTORS,
                kart_motors_MotorComponentId_MAIN_DRIVE,
                kart_motors_MotorCommandId_SPEED,
                kart_common_ValueType_UINT8,
                100);
  
  // Should map to 100 out of 255 throttle
  TEST_ASSERT_EQUAL(100, lastThrottleValue);
  
  // Test maximum allowed value 
  handleSpeedCommand(kart_common_MessageType_COMMAND,
                kart_common_ComponentType_MOTORS,
                kart_motors_MotorComponentId_MAIN_DRIVE,
                kart_motors_MotorCommandId_SPEED,
                kart_common_ValueType_UINT8,
                255);
  
  TEST_ASSERT_EQUAL(255, lastThrottleValue);
  
  // Test minimum value - should actually enforce MIN_THROTTLE
  handleSpeedCommand(kart_common_MessageType_COMMAND,
                kart_common_ComponentType_MOTORS,
                kart_motors_MotorComponentId_MAIN_DRIVE,
                kart_motors_MotorCommandId_SPEED,
                kart_common_ValueType_UINT8,
                0);
  
  TEST_ASSERT_EQUAL(0, lastThrottleValue);
  
  // Test value below minimum (invalid) - should default to minimum
  handleSpeedCommand(kart_common_MessageType_COMMAND,
                kart_common_ComponentType_MOTORS,
                kart_motors_MotorComponentId_MAIN_DRIVE,
                kart_motors_MotorCommandId_SPEED,
                kart_common_ValueType_UINT8,
                -50); // Invalid negative value
  
  // Should enforce 0 minimum
  TEST_ASSERT_EQUAL(0, lastThrottleValue);
}

void test_direction_command(void) {
  // Test forward direction
  handleDirectionCommand(kart_common_MessageType_COMMAND,
                   kart_common_ComponentType_MOTORS,
                   kart_motors_MotorComponentId_MAIN_DRIVE,
                   kart_motors_MotorCommandId_DIRECTION,
                   kart_common_ValueType_UINT8,
                   kart_motors_MotorDirectionValue_FORWARD);
  
  TEST_ASSERT_TRUE(lastDirectionValue);
  
  // Test reverse direction
  handleDirectionCommand(kart_common_MessageType_COMMAND,
                   kart_common_ComponentType_MOTORS,
                   kart_motors_MotorComponentId_MAIN_DRIVE,
                   kart_motors_MotorCommandId_DIRECTION,
                   kart_common_ValueType_UINT8,
                   kart_motors_MotorDirectionValue_REVERSE);
  
  TEST_ASSERT_FALSE(lastDirectionValue);
  
  // Test invalid direction value - should default to forward
  handleDirectionCommand(kart_common_MessageType_COMMAND,
                   kart_common_ComponentType_MOTORS,
                   kart_motors_MotorComponentId_MAIN_DRIVE,
                   kart_motors_MotorCommandId_DIRECTION,
                   kart_common_ValueType_UINT8,
                   99); // Invalid direction value
  
  // Should stay with the last valid value
  TEST_ASSERT_FALSE(lastDirectionValue);
}

void test_brake_command(void) {
  // Test brake enabled
  handleBrakeCommand(kart_common_MessageType_COMMAND,
                kart_common_ComponentType_MOTORS,
                kart_motors_MotorComponentId_MAIN_DRIVE,
                kart_motors_MotorCommandId_BRAKE,
                kart_common_ValueType_UINT8,
                1); // Enable brake
  
  TEST_ASSERT_TRUE(lastLowBrakeValue);
  TEST_ASSERT_FALSE(lastHighBrakeValue); // High brake should not be affected
  
  // Test brake disabled
  handleBrakeCommand(kart_common_MessageType_COMMAND,
                kart_common_ComponentType_MOTORS,
                kart_motors_MotorComponentId_MAIN_DRIVE,
                kart_motors_MotorCommandId_BRAKE,
                kart_common_ValueType_UINT8,
                0); // Disable brake
  
  TEST_ASSERT_FALSE(lastLowBrakeValue);
  TEST_ASSERT_FALSE(lastHighBrakeValue);
  
  // Test high brake value
  handleBrakeCommand(kart_common_MessageType_COMMAND,
                kart_common_ComponentType_MOTORS,
                kart_motors_MotorComponentId_MAIN_DRIVE,
                kart_motors_MotorCommandId_BRAKE,
                kart_common_ValueType_UINT8,
                2); // High brake
  
  TEST_ASSERT_FALSE(lastLowBrakeValue);
  TEST_ASSERT_TRUE(lastHighBrakeValue);
  
  // Test both brakes engaged
  handleBrakeCommand(kart_common_MessageType_COMMAND,
                kart_common_ComponentType_MOTORS,
                kart_motors_MotorComponentId_MAIN_DRIVE,
                kart_motors_MotorCommandId_BRAKE,
                kart_common_ValueType_UINT8,
                3); // Both brakes (value 3 = 0b11)
  
  TEST_ASSERT_TRUE(lastLowBrakeValue);
  TEST_ASSERT_TRUE(lastHighBrakeValue);
}

void test_speed_mode_command(void) {
  // Test LOW speed mode
  handleModeCommand(kart_common_MessageType_COMMAND,
               kart_common_ComponentType_MOTORS,
               kart_motors_MotorComponentId_MAIN_DRIVE,
               kart_motors_MotorCommandId_MODE,
               kart_common_ValueType_UINT8,
               kart_motors_MotorModeValue_LOW);
  
  TEST_ASSERT_EQUAL(1, lastSpeedModeValue); // 1 = LOW
  
  // Test MEDIUM speed mode - should map to HIGH (2) for this controller
  handleModeCommand(kart_common_MessageType_COMMAND,
               kart_common_ComponentType_MOTORS,
               kart_motors_MotorComponentId_MAIN_DRIVE,
               kart_motors_MotorCommandId_MODE,
               kart_common_ValueType_UINT8,
               kart_motors_MotorModeValue_MEDIUM);
  
  TEST_ASSERT_EQUAL(2, lastSpeedModeValue); // 2 = HIGH
  
  // Test HIGH speed mode
  handleModeCommand(kart_common_MessageType_COMMAND,
               kart_common_ComponentType_MOTORS,
               kart_motors_MotorComponentId_MAIN_DRIVE,
               kart_motors_MotorCommandId_MODE,
               kart_common_ValueType_UINT8,
               kart_motors_MotorModeValue_HIGH);
  
  TEST_ASSERT_EQUAL(2, lastSpeedModeValue); // 2 = HIGH
  
  // Test OFF mode (set to 0)
  handleModeCommand(kart_common_MessageType_COMMAND,
               kart_common_ComponentType_MOTORS,
               kart_motors_MotorComponentId_MAIN_DRIVE,
               kart_motors_MotorCommandId_MODE,
               kart_common_ValueType_UINT8,
               99); // Invalid mode, should turn off
  
  TEST_ASSERT_EQUAL(0, lastSpeedModeValue); // 0 = OFF
}

void test_emergency_command(void) {
  // Set up with motor running
  lastThrottleValue = 100;
  lastLowBrakeValue = false;
  lastHighBrakeValue = false;
  
  // Test emergency stop
  handleEmergencyCommand(kart_common_MessageType_COMMAND,
                    kart_common_ComponentType_MOTORS,
                    kart_motors_MotorComponentId_MAIN_DRIVE,
                    kart_motors_MotorCommandId_EMERGENCY,
                    kart_common_ValueType_UINT8,
                    kart_motors_MotorEmergencyValue_STOP);
  
  // Should stop motor and engage brakes
  TEST_ASSERT_EQUAL(0, lastThrottleValue);
  TEST_ASSERT_TRUE(lastLowBrakeValue);
  
  // Reset
  lastThrottleValue = 100;
  lastLowBrakeValue = false;
  lastHighBrakeValue = false;
  
  // Test shutdown
  handleEmergencyCommand(kart_common_MessageType_COMMAND,
                    kart_common_ComponentType_MOTORS,
                    kart_motors_MotorComponentId_MAIN_DRIVE,
                    kart_motors_MotorCommandId_EMERGENCY,
                    kart_common_ValueType_UINT8,
                    kart_motors_MotorEmergencyValue_SHUTDOWN);
  
  // Should stop motor, engage all brakes, and disable motor
  TEST_ASSERT_EQUAL(0, lastThrottleValue);
  TEST_ASSERT_TRUE(lastLowBrakeValue);
  TEST_ASSERT_TRUE(lastHighBrakeValue);
}

void test_status_command(void) {
  // Mock RPM value for testing
  currentRpm = 1500;
  
  // This should trigger sending a status update
  // We can't easily test the CAN sending, so we'll just verify it doesn't crash
  handleStatusCommand(kart_common_MessageType_COMMAND,
                 kart_common_ComponentType_MOTORS,
                 kart_motors_MotorComponentId_MAIN_DRIVE,
                 kart_motors_MotorCommandId_STATUS,
                 kart_common_ValueType_UINT8,
                 1);
  
  // Not much to test here without mocking the CAN interface further
  TEST_ASSERT_TRUE(true);
}

// Run all tests
int main() {
  UNITY_BEGIN();
  
  RUN_TEST(test_speed_command);
  RUN_TEST(test_direction_command);
  RUN_TEST(test_brake_command);
  RUN_TEST(test_speed_mode_command);
  RUN_TEST(test_emergency_command);
  RUN_TEST(test_status_command);
  
  return UNITY_END();
} 