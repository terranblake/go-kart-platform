# Motor Controller Component

This component interfaces with the Kunray MY1020 3kW BLDC motor controller via an ESP32, handling throttle, direction, braking, speed modes, and sensor readings (RPM, Temperatures via ADS1115).

## Features

*   Controls throttle (PWM/DAC), direction (Forward/Reverse/Neutral), and 2-level braking.
*   Supports 3 speed modes (Off, Low, High).
*   Receives commands and sends status updates over CAN bus using Protobuf messages.
*   Reads motor RPM using Hall sensors.
*   Reads Battery and Motor temperatures using NTC thermistors connected to an external ADS1115 ADC over I2C for improved noise immunity.
*   Handles Emergency Stop/Shutdown commands.
*   Provides serial debugging interface (`DEBUG_MODE=1`).

## Hardware

*   ESP32 Development Board
*   Kunray MY1020 3kW BLDC Motor Controller
*   MCP2515 CAN Transceiver Module
*   ADS1115 16-Bit ADC Module (I2C)
*   NTC Thermistors (e.g., NTCLE100E3xxxx) with 10kΩ series resistors for temperature sensing.
*   Appropriate wiring, connectors, and power supply.

## Wiring

Refer to `components/motors/include/Config.h` for specific pin assignments.

**Key Connections:**

*   **CAN:** ESP32 SPI pins (MOSI, MISO, SCK) connect to MCP2515 SPI pins. `CAN_CS_PIN` (GPIO 5) connects to MCP2515 CS. `CAN_INT_PIN` (GPIO 4) connects to MCP2515 INT.
*   **Motor Controller Signals:** Connect ESP32 pins (`THROTTLE_PIN`, `DIRECTION_PIN`, `SPEED_MODE_PIN_1`, `SPEED_MODE_PIN_2`, `LOW_BRAKE_PIN`, `HIGH_BRAKE_PIN`) to the corresponding inputs on the Kunray controller.
*   **Hall Sensors:** Connect motor Hall sensor outputs (A, B, C) to ESP32 input pins (`HALL_A_PIN`, `HALL_B_PIN`, `HALL_C_PIN`).
*   **I2C (ADS1115):**
    *   ESP32 `I2C_SDA_PIN` (GPIO 21) <-> ADS1115 SDA
    *   ESP32 `I2C_SCL_PIN` (GPIO 22) <-> ADS1115 SCL
    *   ESP32 `3.3V` <-> ADS1115 VDD (Verify module voltage!)
    *   ESP32 `GND` <-> ADS1115 GND
    *   ADS1115 `ADDR` pin connected to `GND` (for address 0x48).
*   **Thermistors (Connected via ADS1115):**
    *   Each thermistor forms a voltage divider with a 10kΩ series resistor (connected to 3.3V).
    *   The midpoint of the Battery thermistor divider connects to ADS1115 `A0`.
    *   The midpoint of the Motor thermistor divider connects to ADS1115 `A1`.
    *   (Optional: Controller thermistor divider midpoint to ADS1115 `A2`).

**ASCII Wiring Diagram (ESP32, ADS1115, Thermistors):**

```ascii
   ESP32 Dev Board                     ADS1115 Module
+---------------------+             +--------------------+
|                     |             |                    |
| 3.3V  -----------(Vin)----------->| VDD                |
| GND   -----------(GND)----------->| GND                |
| GPIO 21 (SDA) ----(I2C SDA)------>| SDA                |
| GPIO 22 (SCL) ----(I2C SCL)------>| SCL                |
|                     |             | ADDR -------> GND  |
|                     |             |                    |
|                     |             | A0  <-----.        |
|                     |             | A1  <-----.|-----. |
|                     |             | A2        | |     |
|                     |             | A3        | |     |
+---------------------+             +-----------| |-----+
                                                | |
       +3.3V                                    | |
         |                                      | |
        [R] 10k (Series)                        | |
         |                                      | |
         +----(Batt Thermistor Divider Midpoint)-+ |
         |                                        |
        [T] Batt Thermistor                       |
         |                                        |
        GND                                       |
                                                  |
       +3.3V                                      |
         |                                        |
        [R] 10k (Series)                          |
         |                                        |
         +----(Motor Thermistor Divider Midpoint)-+
         |
        [T] Motor Thermistor
         |
        GND

(Note: Other ESP32 pins connect to motor controller, CAN module, Hall sensors as per Config.h)
```

## Software Dependencies

*   `autowp/autowp-mcp2515` (for CAN communication via MCP2515)
*   `adafruit/Adafruit ADS1X15` (for interfacing with the ADS1115 ADC)
*   `adafruit/Adafruit BusIO` (Dependency for ADS1X15 library)
*   Nanopb (included via project structure for Protobuf serialization)
*   Project-specific Protobuf message definitions (common.pb, motors.pb, batteries.pb)
*   Common Sensor Framework (`Sensor.h`, `SensorRegistry.h`, `AnalogReader.h`, etc.)

(See `components/motors/platformio.ini` for specific versions and library management)

## Building and Uploading

Use PlatformIO:

```bash
# Compile debug environment
platformio run -d components/motors -e debug

# Upload and monitor debug environment
platformio run -d components/motors -e debug --target upload --target monitor
```

## Communication Protocol

*   **Listens for:** `COMMAND` messages targeting `ComponentType: MOTORS` and `ComponentId: MOTOR_LEFT_REAR` (or `ALL` for broadcast like Emergency Stop).
    *   `CommandId: THROTTLE` (Value: 0-255)
    *   `CommandId: DIRECTION` (Value: `kart_motors_MotorDirectionValue` enum)
    *   `CommandId: BRAKE` (Value: `kart_motors_MotorBrakeValue` enum)
    *   `CommandId: MODE` (Value: `kart_motors_MotorModeValue` enum)
    *   `CommandId: EMERGENCY` (Value: `kart_motors_MotorEmergencyValue` enum)
*   **Sends:** `STATUS` messages.
    *   From `ComponentType: MOTORS`, `ComponentId: MOTOR_LEFT_REAR`:
        *   `CommandId: RPM` (Value: Motor RPM, UINT16)
        *   `CommandId: TEMPERATURE` (Value: Motor Temp in tenths °C, UINT16)
    *   From `ComponentType: BATTERIES`, `ComponentId: MOTOR_LEFT_REAR`:
        *   `CommandId: TEMPERATURE` (Value: Battery Temp for this motor's pack, tenths °C, UINT16)

(Refer to `.proto` files and `ProtobufCANInterface.h` for details)

## TODO

*   Implement Controller Temperature sensing (assign ADS1115 channel, uncomment code).
*   Add handling for other controller signals if needed (E-Lock, Cruise).
*   Consider adding NVS storage for calibration/configuration values. 