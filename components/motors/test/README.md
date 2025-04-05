# Kunray MY1020 Controller Test Suite

This directory contains test scripts for verifying hardware connectivity and functionality with the Kunray MY1020 3kW BLDC motor controller.

## BasicControllerTest.cpp

This script is designed to test the most fundamental aspects of controller operation:
- Throttle control (variable speed)
- Direction control (forward/reverse)
- Speed mode selection (off/low/high)
- Motor RPM monitoring via hall sensors

### Prerequisites

- ESP8266 NodeMCU (recommended) or Arduino Nano
  - ESP8266 is strongly recommended as it supports interrupts on all pins except GPIO16
  - Arduino Nano only supports interrupts on pins 2 and 3, limiting hall sensor monitoring
- Proper wiring between microcontroller and Kunray controller (see wiring diagram below)
- Power supply for microcontroller and motor controller (48V-72V battery for controller)
- Motor connected to controller (or appropriate test load)
- Arduino IDE or PlatformIO for uploading the code
- Optional: Relay/transistor module for controlling the low brake
- Access to the motor's hall sensor wires (for RPM monitoring)

### Controller Characteristics

**Throttle Response Thresholds:**
- The motor does not respond until throttle value reaches ~75 (out of 255)
- At lower values (0-74), the controller does not activate the motor
- In HIGH speed mode, throttle response around 75 may be unstable/shaky
- For smooth operation, use 85+ for LOW speed, 100+ for HIGH speed
- The minimum throttle threshold applies to both forward and reverse directions

### Wiring Diagram

```
ESP8266 NodeMCU                Kunray MY1020 Controller
+-----------+                 +----------------------+
|           |                 |  THROTTLE CONNECTOR  |
| D1 (GPIO5)|---------------->| Green (Signal)       |
|           |                 | Red (5V from controller)|
|           |                 |                      |
|           |                 |  REVERSE CONNECTOR   |
| D2 (GPIO4)|---------------->| Blue (Signal)        |
|           |                 |                      |
|           |                 |  3-SPEED CONNECTOR   |
| D5 (GPIO14)|---------------->| White (Speed 1)      |
| D6 (GPIO12)|---------------->| Blue (Speed 2)       |
|           |                 |                      |
| D7 (GPIO13)|---------------->| High Brake: Orange (Signal)|
| D0 (GPIO16)|-----[Transistor]->| Low Brake: Yellow (via transistor)|
|           |                 |                      |
| D3 (GPIO0)|<----------------| Hall Sensor A        |
| D4 (GPIO2)|<----------------| Hall Sensor B        |
| D8 (GPIO15)|<----------------| Hall Sensor C       |
|           |                 |                      |
| GND       |-+-------------->| Black (GND) - Throttle   |
|           | |              >| Black (GND) - Reverse    |
|           | |              >| Black (GND) - 3-Speed    |
|           | +-------------->| Black (GND) - Hall Sensors |
+-----------+                 +----------------------+
                              
           COMMON GROUND CONNECTION

NOTE: Low Brake (Yellow wire) must remain DISCONNECTED for normal operation
OR use a transistor/relay circuit (see Low Brake Control section below)
```

**Important Power Notes:**
- DO NOT connect Arduino 5V to the throttle's red wire
- The controller supplies its own 5V reference through the red wire
- The throttle circuit receives power FROM the controller, not from the Arduino
- Connecting both power sources could create ground loops or damage components

**Important Ground Notes:**
- ALL black wires MUST connect to the same Arduino GND pin
- Using a breadboard or wire splitter to create a common ground point is recommended
- Proper ground connection is essential for signal integrity and safety

**Critical Brake Notes:**
- The Low Brake yellow wire must remain DISCONNECTED for normal operation
- Connecting this wire (even to LOW/GND) will prevent the motor from running 
- High Brake (orange) can be controlled normally via D7

### Hall Sensor Connections

The hall sensors provide feedback on motor rotation and speed. Connect them as follows:

1. Locate the hall sensor outputs from the motor or controller:
   - Most BLDC motors have 3 hall sensors (A, B, C)
   - Each sensor typically requires 5V power, GND, and provides a signal output
   - For ESP8266: Connect all three hall sensor signals to interrupt-capable pins (D3, D4, D8 recommended)
   - For Arduino Nano: Connect only Hall A and B to interrupt pins (D2, D3). Hall C will use polling.
   - Use a common ground connection

2. In the test code, hall sensor feedback is used to:
   - Calculate motor RPM in real-time
   - Confirm motor response to throttle changes
   - Verify brake functionality

3. If hall sensor connections are not accessible:
   - The test will still work but without RPM data
   - Comment out the hall sensor code sections if not using

**Important Note on Hall Sensor Accuracy:**
- Using the ESP8266 with interrupts on all three hall sensors provides more accurate RPM measurements, especially at high speeds
- Arduino Nano can only use interrupts on two sensors, which may result in less accurate readings at very high RPMs
- The code automatically handles both configurations

### Low Brake Control Using Transistor

Since the yellow wire needs to be physically disconnected for normal operation, you can use a transistor to control it:

```
ESP8266 NodeMCU         Transistor          Kunray Controller
+-----------+        +-------+           +----------------+
| D0 (GPIO16)|---[1K]--| Base |           |                |
|           |        |       |           |                |
|           |        | Coll. |-----------> Yellow (Low Brake) |
|           |        |       |           |                |
| GND       |--------| Emit. |           |                |
+-----------+        +-------+           +----------------+
```

With this setup:
- D0 HIGH = Transistor ON = Yellow wire connected to GND = Brake engaged
- D0 LOW = Transistor OFF = Yellow wire disconnected = Normal operation

### Speed Mode Configuration
The 3-speed connector has two signal wires (White and Blue) plus ground:

| D5 (GPIO14) | D6 (GPIO12) | Result    |
|------------|-----------|-----------|
| LOW        | LOW       | OFF       |
| HIGH       | LOW       | LOW Speed |
| LOW        | HIGH      | HIGH Speed|

Both D5 and D6 connect directly to their respective wires. Make sure all grounds are connected to a common point.

### Safety Precautions

⚠️ **IMPORTANT SAFETY WARNINGS** ⚠️

1. This test will cause the motor to run! Ensure motor is securely mounted and safe to operate.
2. Start with lower voltage if possible (e.g., 48V instead of 72V for initial testing).
3. Keep motor clear of obstructions and people during testing.
4. Have an emergency power cutoff ready at all times.
5. Modify the `MAX_THROTTLE` value in the code to limit maximum motor speed during testing.

### How to Use

1. Connect Arduino to the Kunray controller following the wiring diagram.
2. For the low brake (yellow wire) either:
   - Leave it completely DISCONNECTED, or
   - Connect it through a transistor as shown in the diagram above
3. Connect the hall sensor wires if available for RPM monitoring
4. Open the sketch in Arduino IDE or PlatformIO.
5. Set `USING_TRANSISTOR` or `USING_RELAY` based on your setup
6. Upload the code to the Arduino.
7. Open Serial Monitor at 115200 baud.
8. Follow the on-screen instructions to start the test sequence.
9. The test will automatically cycle through the following sequence while displaying RPM data:
   - Part 1: Gradual throttle increase in forward direction
   - Part 2: Testing OFF/LOW/HIGH speed modes
   - Part 3: Testing reverse direction
   - Part 4: Combined test of various settings
   - Part 5: Testing brakes (both low and high if connected)

### Adjusting Test Parameters

You can modify these constants at the top of the file to adjust the test behavior:

```cpp
#define STEP_DELAY 3000       // Delay between test steps (ms)
#define THROTTLE_INCREMENT 25 // Throttle increment for speed tests (0-255)
#define MAX_THROTTLE 150      // Maximum throttle level for safety during testing
#define MIN_THROTTLE 75       // Minimum throttle value where motor actually responds
#define RPM_UPDATE_INTERVAL 500 // How often to update RPM calculation (ms)
```

**Throttle Value Guidelines:**
- `MIN_THROTTLE`: Set to 75 by default (first response point)
- For smoother operation in LOW speed mode, use 85+
- For smoother operation in HIGH speed mode, use 100+
- Adapt these values if your controller responds differently

### Reading the Test Output

The serial monitor will display data in this format:
```
Setting throttle to: 75 | RPM: 127 | Hall: 3
```

This shows:
- The current action and value (throttle, brake, etc.)
- The measured RPM from hall sensor data
- The current hall sensor state (binary pattern 0-7)

After the test completes, the Arduino will continue monitoring and reporting RPM.

### Troubleshooting

If the motor doesn't respond:
1. Check that you're using throttle values above the minimum threshold (75+)
2. If not using a transistor, make sure the Low Brake (yellow) wire is DISCONNECTED
3. If using a transistor, verify the wiring and that D0 is LOW during normal operation
4. Check all other wiring connections
5. Verify power supply to both Arduino and controller
6. Ensure controller is properly connected to the motor
7. Check serial output for any error messages
8. Try manually cycling the controller power
9. Verify the throttle signal is being properly generated by checking the voltage on D1

If the motor operation is unstable/shaky:
1. Try increasing the throttle value (above 100 is recommended for stability)
2. Ensure all ground connections are solid
3. Check that the throttle signal wire has no interference sources nearby
4. Test at different speed modes (LOW might be more stable than HIGH at lower throttle values)

If RPM readings are not working:
1. Verify hall sensor connections (ground, signal wires)
2. Check that the hall sensors are receiving power
3. Use a multimeter to verify that hall sensor signals change when rotating the motor by hand
4. Try adjusting the RPM calculation parameters in the code if readings seem inaccurate

### Next Steps

After successfully running this basic test, you can proceed to:
1. Test other controller features (High Brake, E-Lock, Hard Boot, Cruise Control)
2. Implement more advanced hall sensor feedback monitoring
3. Set up CAN communication for dashboard integration
4. Develop a full application using the components/motors/src files 

## Key Testing Observations

Through extensive testing, we've observed these important characteristics about the Kunray MY1020 controller:

### Throttle Response

- The minimum effective throttle value is 75-85 in LOW mode (controller doesn't respond below this)
- Throttle values 85-150 provide the most stable and controllable response
- Above 200, throttle response may become too sensitive for precise control
- Full throttle (255) should only be used in controlled testing environments
- In HIGH speed mode, minimum effective throttle increases to ~100

### RPM Characteristics

- LOW speed mode provides approximately 65% of the RPM of HIGH speed mode at equal throttle values
- RPM increases mostly linearly with throttle values between 100-200
- Hall sensor readings remain stable and consistent throughout operation

### Direction Changing

- The controller requires a complete stop (0 throttle) before changing direction
- Attempting direction change while throttle is applied results in no response
- There is approximately a 300-500ms delay after direction change before motor responds to throttle

### Braking Behavior

- Low brake (yellow wire) provides immediate full braking when connected
- High brake (orange wire) provides proportional braking control
- When both brakes are applied, low brake takes precedence
- Motor requires ~500ms to resume operation after brake release 

**Arduino Nano Alternative Wiring:**
If using an Arduino Nano, note that only pins 2 and 3 support hardware interrupts, limiting hall sensor monitoring:

```
Arduino Nano                   Kunray MY1020 Controller
+-----------+                 +----------------------+
|           |                 |  THROTTLE CONNECTOR  |
| D5 (PWM)  |---------------->| Green (Signal)       |
|           |                 | Red (5V from controller)|
|           |                 |                      |
|           |                 |  REVERSE CONNECTOR   |
| D6        |---------------->| Blue (Signal)        |
|           |                 |                      |
|           |                 |  3-SPEED CONNECTOR   |
| D8        |---------------->| White (Speed 1)      |
| D9        |---------------->| Blue (Speed 2)       |
|           |                 |                      |
| D10       |---------------->| High Brake: Orange (Signal)|
| D7        |---[Transistor]-->| Low Brake: Yellow   |
|           |                 |                      |
| D2        |<----------------| Hall Sensor A        |
| D3        |<----------------| Hall Sensor B        |
| D4        |<----------------| Hall Sensor C (polled, not interrupt)|
|           |                 |                      |
| A0        |<--[VD]----------| Temp Sensor - Battery|
| A1        |<--[VD]----------| Temp Sensor - Controller|
| A2        |<--[VD]----------| Temp Sensor - Motor  |
|           |                 |                      |
| GND       |-+-------------->| Black (GND) - Throttle   |
|           | |              >| Black (GND) - Reverse    |
|           | |              >| Black (GND) - 3-Speed    |
|           | |              >| Black (GND) - Temp Sensors|
|           | +-------------->| Black (GND) - Hall Sensors |
+-----------+                 +----------------------+
```

**Important Power Notes:**
- DO NOT connect Arduino 5V to the throttle's red wire
- The controller supplies its own 5V reference through the red wire
- The throttle circuit receives power FROM the controller, not from the Arduino
- Connecting both power sources could create ground loops or damage components

**Important Ground Notes:**
- ALL black wires MUST connect to the same Arduino GND pin
- Using a breadboard or wire splitter to create a common ground point is recommended
- Proper ground connection is essential for signal integrity and safety

**Critical Brake Notes:**
- High Brake (orange wire) can be controlled normally via D10

### Hall Sensor Connections

The hall sensors provide feedback on motor rotation and speed. Connect them as follows:

1. Locate the hall sensor outputs from the motor or controller:
   - Most BLDC motors have 3 hall sensors (A, B, C)
   - Each sensor typically requires 5V power, GND, and provides a signal output
   - For ESP8266: Connect all three hall sensor signals to interrupt-capable pins (D8, D9, D10 recommended)
   - For Arduino Nano: Connect only Hall A and B to interrupt pins (D2, D3). Hall C will use polling.
   - Use a common ground connection

2. In the test code, hall sensor feedback is used to:
   - Calculate motor RPM in real-time
   - Confirm motor response to throttle changes
   - Verify brake functionality

3. If hall sensor connections are not accessible:
   - The test will still work but without RPM data
   - Comment out the hall sensor code sections if not using

**Important Note on Hall Sensor Accuracy:**
- Using the ESP8266 with interrupts on all three hall sensors provides more accurate RPM measurements, especially at high speeds
- Arduino Nano can only use interrupts on two sensors, which may result in less accurate readings at very high RPMs
- The code automatically handles both configurations

### Low Brake Control Using Transistor

Since the yellow wire needs to be physically disconnected for normal operation, you MUST use a transistor to control it:

```
Arduino Nano         Transistor          Kunray Controller
+-----------+        +-------+           +----------------+
| D7        |---[1K]--| Base |           |                |
|           |        |       |           |                |
|           |        | Coll. |-----------> Yellow (Low Brake) |
|           |        |       |           |                |
| GND       |--------| Emit. |           |                |
+-----------+        +-------+           +----------------+
```

With this setup:
- D7 HIGH = Transistor ON = Yellow wire connected to GND = Brake engaged
- D7 LOW = Transistor OFF = Yellow wire disconnected = Normal operation

**IMPORTANT:** Direct connection of the yellow wire will prevent the motor from running. Always use the transistor circuit shown above.

### Speed Mode Configuration
The 3-speed connector has two signal wires (White and Blue) plus ground:

| D8 (GPIO15) | D9 (GPIO13) | Result    |
|------------|-----------|-----------|
| LOW        | LOW       | OFF       |
| HIGH       | LOW       | LOW Speed |
| LOW        | HIGH      | HIGH Speed|

Both D8 and D9 connect directly to their respective wires. Make sure all grounds are connected to a common point.

### Safety Precautions

⚠️ **IMPORTANT SAFETY WARNINGS** ⚠️

1. This test will cause the motor to run! Ensure motor is securely mounted and safe to operate.
2. Start with lower voltage if possible (e.g., 48V instead of 72V for initial testing).
3. Keep motor clear of obstructions and people during testing.
4. Have an emergency power cutoff ready at all times.
5. Modify the `MAX_THROTTLE` value in the code to limit maximum motor speed during testing.

### How to Use

1. Connect Arduino to the Kunray controller following the wiring diagram.
2. For the low brake (yellow wire) either:
   - Leave it completely DISCONNECTED, or
   - Connect it through a transistor as shown in the diagram above
3. Connect the hall sensor wires if available for RPM monitoring
4. Open the sketch in Arduino IDE or PlatformIO.
5. Set `USING_TRANSISTOR` or `USING_RELAY` based on your setup
6. Upload the code to the Arduino.
7. Open Serial Monitor at 115200 baud.
8. Follow the on-screen instructions to start the test sequence.
9. The test will automatically cycle through the following sequence while displaying RPM data:
   - Part 1: Gradual throttle increase in forward direction
   - Part 2: Testing OFF/LOW/HIGH speed modes
   - Part 3: Testing reverse direction
   - Part 4: Combined test of various settings
   - Part 5: Testing brakes (both low and high if connected)

### Adjusting Test Parameters

You can modify these constants at the top of the file to adjust the test behavior:

```cpp
#define STEP_DELAY 3000       // Delay between test steps (ms)
#define THROTTLE_INCREMENT 25 // Throttle increment for speed tests (0-255)
#define MAX_THROTTLE 150      // Maximum throttle level for safety during testing
#define MIN_THROTTLE 75       // Minimum throttle value where motor actually responds
#define RPM_UPDATE_INTERVAL 500 // How often to update RPM calculation (ms)
```

**Throttle Value Guidelines:**
- `MIN_THROTTLE`: Set to 75 by default (first response point)
- For smoother operation in LOW speed mode, use 85+
- For smoother operation in HIGH speed mode, use 100+
- Adapt these values if your controller responds differently

### Reading the Test Output

The serial monitor will display data in this format:
```
Setting throttle to: 75 | RPM: 127 | Hall: 3
```

This shows:
- The current action and value (throttle, brake, etc.)
- The measured RPM from hall sensor data
- The current hall sensor state (binary pattern 0-7)

After the test completes, the Arduino will continue monitoring and reporting RPM.

### Troubleshooting

If the motor doesn't respond:
1. Check that you're using throttle values above the minimum threshold (75+)
2. If not using a transistor, make sure the Low Brake (yellow) wire is DISCONNECTED
3. If using a transistor, verify the wiring and that D10 is LOW during normal operation
4. Check all other wiring connections
5. Verify power supply to both Arduino and controller
6. Ensure controller is properly connected to the motor
7. Check serial output for any error messages
8. Try manually cycling the controller power
9. Verify the throttle signal is being properly generated by checking the voltage on D1

If the motor operation is unstable/shaky:
1. Try increasing the throttle value (above 100 is recommended for stability)
2. Ensure all ground connections are solid
3. Check that the throttle signal wire has no interference sources nearby
4. Test at different speed modes (LOW might be more stable than HIGH at lower throttle values)

If RPM readings are not working:
1. Verify hall sensor connections (ground, signal wires)
2. Check that the hall sensors are receiving power
3. Use a multimeter to verify that hall sensor signals change when rotating the motor by hand
4. Try adjusting the RPM calculation parameters in the code if readings seem inaccurate

### Next Steps

After successfully running this basic test, you can proceed to:
1. Test other controller features (High Brake, E-Lock, Hard Boot, Cruise Control)
2. Implement more advanced hall sensor feedback monitoring
3. Set up CAN communication for dashboard integration
4. Develop a full application using the components/motors/src files 

## Key Testing Observations

Through extensive testing, we've observed these important characteristics about the Kunray MY1020 controller:

### Throttle Response

- The minimum effective throttle value is 75-85 in LOW mode (controller doesn't respond below this)
- Throttle values 85-150 provide the most stable and controllable response
- Above 200, throttle response may become too sensitive for precise control
- Full throttle (255) should only be used in controlled testing environments
- In HIGH speed mode, minimum effective throttle increases to ~100

### RPM Characteristics

- LOW speed mode provides approximately 65% of the RPM of HIGH speed mode at equal throttle values
- RPM increases mostly linearly with throttle values between 100-200
- Hall sensor readings remain stable and consistent throughout operation

### Direction Changing

- The controller requires a complete stop (0 throttle) before changing direction
- Attempting direction change while throttle is applied results in no response
- There is approximately a 300-500ms delay after direction change before motor responds to throttle

### Braking Behavior

- Low brake (yellow wire) provides immediate full braking when connected
- High brake (orange wire) provides proportional braking control
- When both brakes are applied, low brake takes precedence
- Motor requires ~500ms to resume operation after brake release 

### Temperature Sensor Connections

The test setup can monitor temperatures at three critical points using NTCLE100E3203JBD NTC thermistors:

1. **Battery Temperature** - Connected to A0
2. **Controller Temperature** - Connected to A1 
3. **Motor Temperature** - Connected to A2

Each thermistor requires a voltage divider circuit:

```
Arduino 5V
   |
   Z
   Z 10kΩ Resistor
   Z
   |
   +-----------------> To Analog Pin (A0/A1/A2)
   |
   Z
   Z NTC Thermistor (NTCLE100E3203JBD)
   Z
   |
  GND
```

**Installation:**
- Attach the thermistor to the surface being monitored using thermal paste and secure with heat-resistant tape
- For motor temperature, place on the motor housing (not the shaft)
- For controller, place on the heatsink or case (not directly on components)
- For battery, attach to the battery pack case (center of the pack if possible)

**Code Integration:**
- Uncomment the temperature monitoring section in BasicControllerTest.cpp
- Add these constants at the top of the file:

```cpp
// NTC Thermistor parameters for NTCLE100E3203JBD
#define TEMP_SENSOR_BATTERY A0
#define TEMP_SENSOR_CONTROLLER A1
#define TEMP_SENSOR_MOTOR A2
#define THERMISTOR_NOMINAL 10000   // Resistance at 25°C
#define TEMPERATURE_NOMINAL 25     // Temperature for nominal resistance (°C)
#define B_COEFFICIENT 3977         // Beta coefficient from datasheet
#define SERIES_RESISTOR 10000      // Value of the series resistor
```

**Temperature Conversion:**
The code uses the Steinhart-Hart equation to convert resistance to temperature:

```cpp
float readTemperature(int pin) {
  float reading = analogRead(pin);
  // Convert reading to resistance
  reading = (1023 / reading) - 1;
  reading = SERIES_RESISTOR / reading;
  
  // Steinhart-Hart equation for temperature
  float steinhart = reading / THERMISTOR_NOMINAL;          // (R/Ro)
  steinhart = log(steinhart);                              // ln(R/Ro)
  steinhart /= B_COEFFICIENT;                              // 1/B * ln(R/Ro)
  steinhart += 1.0 / (TEMPERATURE_NOMINAL + 273.15);       // + (1/To)
  steinhart = 1.0 / steinhart;                             // Invert
  steinhart -= 273.15;                                     // Convert to °C
  
  return steinhart;
}
```

The serial monitor will now display temperature readings along with RPM data:
```
Setting throttle to: 75 | RPM: 127 | Hall: 3 | Temp(°C): Bat=32.1 Ctrl=45.6 Mot=52.3
```

**Safety Thresholds:**
- Battery: Warning >45°C, Critical >55°C
- Controller: Warning >65°C, Critical >80°C 
- Motor: Warning >75°C, Critical >90°C

If critical temperatures are detected, the test will automatically engage safe mode (reduce throttle or stop).