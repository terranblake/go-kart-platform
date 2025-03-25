#!/usr/bin/env python3
"""
Socket-based CAN test that bypasses can-utils and uses the socket API directly
"""

import socket
import struct
import time
import subprocess
import os

# Constants from protocol definitions
# MessageType values
MESSAGE_TYPE_COMMAND = 0
MESSAGE_TYPE_STATUS = 1
MESSAGE_TYPE_ACK = 2
MESSAGE_TYPE_ERROR = 3

# ComponentType values
COMPONENT_TYPE_LIGHTS = 0
COMPONENT_TYPE_MOTORS = 1
COMPONENT_TYPE_SENSORS = 2
COMPONENT_TYPE_BATTERY = 3
COMPONENT_TYPE_CONTROLS = 4

def create_header_byte(message_type, component_type):
    """Create the first byte of the CAN message that contains message type and component type"""
    # Message type is 2 bits (bits 7-6), component type is 3 bits (bits 5-3)
    return (message_type << 6) | (component_type << 3)

def reset_can_interface(device="can0"):
    """Reset the CAN interface to make sure it's in a good state"""
    print(f"Resetting CAN interface {device}...")
    try:
        subprocess.run(["sudo", "ip", "link", "set", device, "down"], check=True)
        time.sleep(0.5)
        subprocess.run(["sudo", "ip", "link", "set", device, "up", "type", "can", "bitrate", "500000"], check=True)
        time.sleep(0.5)
        print(f"CAN interface {device} reset successfully")
        return True
    except subprocess.CalledProcessError as e:
        print(f"Error resetting CAN interface: {e}")
        return False

def send_can_message_socket(device, can_id, data):
    """Send a CAN message using the socket API directly"""
    try:
        # Create a raw CAN socket
        s = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
        
        # Get the interface index
        s.bind((device,))
        
        # Convert hex string to bytes
        data_bytes = bytes.fromhex(data)
        if len(data_bytes) < 8:
            # Pad to 8 bytes if needed
            data_bytes = data_bytes + b'\x00' * (8 - len(data_bytes))
        elif len(data_bytes) > 8:
            # Truncate if more than 8 bytes
            data_bytes = data_bytes[:8]
            
        # Linux CAN frame format:
        # <IB3x8s - I=CAN ID (32bit), B=data length, 3x=padding, 8s=data
        can_dlc = len(data_bytes)
        can_frame = struct.pack("=IB3x8s", can_id, can_dlc, data_bytes)
        
        print(f"Sending CAN frame: ID={can_id:X}, Data={data}")
        print(f"First byte breakdown: {bin(int(data[:2], 16))[2:].zfill(8)}")
        s.send(can_frame)
        print("Message sent successfully via socket API")
        
        # Close the socket
        s.close()
        return True
    except Exception as e:
        print(f"Error sending CAN message via socket: {e}")
        if 's' in locals():
            s.close()
        return False

def main():
    # Define our test device
    device = "can0"
    
    # Reset the CAN interface
    if not reset_can_interface(device):
        print("Failed to reset CAN interface, exiting")
        return
        
    # Test with standard ID format messages
    
    # 1. Simple test message - standard OBD-II
    can_id = 0x7DF  # Standard OBD-II diagnostic request
    data = "0101000000000000"  # OBD-II Mode 1 PID 1 (monitor status)
    print("\n--- Testing simple OBD-II message ---")
    send_can_message_socket(device, can_id, data)
    time.sleep(1)
    
    # 2. Lights location command
    print("\n--- Sending lights location command with socket API ---")
    can_id = 0x001  # Our node ID
    
    # Create proper header byte for COMMAND to LIGHTS component
    header_byte = create_header_byte(MESSAGE_TYPE_COMMAND, COMPONENT_TYPE_LIGHTS)
    data = f"{header_byte:02x}00000920000000"  # Lights location command
    
    print(f"Header byte for LIGHTS command: 0x{header_byte:02x} (binary: {bin(header_byte)[2:].zfill(8)})")
    send_can_message_socket(device, can_id, data)
    time.sleep(1)
    
    # 3. Controls diagnostic command
    print("\n--- Sending controls diagnostic command with socket API ---")
    can_id = 0x001  # Our node ID
    
    # Create proper header byte for COMMAND to CONTROLS component
    header_byte = create_header_byte(MESSAGE_TYPE_COMMAND, COMPONENT_TYPE_CONTROLS)
    data = f"{header_byte:02x}00080320000006"  # Controls diagnostic command
    
    print(f"Header byte for CONTROLS command: 0x{header_byte:02x} (binary: {bin(header_byte)[2:].zfill(8)})")
    send_can_message_socket(device, can_id, data)
    
    print("\nTest completed. Check Arduino for responses.")

if __name__ == "__main__":
    main() 