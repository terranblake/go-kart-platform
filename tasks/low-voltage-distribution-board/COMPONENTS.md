# Power Distribution Board - Component List

## Power Input and Protection
- J1: 12V Power Input Terminal Block/Connector (10A rating minimum)
- D1: Reverse Polarity Protection Schottky Diode (STPS10L25G or similar, 10A/25V)
- F1: Main Fuse, 10A, Slow Blow with Holder
- SW1: Main Power Switch, SPST, 10A Rating
- TVS1: Transient Voltage Suppressor Diode (SMCJ15A or similar, 15V clamping)

## Filtering and Storage
- C1, C2: 1000μF 25V Electrolytic Capacitors (Low ESR type)
- C3-C9: 0.1μF 50V Ceramic Capacitors (for individual outputs)
- FB1: Ferrite Bead (for EMI filtering, 1A+ rating)
- L1: 10μH Power Inductor (optional additional filtering)

## Voltage Regulation
- U1-U3: LM7805 5V Linear Voltage Regulator (1A)
- U4: LM7805 5V Linear Voltage Regulator with larger heatsink (for RPi)
- C10-C13: 10μF 25V Input Filter Capacitors (for each regulator)
- C14-C17: 47μF 10V Output Filter Capacitors (for each regulator)
- HS1-HS4: TO-220 Heatsinks for Voltage Regulators

## Distribution
- BUS1: 12V Distribution Bus Bar or Heavy Copper Trace
- BUS2: GND Distribution Bus Bar or Heavy Copper Trace
- BUS3: 5V Distribution Bus Bar or Heavy Copper Trace
- J2-J8: Output Terminal Blocks/Connectors (for Nanos and RPi)

## Monitoring and Indicators
- R1: Bleeder Resistor (10kΩ 1W)
- R2: Current Sense Resistor (0.01Ω 2W)
- U5: Voltage Monitor IC (MCP39F511A or similar)
- D2: Power Indicator LED (Green)
- R3: LED Current Limiting Resistor (1kΩ)
- U6: Optional Current Monitor IC (INA219 or similar)

## Protection (Individual Circuits)
- F2-F8: 1A Fuses for Arduino Nano Outputs
- F9: 2A Fuse for Raspberry Pi Output

## High Voltage Isolation
- IC1-IC7: Optocouplers for signal isolation (PC817 or similar)
- R4-R10: Input Current Limiting Resistors for Optocouplers (330Ω)
- R11-R17: Pull-up Resistors for Optocoupler Outputs (10kΩ)

## Mechanical
- PCB1: FR-4 PCB with 2oz Copper (for higher current capacity)
- M1-M4: PCB Mounting Standoffs/Screws
- ENCL1: Optional Enclosure