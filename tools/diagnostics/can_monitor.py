#!/usr/bin/env python3
# CAN Monitor Script
# Monitors the CAN bus and converts messages to human-readable format
# based on the go-kart platform protocol definitions

import sys
import os
import argparse
import socket
import struct
import time
from binascii import unhexlify
from datetime import datetime, timedelta

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

# CAN frame format (from linux/can.h)
CAN_MTU = 16  # sizeof(struct can_frame)
CAN_FRAME_FORMAT = "=IB3x8s"  # struct can_frame format

# CAN bus constants
CAN_OVERHEAD_BITS = 47  # Standard CAN frame overhead (SOF, ID, Control, CRC, etc.)
CAN_EOF_BITS = 7  # End of frame bits
CAN_INTERFRAME_BITS = 3  # Interframe spacing bits

# CAN ID field bit shifts and masks (matching ProtobufCANInterface.h)
MSG_TYPE_SHIFT = 10
COMP_TYPE_SHIFT = 7
COMP_ID_SHIFT = 4
CMD_ID_SHIFT = 2
VALUE_TYPE_SHIFT = 0

MSG_TYPE_MASK = 0x1
COMP_TYPE_MASK = 0x7
COMP_ID_MASK = 0x7
CMD_ID_MASK = 0x3
VALUE_TYPE_MASK = 0x3

class BusStats:
    def __init__(self, bitrate):
        self.bitrate = bitrate
        self.start_time = datetime.now()
        self.last_update = self.start_time
        self.total_bits = 0
        self.frame_count = 0
        
    def update(self, frame_length):
        now = datetime.now()
        self.frame_count += 1
        
        # Calculate bits for this frame:
        # - 47 bits overhead (SOF, ID, Control, CRC, etc.)
        # - 8 bits per data byte
        # - 7 bits EOF
        # - 3 bits interframe spacing
        frame_bits = CAN_OVERHEAD_BITS + (frame_length * 8) + CAN_EOF_BITS + CAN_INTERFRAME_BITS
        self.total_bits += frame_bits
        
        # Calculate current utilization stats
        elapsed = (now - self.start_time).total_seconds()
        bits_per_second = self.total_bits / elapsed
        kb_per_second = bits_per_second / 1000
        utilization = (bits_per_second / self.bitrate) * 100
        
        return f"{utilization:.1f}% | {kb_per_second:.1f}kb/s | {self.frame_count}"

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

def parse_can_message(can_id, data, length, bus_stats):
    """Parse a CAN message using the protocol definitions"""
    try:
        # Update bus statistics and get stats string
        stats_str = bus_stats.update(length)
        
        # Extract fields from CAN ID
        msg_type = (can_id >> MSG_TYPE_SHIFT) & MSG_TYPE_MASK
        msg_type_str = get_enum_name(common_pb2.MessageType, msg_type)
        
        comp_type = (can_id >> COMP_TYPE_SHIFT) & COMP_TYPE_MASK
        comp_type_str = get_enum_name(common_pb2.ComponentType, comp_type)
        
        comp_id = (can_id >> COMP_ID_SHIFT) & COMP_ID_MASK
        comp_id_str = str(comp_id)  # Default value
        
        cmd_id = (can_id >> CMD_ID_SHIFT) & CMD_ID_MASK
        cmd_id_str = str(cmd_id)  # Default value
        
        value_type = (can_id >> VALUE_TYPE_SHIFT) & VALUE_TYPE_MASK
        value_type_str = get_enum_name(common_pb2.ValueType, value_type)
        
        # Calculate value from data bytes
        value = int.from_bytes(data[:length], byteorder='big')
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
        
        # Format the output with bus stats
        print(f"{msg_type_str:<10} | {comp_type_str:<10} | {comp_id_str:<15} | {cmd_id_str:<15} | {value_type_str:<10} | {value_display:<15} | {stats_str}")
        
    except Exception as e:
        print(f"Error parsing message: {e}")
        print(f"Raw CAN ID: 0x{can_id:04X}, Length: {length}, Data: {data.hex()}")

def monitor_can_bus(interface, bitrate):
    """Monitor the CAN bus using SocketCAN"""
    # Print header
    print(f"{'MSG_TYPE':<10} | {'COMP_TYPE':<10} | {'COMPONENT':<15} | {'COMMAND':<15} | {'VAL_TYPE':<10} | {'VALUE':<15} | {'BUS STATS'}")
    print("-" * 120)
    
    try:
        # Create a raw CAN socket
        sock = socket.socket(socket.PF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
        
        # Bind to the interface
        sock.bind((interface,))
        
        # Set non-blocking mode
        sock.setblocking(False)
        
        # Initialize bus statistics
        bus_stats = BusStats(bitrate)
        
        print(f"Monitoring CAN interface {interface} at {bitrate/1000} kb/s...")
        print("Press Ctrl+C to exit")
        print("")
        
        while True:
            try:
                # Try to receive a frame
                frame = sock.recv(CAN_MTU)
                if frame:
                    # Unpack the CAN frame
                    can_id, length, data = struct.unpack(CAN_FRAME_FORMAT, frame)
                    
                    # Parse and display the message
                    parse_can_message(can_id, data, length, bus_stats)
            
            except socket.error:
                # No data available, sleep briefly
                time.sleep(0.001)
    
    except KeyboardInterrupt:
        print("\nCAN monitoring stopped by user")
    except Exception as e:
        print(f"Error monitoring CAN bus: {e}")
    finally:
        # Clean up
        if 'sock' in locals():
            sock.close()

def main():
    """Main function"""
    parser = argparse.ArgumentParser(description='Monitor CAN bus and interpret messages using protocol definitions')
    parser.add_argument('interface', nargs='?', default='can0', help='CAN interface to monitor (default: can0)')
    parser.add_argument('--bitrate', type=int, default=500000, help='CAN bus bitrate in bits/second (default: 500000)')
    parser.add_argument('--debug', action='store_true', help='Print debug information about protocol enums')
    args = parser.parse_args()
    
    if args.debug:
        debug_protocol_enums()
        return
    
    print(f"Starting CAN monitor on interface {args.interface}...")
    monitor_can_bus(args.interface, args.bitrate)

if __name__ == '__main__':
    main()
