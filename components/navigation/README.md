# Navigation Component (`components/navigation`)

## Overview

This component manages navigation-related sensors for the go-kart platform. It reads data from various sensors, processes it, and publishes the relevant information (like position, orientation, speed, environment data) onto the CAN bus using the defined protobuf messages (`navigation.proto`).

Currently managed sensors:
*   **AHT21:** Temperature and Relative Humidity sensor (I2C)
*   **GY-521 (MPU-6050):** 6-DoF Inertial Measurement Unit (Accelerometer + Gyroscope) (I2C)
*   **ATGM336H:** GPS/BDS GNSS Module (UART)

## Pin Mapping (ESP32 - 38-pin Dev Kit)

This table outlines the pin connections based on `include/Config.h` and common defaults for standard ESP32 modules (e.g., ESP32-WROOM-32 on a 38-pin board). **Verify ALL pins against your specific ESP32 board layout and ensure no conflicts exist.** The CAN pins now match the motor component's test configuration.

| Function              | Pin Name (`Config.h`) | ESP32 GPIO    | Notes                                                                    |
| --------------------- | --------------------- | ------------- | ------------------------------------------------------------------------ |
| **MCP2515 CAN**       |                       |               |                                                                          |
| Chip Select (CS)      | `CAN_CS_PIN`          | 5             | Configured Chip Select (Strapping Pin - Must be HIGH at boot).           |
| Interrupt (INT)     | `CAN_INT_PIN`         | 4             | Configured Interrupt pin (Matches Motors Test - Changed from 17).        |
| SPI Clock (SCK)       | -                     | 18            | *Default VSPI SCK. Verify.*                                              |
| SPI MOSI (Master Out) | -                     | 23            | *Default VSPI MOSI (COPI). Verify.*                                      |
| SPI MISO (Master In)  | -                     | 19            | *Default VSPI MISO (CIPO). Verify.*                                      |
| Power (VCC)           | -                     | 3V3 / 5V      | Supply voltage for MCP2515 (check module specs).                         |
| Ground (GND)          | -                     | GND           | Common ground connection.                                                |
| **I2C Sensors**       |                       |               |                                                                          |
| I2C Data (SDA)        | `I2C_SDA_PIN`         | 21            | Configured I2C Data Line (AHT21, GY-521). Standard `Wire` SDA.           |
| I2C Clock (SCL)       | `I2C_SCL_PIN`         | 22            | Configured I2C Clock Line (AHT21, GY-521). Standard `Wire` SCL.          |
| **GPS Module**        |                       |               |                                                                          |
| GPS RX (ESP32 UART)   | `GPS_RX_PIN`          | 16            | Connect to ATGM336H **TX** pin. Uses `Serial2` RX (Standard Pin).        |
| GPS TX (ESP32 UART)   | `GPS_TX_PIN`          | 17            | Connect to ATGM336H **RX** pin. Uses `Serial2` TX (Standard Pin).        |
| **Sensor Power**      |                       |               |                                                                          |
| Power (VCC)           | -                     | 3V3           | Power for AHT21 (2.0-5.5V), GY-521 (3.3-5V typical), ATGM336H (2.7-3.6V). |
| Ground (GND)          | -                     | GND           | Common ground connection required for all components.                    |

**Important Notes:**
*   **GPIO 5 (CAN CS):** This is a **strapping pin** and *must* be HIGH during boot. Ensure the MCP2515 module doesn't have a pull-down on CS, or use a different non-strapping pin if boot problems occur.
*   **SPI Pins:** The SCK, MOSI, and MISO pins listed (18, 23, 19) are common defaults for the **VSPI** interface on standard ESP32 modules. You *must* verify these pins based on your specific board and confirm which SPI interface (`HSPI` or `VSPI`) the CAN library is configured to use.
*   **Serial2 Pins:** GPS now uses the default `Serial2` pins (RX=16, TX=17). Ensure these are available and not used by other peripherals.
*   **CAN INT Pin:** The CAN Interrupt is now on GPIO 4.
*   **I2C Pins:** The standard ESP32 `Wire` library defaults (SDA=21, SCL=22) match the configuration. No explicit `Wire.begin(SDA, SCL)` is typically needed unless using non-standard pins.
*   **Sensor Voltages:** Check the specific breakout board/module documentation for precise voltage requirements. While the listed ranges are common, some breakouts include regulators.
*   **Pin Conflicts:** Always double-check all pin assignments against your specific ESP32 board's documentation to ensure there are no conflicts between peripherals.

## Dependencies

This component requires the following libraries (ensure they are added to `platformio.ini`):

*   `Adafruit AHTX0`
*   `Adafruit MPU6050`
*   `Adafruit Unified Sensor`
*   `Adafruit BusIO`
*   `TinyGPS++`
*   MCP2515 CAN Driver (e.g., `mcp_can` by Cory Fowler)
*   `nanopb` (Protobuf generation and runtime library)
*   `ProtobufCANInterface` (Assumed custom library for CAN communication)
*   `SensorRegistry` & `Sensor` base class (Assumed custom common libraries)

## Configuration

Key hardware and communication settings are defined in `include/Config.h`:

*   `NODE_ID`: CAN bus Node ID for this component.
*   `CAN_CS_PIN`, `CAN_INT_PIN`: Pins for the MCP2515 CAN controller.
*   `CAN_BAUDRATE`: CAN bus speed.
*   `DEBUG_MODE`: Enable/disable `Serial` output.
*   `I2C_SDA_PIN`, `I2C_SCL_PIN`: Pins for I2C communication.
*   `GPS_RX_PIN`, `GPS_TX_PIN`: Pins used for the `Serial2` connection to the GPS module.

## Usage & Integration

*   The `main.cpp` file initializes the CAN interface, sensor registry, and all sensor variant objects.
*   Each sensor variant (e.g., `AHT21Sensor`) corresponds to a specific data point reported over CAN (e.g., `TEMPERATURE_AMBIENT`).
*   The `SensorRegistry` handles polling sensors based on their `updateInterval` and sending updated values over CAN via the `ProtobufCANInterface`.
*   The `loop()` function calls `canInterface.process()`, `gpsFixSensor->processSerial()` (to feed data to TinyGPS++), and `sensorRegistry.process()`.

## CAN Bus Communication

### Published Messages (STATUS)

This component publishes sensor readings periodically via the `SensorRegistry`. Each reading is sent as a `STATUS` message with:
*   `component_type`: `NAVIGATION`
*   `component_id`: Specific sensor source (e.g., `IMU_PRIMARY`, `GPS_PRIMARY`, `ENVIRONMENT_PRIMARY`)
*   `command_id`: Specific data type (e.g., `ACCEL_X`, `LATITUDE`, `TEMPERATURE_AMBIENT`)
*   `value_type`: Appropriate type (e.g., `FLOAT`, `UINT8`, `INT16`)
*   `value`: The sensor reading, encoded according to the `value_type`.

### Handled Messages (COMMAND)

The component listens for `COMMAND` messages to configure sensor settings:

| Target Component ID | Command ID                 | Description                     | Value Type Expected | Value Enum (`navigation.proto`) | Notes                                                      |
| ------------------- | -------------------------- | ------------------------------- | ------------------- | ------------------------------- | ---------------------------------------------------------- |
| `IMU_PRIMARY`       | `ACCELEROMETER_RANGE`      | Set IMU Accel Range             | UINT8               | `AccelerometerRangeValue`     | e.g., 0 = ±2g, 1 = ±4g, 2 = ±8g, 3 = ±16g                   |
| `IMU_PRIMARY`       | `GYROSCOPE_RANGE`          | Set IMU Gyro Range              | UINT8               | `GyroscopeRangeValue`         | e.g., 0=±250°/s, 1=±500°/s, 2=±1000°/s, 3=±2000°/s         |
| `IMU_PRIMARY`       | `FILTER_BANDWIDTH`         | Set IMU DLPF Bandwidth          | UINT8               | `FilterBandwidthValue`        | e.g., 0=260Hz, 1=184Hz, ..., 6=5Hz                         |
| `IMU_PRIMARY`       | `TRIGGER_CALIBRATION`      | Trigger IMU Calibration         | (Ignored)           | -                               | Exact behavior depends on implementation (currently logs). |
| `GPS_PRIMARY`       | `GPS_UPDATE_RATE`          | Set GPS Update Rate             | UINT8               | `GpsUpdateRateValue`          | e.g., 1=1Hz, 5=5Hz, 10=10Hz                                |
| `GPS_PRIMARY`       | `GNSS_CONSTELLATION`       | Set GNSS Constellation Mode     | UINT8               | `GnssConstellationValue`    | Placeholder - Exact commands not implemented.              |
| `GPS_PRIMARY`       | `NMEA_OUTPUT_CONFIG`       | Set NMEA Sentence Output        | UINT8               | `NmeaOutputValue`             | Placeholder - Exact commands not implemented.              |
| `GPS_PRIMARY`       | `STATIC_NAVIGATION_MODE`   | Set Static Navigation Mode      | UINT8               | `StaticNavigationValue`       | 0=Off, 1=On                                                |

The component will respond with an `ACK` message upon successful handling or a `NACK` message on failure (e.g., invalid value, sensor not initialized). 