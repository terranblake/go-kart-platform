#!/usr/bin/env python3
# CAN Monitor Script
# Monitors the CAN bus and converts messages to human-readable format
# based on the go-kart platform protocol definitions

import sys
import os
import re
import argparse
import subprocess
from binascii import unhexlify

# Add the protocol directory to the Python path
script_dir = os.path.dirname(os.path.abspath(__file__))
protocol_dir = os.path.join(os.path.dirname(os.path.dirname(script_dir)), "protocol/generated/python")
sys.path.append(protocol_dir)

# Import the generated protocol buffers
try:
    import common_pb2
    import lights_pb2
    import motors_pb2
    import sensors_pb2
    import controls_pb2
except ImportError as e:
    print(f"Error importing protocol definitions: {e}")
    print(f"Looking for protocol definitions in: {protocol_dir}")
    print("Make sure the protocol buffer definitions are generated.")
    sys.exit(1)

# Regular expression pattern to parse candump output
# Example: can0  123   [8]  11 22 33 44 55 66 77 88
CANDUMP_PATTERN = re.compile(r'(\w+)\s+([0-9A-F]+)\s+\[\d+\]\s+([0-9A-F\s]+)')

def get_enum_name(enum_obj, value, default="UNKNOWN"):
    """Get the name for an enum value from its EnumTypeWrapper object"""
    try:
        # Convert value to int to ensure proper lookup
        value = int(value)
        # Try to find the name in the enum
        for name, val in enum_obj.items():
            if val == value:
                return name
    except (AttributeError, ValueError, TypeError):
        pass
    return f"{default}({value})"

def parse_can_message(data_str):
    """Parse a CAN message using the protocol definitions"""
    try:
        # Remove spaces and convert to bytes
        data_str = data_str.replace(" ", "")
        if len(data_str) < 16:  # Need at least 8 bytes
            print(f"Invalid data length: {len(data_str)}")
            return
            
        data_bytes = unhexlify(data_str)
        
        # Extract bytes according to protocol definition
        byte0 = data_bytes[0]
        byte1 = data_bytes[1]
        byte2 = data_bytes[2]
        byte3 = data_bytes[3]
        byte4 = data_bytes[4]
        
        # Extract message type (2 bits from byte0)
        msg_type = byte0 >> 6
        msg_type_str = get_enum_name(common_pb2.MessageType, msg_type)
        
        # Extract component type (3 bits from byte0)
        comp_type = (byte0 >> 3) & 0x07
        comp_type_str = get_enum_name(common_pb2.ComponentType, comp_type)
        
        # Extract component ID (byte2)
        comp_id = byte2
        comp_id_str = str(comp_id)  # Default value
        
        # Extract command ID (byte3)
        cmd_id = byte3
        cmd_id_str = str(cmd_id)  # Default value
        
        # Extract value type (4 bits from byte4)
        value_type = byte4 >> 4
        value_type_str = get_enum_name(common_pb2.ValueType, value_type)
        
        # Calculate value (bytes 5-7)
        value = int.from_bytes(data_bytes[5:8], byteorder='big')
        value_display = value  # Default display value
        
        # Map component ID to string based on component type
        if comp_type == common_pb2.LIGHTS:  # LIGHTS
            if comp_id == 255:  # Special case for ALL
                comp_id_str = "ALL"
            else:
                comp_id_str = get_enum_name(lights_pb2.LightComponentId, comp_id, "LIGHT")
            
            cmd_id_str = get_enum_name(lights_pb2.LightCommandId, cmd_id, "CMD")
            
            # For certain command IDs, interpret the value field
            if cmd_id == lights_pb2.MODE:  # MODE
                value_display = get_enum_name(lights_pb2.LightModeValue, value, str(value))
            elif cmd_id == lights_pb2.SIGNAL:  # SIGNAL
                value_display = get_enum_name(lights_pb2.LightSignalValue, value, str(value))
            elif cmd_id == lights_pb2.PATTERN:  # PATTERN
                value_display = get_enum_name(lights_pb2.LightPatternValue, value, str(value))
            elif cmd_id == lights_pb2.COLOR:  # COLOR
                value_display = get_enum_name(lights_pb2.LightColorValue, value, str(value))
            elif cmd_id == lights_pb2.SEQUENCE:  # SEQUENCE
                value_display = get_enum_name(lights_pb2.LightSequenceValue, value, str(value))
                
        elif comp_type == common_pb2.MOTORS:  # MOTORS
            if comp_id == 255:  # Special case for ALL
                comp_id_str = "ALL"
            else:
                comp_id_str = get_enum_name(motors_pb2.MotorComponentId, comp_id, "MOTOR")
            
            cmd_id_str = get_enum_name(motors_pb2.MotorCommandId, cmd_id, "CMD")
            
            # For certain command IDs, interpret the value field
            if cmd_id == motors_pb2.DIRECTION:  # DIRECTION
                value_display = get_enum_name(motors_pb2.MotorDirectionValue, value, str(value))
            elif cmd_id == motors_pb2.BRAKE:  # BRAKE
                value_display = get_enum_name(motors_pb2.MotorBrakeValue, value, str(value))
            elif cmd_id == motors_pb2.MODE:  # MODE
                value_display = get_enum_name(motors_pb2.MotorModeValue, value, str(value))
            elif cmd_id == motors_pb2.EMERGENCY:  # EMERGENCY
                value_display = get_enum_name(motors_pb2.MotorEmergencyValue, value, str(value))
            
        elif comp_type == common_pb2.SENSORS:  # SENSORS
            cmd_id_str = get_enum_name(sensors_pb2.SensorCommandId, cmd_id, "CMD")
            
            # For STATUS command, interpret the value field
            if cmd_id == sensors_pb2.STATUS:  # STATUS
                value_display = get_enum_name(sensors_pb2.SensorStatusValue, value, str(value))
            
        elif comp_type == common_pb2.CONTROLS:  # CONTROLS
            if comp_id == 10:  # Special case for ALL
                comp_id_str = "ALL"
            else:
                comp_id_str = get_enum_name(controls_pb2.ControlComponentId, comp_id, "CONTROL")
            
            cmd_id_str = get_enum_name(controls_pb2.ControlCommandId, cmd_id, "CMD")
            
            # For certain command IDs, interpret the value field
            if cmd_id == controls_pb2.MODE:  # MODE
                value_display = get_enum_name(controls_pb2.ControlModeValue, value, str(value))
            elif cmd_id == controls_pb2.CALIBRATE:  # CALIBRATE
                value_display = get_enum_name(controls_pb2.ControlCalibrateValue, value, str(value))
            elif cmd_id == controls_pb2.EMERGENCY:  # EMERGENCY
                value_display = get_enum_name(controls_pb2.ControlEmergencyValue, value, str(value))
        
        # Format the output
        print(f"{msg_type_str:<10} | {comp_type_str:<10} | {comp_id_str:<15} | {cmd_id_str:<15} | {value_type_str:<10} | {value_display}")
        
    except Exception as e:
        print(f"Error parsing message: {e}")
        print(f"Raw data: {data_str}")

def debug_protocol_enums():
    """Print information about all direct enums"""
    print("\nCommon Enums:")
    print(f"  MessageType: {list(common_pb2.MessageType.items())}")
    print(f"  ComponentType: {list(common_pb2.ComponentType.items())}")
    print(f"  ValueType: {list(common_pb2.ValueType.items())}")
    
    print("\nLights Enums:")
    print(f"  LightComponentId: {list(lights_pb2.LightComponentId.items())}")
    print(f"  LightCommandId: {list(lights_pb2.LightCommandId.items())}")
    print(f"  LightModeValue: {list(lights_pb2.LightModeValue.items())}")
    
    print("\nDirect Constants:")
    print(f"  LIGHTS = {common_pb2.LIGHTS}")
    print(f"  MOTORS = {common_pb2.MOTORS}")
    print(f"  SENSORS = {common_pb2.SENSORS}")
    print(f"  STATUS = {common_pb2.STATUS}")
    print(f"  COMMAND = {common_pb2.COMMAND}")
    
    print("\nLights Constants:")
    print(f"  MODE = {lights_pb2.MODE}")
    print(f"  SIGNAL = {lights_pb2.SIGNAL}")
    print(f"  FRONT = {lights_pb2.FRONT}")
    print(f"  REAR = {lights_pb2.REAR}")

def monitor_can_bus(interface):
    """Monitor the CAN bus using candump and parse messages"""
    # Print header
    print(f"{'MSG_TYPE':<10} | {'COMP_TYPE':<10} | {'COMPONENT':<15} | {'COMMAND':<15} | {'VAL_TYPE':<10} | {'VALUE':<15}")
    print("-" * 80)
    
    try:
        # Start candump process
        process = subprocess.Popen(
            ['candump', interface],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        
        # Process output
        for line in process.stdout:
            line = line.strip()
            match = CANDUMP_PATTERN.search(line)
            if match:
                _, can_id, data = match.groups()
                parse_can_message(data)
    
    except KeyboardInterrupt:
        print("\nCAN monitoring stopped by user")
    except Exception as e:
        print(f"Error monitoring CAN bus: {e}")
    finally:
        # Clean up
        if 'process' in locals():
            process.terminate()
            process.wait()

def check_dependencies():
    """Check if required dependencies are installed"""
    try:
        subprocess.run(['candump', '--version'], 
                      stdout=subprocess.PIPE, 
                      stderr=subprocess.PIPE, 
                      check=False)
    except FileNotFoundError:
        print("Error: candump not found. Please install can-utils.")
        return False
    return True

def main():
    """Main function"""
    parser = argparse.ArgumentParser(description='Monitor CAN bus and interpret messages using protocol definitions')
    parser.add_argument('interface', nargs='?', default='can0', help='CAN interface to monitor (default: can0)')
    parser.add_argument('--debug', action='store_true', help='Print debug information about protocol enums')
    args = parser.parse_args()
    
    if args.debug:
        debug_protocol_enums()
        return
    
    if not check_dependencies():
        sys.exit(1)
    
    print(f"Starting CAN monitor on interface {args.interface}...")
    print("Press Ctrl+C to exit")
    print("")
    
    monitor_can_bus(args.interface)

if __name__ == '__main__':
    main()
