<!-- LLM_CONTEXT
component: lights
purpose: Control LED strips for vehicle lighting (headlights, tail lights, turn signals, brake lights)
hardware: FastLED-compatible LED strips connected to Arduino
protocol: Receives CAN commands for mode, turn signals, brake status
states: off, on, bright, hazard, with optional turn signals and brake activation
animations: startup/shutdown sequences, turn signal animations
-->

# Go-Kart Lights Component

## Overview
Arduino-based controller for all vehicle lighting functions using addressable LED strips.

## State Machine
<!-- LLM_CODE_MARKER: state_machine -->
```
┌──────┐   MODE=ON    ┌──────────┐
│ OFF  │─────────────►│ NORMAL   │
└──────┘              └────┬─────┘
   ▲                       │
   │                       │ MODE=BRIGHT
   │ MODE=OFF              ▼
┌──┴───────┐         ┌──────────┐
│ SHUTDOWN │◄────────┤ BRIGHT   │
└──────────┘ MODE=OFF└────┬─────┘
                          │
                          │ MODE=HAZARD
                          ▼
                     ┌──────────┐
                     │ HAZARD   │
                     └──────────┘
```
<!-- LLM_CODE_MARKER_END -->

## Commands Reference
<!-- LLM_CODE_MARKER: commands -->
| Command | Description | Parameter | Example |
|---------|-------------|-----------|---------|
| MODE | Set primary light mode | LightModeValue | `MODE=ON` |
| SIGNAL | Activate turn signals | LightSignalValue | `SIGNAL=LEFT` |
| BRAKE | Activate brake lights | Boolean | `BRAKE=1` |
| LOCATION | Set light location on vehicle | LightComponentId | `LOCATION=FRONT` |
<!-- LLM_CODE_MARKER_END -->

## Configuration Constants
<!-- LLM_CODE_MARKER: configuration -->
```cpp
// Pin definitions
#define DATA_PIN 6           // LED data pin

// Light configuration
#define NUM_LEDS 60          // Total LEDs in strip
#define DEFAULT_BRIGHTNESS 50 // Default brightness
#define BRIGHT_BRIGHTNESS 127 // High beam brightness
#define BRAKE_BRIGHTNESS 255  // Brake light brightness

// Turn signal configuration
#define TURN_SIGNAL_BLINK_RATE 500 // ms
#define TURN_SIGNAL_COUNT 10       // LEDs per turn signal
```
<!-- LLM_CODE_MARKER_END -->

## Protocol Message Mapping
<!-- LLM_CODE_MARKER: protocol_mapping -->
| Command | Component ID | Command ID | Value Type | Values |
|---------|--------------|------------|------------|--------|
| Light Mode | FRONT(0)/REAR(1)/ALL(255) | MODE(0) | UINT8 | OFF(0), ON(1), BRIGHT(4), HAZARD(8) |
| Turn Signal | ALL(255) | SIGNAL(1) | UINT8 | NONE(0), LEFT(1), RIGHT(2) |
| Brake Lights | REAR(1) | BRAKE(8) | BOOLEAN | 0 (off), 1 (on) |
| Set Location | * | LOCATION(9) | UINT8 | FRONT(0), REAR(1), INTERIOR(4), etc. |
<!-- LLM_CODE_MARKER_END -->

## Implementation Details

### Light Patterns
<!-- LLM_CODE_MARKER: light_patterns -->
- **Normal Mode**: Solid white front, solid red rear
- **Bright Mode**: Brighter white front lights
- **Hazard Mode**: All lights flash amber
- **Turn Signals**: Sequential amber animation on affected side
- **Brake Lights**: Full brightness red on rear lights
- **Startup Animation**: Expanding pattern from center
- **Shutdown Animation**: Contracting pattern to center
<!-- LLM_CODE_MARKER_END -->

### Handler Functions
<!-- LLM_API_BOUNDARY: Handler Functions -->
```cpp
// Mode handler (off, on, bright, hazard)
void handleLightMode(kart_common_MessageType msg_type,
                    kart_common_ComponentType comp_type,
                    uint8_t component_id,
                    uint8_t command_id,
                    kart_common_ValueType value_type,
                    int32_t value);

// Turn signal handler (none, left, right)
void handleLightSignal(kart_common_MessageType msg_type,
                      kart_common_ComponentType comp_type,
                      uint8_t component_id,
                      uint8_t command_id,
                      kart_common_ValueType value_type,
                      int32_t value);

// Brake light handler (on/off)
void handleLightBrake(kart_common_MessageType msg_type,
                     kart_common_ComponentType comp_type,
                     uint8_t component_id,
                     uint8_t command_id,
                     kart_common_ValueType value_type,
                     int32_t value);
```
<!-- LLM_API_END -->

## Usage Examples
<!-- LLM_CODE_MARKER: usage_examples -->
```cpp
// Turn on front lights
canInterface.sendMessage(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_LIGHTS,
    kart_lights_LightComponentId_FRONT,
    kart_lights_LightCommandId_MODE,
    kart_common_ValueType_UINT8,
    kart_lights_LightModeValue_ON);

// Activate left turn signal
canInterface.sendMessage(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_LIGHTS,
    kart_lights_LightComponentId_ALL,
    kart_lights_LightCommandId_SIGNAL,
    kart_common_ValueType_UINT8,
    kart_lights_LightSignalValue_LEFT);

// Activate brake lights
canInterface.sendMessage(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_LIGHTS,
    kart_lights_LightComponentId_REAR,
    kart_lights_LightCommandId_BRAKE,
    kart_common_ValueType_BOOLEAN,
    1); // Brake ON
```
<!-- LLM_CODE_MARKER_END -->

## Testing
Run the built-in test sequence to verify all light functions:
```cpp
// Activate test mode
canInterface.sendMessage(
    kart_common_MessageType_COMMAND,
    kart_common_ComponentType_CONTROLS,
    kart_controls_ControlComponentId_SYSTEM,
    kart_controls_ControlCommandId_MODE,
    kart_common_ValueType_UINT8,
    kart_controls_ControlModeValue_TEST);
``` 