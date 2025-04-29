# Controls Component

## Overview
This component is responsible for reading physical driver inputs and translating them into CAN messages for other components to act upon. It handles inputs such as:

- Throttle Pedal Position
- Brake Pedal Position (Placeholder)
- Turn Signal Switch State
- Hazard Light Switch State
- Headlight Switch State
- Steering Wheel Position (Placeholder/Optional)

## Hardware
- Microcontroller: ESP32 (Assumed)
- CAN Transceiver: MCP2515 (Assumed)
- Inputs:
  - Analog Hall effect sensors or potentiometers for throttle, brake, steering.
  - Digital switches/buttons for turn signals, hazards, lights.

## Software
- Reads analog inputs using `InternalADCReader`.
- Reads digital inputs using `DigitalInputSensor`.
- Maps analog inputs to standardized ranges (e.g., 0-1024) using `MappedAnalogSensor`.
- Uses `SensorRegistry` to manage sensor readings and periodic CAN updates.
- Publishes input states via CAN using `ProtobufCANInterface` according to the `controls.proto` definition.

## CAN Interface
- **Component Type:** `CONTROLS` (3)
- **Component IDs:** `THROTTLE` (1), `BRAKE` (2), `STEERING` (3), `TURN_SIGNAL_SWITCH` (4), `HAZARD_SWITCH` (5), `LIGHTS_SWITCH` (6)
- **Commands Sent:** `POSITION` (10), `STATE` (11), `MODE` (3)
- **Commands Received:** Potentially `CALIBRATE` (6) or `ENABLE`/`DISABLE` (0/1).

## Setup & Wiring
- Connect analog sensors to the ADC pins defined in `include/Config.h`.
- Connect digital switches to the GPIO pins defined in `include/Config.h` (ensure appropriate pull-ups or pull-downs are used based on switch type and `pinMode` setting).
- Connect MCP2515 module to the SPI pins defined in `include/Config.h`.

## Building & Flashing
```bash
cd components/controls
platformio run -e debug -t upload
```

## Serial Debug Commands (`DEBUG_MODE=1`)
- `STATUS`: Show current raw readings from sensors.
- `HELP`: Display available commands.
- `T:[0-1024]`: Simulate sending a throttle position STATUS message.
- `B:[0-1024]`: Simulate sending a brake position STATUS message.
- `TS:[0|L|R]`: Simulate sending a turn signal state STATUS message.
- `HZ:[0|1]`: Simulate sending a hazard switch state STATUS message.
- `L:[0-3]`: Simulate sending a lights switch mode STATUS message. 