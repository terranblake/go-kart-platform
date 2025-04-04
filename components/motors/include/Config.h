#ifndef CONFIG_H
#define CONFIG_H

// Debug mode - enable for serial output
#define DEBUG_ENABLED 1

// CAN interface
#define CAN_CS_PIN 10
#define CAN_INT_PIN 2
#define CAN_SPEED 500E3
#define NODE_ID 0x20  // Node ID for this motor controller

// Motor Controller Pins - Kunray MY1020 3kW BLDC Controller
// Output pins (signals from Arduino to controller)
#define THROTTLE_PIN 5        // PWM output for throttle control
#define DIRECTION_PIN 6       // Reverse wire: HIGH = forward, LOW = reverse

// There are 3 speed modes, controlled by a single 3-speed connector (L M H)
// We use 2 pins to control the 3 speeds through a simple circuit
#define SPEED_MODE_PIN_1 8    // Speed mode selection pin 1
#define SPEED_MODE_PIN_2 9    // Speed mode selection pin 2

// Brake control pins (2 levels available on the controller)
#define LOW_BRAKE_PIN 7       // Low level brake signal
#define HIGH_BRAKE_PIN A0     // High level brake signal

// Key/Safety features
#define E_LOCK_PIN A1         // E-Lock electronic key signal (safety)
#define HARD_BOOT_PIN A2      // Hard boot/reset functionality

// Cruise control
#define CRUISE_PIN A3         // Cruise control activation

// Hall sensor inputs (feedback from motor)
#define HALL_SENSOR_1 A4      // Hall sensor 1 input
#define HALL_SENSOR_2 A5      // Hall sensor 2 input
#define HALL_SENSOR_3 3       // Hall sensor 3 input

// Motor speed constants
#define MOTOR_MIN_SPEED 0
#define MOTOR_MAX_SPEED 255   // Max value for 8-bit PWM

// Status update interval (ms)
#define STATUS_INTERVAL 500

// Motor state defaults
#define DEFAULT_SPEED 0       // Initial speed (0-255)
#define DEFAULT_DIRECTION kart_motors_MotorDirectionValue_FORWARD
#define DEFAULT_MODE kart_motors_MotorModeValue_LOW

#endif // CONFIG_H
