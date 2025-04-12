#ifndef CONFIG_H
#define CONFIG_H

// CAN interface
#ifndef NODE_ID
#define NODE_ID 0x20  // Node ID for this motor controller
#endif


// CAN speed
#define CAN_SPEED CAN_500KBPS

// Motor Controller Pins - Kunray MY1020 3kW BLDC Controller
// Output pins (signals from Arduino to controller)
#define THROTTLE_PIN 5        // PWM output for throttle control
#define DIRECTION_PIN 6       // Reverse wire: HIGH = forward, LOW = reverse

#define MIN_THROTTLE 40
#define MAX_THROTTLE 200

// There are 3 speed modes, controlled by a single 3-speed connector (L M H)
// We use 2 pins to control the 3 speeds through a simple circuit
#define SPEED_MODE_PIN_1 A1    // Speed mode selection pin 1 - WHITE
#define SPEED_MODE_PIN_2 A2    // Speed mode selection pin 2 - BLUE

// Brake control pins (2 levels available on the controller)
#define LOW_BRAKE_PIN A4       // Low level brake signal
#define HIGH_BRAKE_PIN A3     // High level brake signal

// Key/Safety features
#define E_LOCK_PIN -1         // E-Lock electronic key signal (safety)
#define HARD_BOOT_PIN -1      // Hard boot/reset functionality

// Cruise control
#define CRUISE_PIN -1         // Cruise control activation

// Hall sensor inputs (feedback from motor)
#define HALL_A_PIN 7          // Hall sensor A (reading only; had to move from pin 2 due to conflict with CAN0 RX)
#define HALL_B_PIN 3          // Hall sensor B (interrupt pin)
#define HALL_C_PIN 4          // Hall sensor C (reading only)

// Temperature sensor pins
#define TEMP_SENSOR_BATTERY A5   // Battery temperature sensor (A6 analog input)
#define TEMP_SENSOR_CONTROLLER A6 // Controller temperature sensor (A7 analog input)
#define TEMP_SENSOR_MOTOR A7     // Motor temperature sensor (digital pin with analog capabilities)

// NTC Thermistor parameters for NTCLE100E3203JBD
#define THERMISTOR_NOMINAL 10000   // Resistance at 25°C
#define TEMPERATURE_NOMINAL 25     // Temperature for nominal resistance (°C)
#define B_COEFFICIENT 3977         // Beta coefficient from datasheet
#define SERIES_RESISTOR 10000      // Value of the series resistor

// Motor speed constants
#define MOTOR_MIN_SPEED 0
#define MOTOR_MAX_SPEED 255   // Max value for 8-bit PWM

// Status update interval (ms)
#define STATUS_INTERVAL 500

// RPM update interval (ms)
#define RPM_UPDATE_INTERVAL 500

// Motor state defaults
#define DEFAULT_SPEED 0       // Initial speed (0-255)
#define DEFAULT_DIRECTION kart_motors_MotorDirectionValue_FORWARD
#define DEFAULT_MODE kart_motors_MotorModeValue_LOW

// Global state variables declarations (these are defined in main.cpp)
extern bool currentLowBrake;
extern bool currentHighBrake;

// Use transistors for brake control (vs relays)
#define USING_TRANSISTOR

// Function prototypes
void setupPins();
void setThrottle(uint8_t level);
void setDirection(bool forward);
void setSpeedMode(uint8_t mode);
void setLowBrake(bool engaged);
void setHighBrake(bool engaged);
void allStop();
void hallSensorA_ISR();
void hallSensorB_ISR();
void hallSensorC_ISR();
void updateHallReadings();
void sendStatusUpdate();
void emergencyStop();
void emergencyShutdown();

// Command handlers
void handleSpeedCommand(kart_common_MessageType msg_type,
                       kart_common_ComponentType comp_type,
                       uint8_t component_id,
                       uint8_t command_id,
                       kart_common_ValueType value_type,
                       int32_t value);

void handleDirectionCommand(kart_common_MessageType msg_type,
                           kart_common_ComponentType comp_type,
                           uint8_t component_id,
                           uint8_t command_id,
                           kart_common_ValueType value_type,
                           int32_t value);

void handleBrakeCommand(kart_common_MessageType msg_type,
                       kart_common_ComponentType comp_type,
                       uint8_t component_id,
                       uint8_t command_id,
                       kart_common_ValueType value_type,
                       int32_t value);

void handleModeCommand(kart_common_MessageType msg_type,
                      kart_common_ComponentType comp_type,
                      uint8_t component_id,
                      uint8_t command_id,
                      kart_common_ValueType value_type,
                      int32_t value);

void handleEmergencyCommand(kart_common_MessageType msg_type,
                           kart_common_ComponentType comp_type,
                           uint8_t component_id,
                           uint8_t command_id,
                           kart_common_ValueType value_type,
                           int32_t value);

void handleStatusCommand(kart_common_MessageType msg_type,
                        kart_common_ComponentType comp_type,
                        uint8_t component_id,
                        uint8_t command_id,
                        kart_common_ValueType value_type,
                        int32_t value);

#endif // CONFIG_H
