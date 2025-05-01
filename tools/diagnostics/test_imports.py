#!/usr/bin/env python3
"""
Test script to verify that imports from shared libraries work correctly
"""

import sys
import os

# Add project root to sys.path
PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '../..'))
sys.path.insert(0, PROJECT_ROOT)

print(f"PROJECT_ROOT: {PROJECT_ROOT}")
print(f"sys.path: {sys.path}")

try:
    # Try to import the protocol registry
    from shared.lib.python.can.protocol_registry import ProtocolRegistry
    print("Successfully imported ProtocolRegistry")
    
    # Try to create a protocol registry
    registry = ProtocolRegistry()
    print(f"Created ProtocolRegistry: {registry}")
    
    # Try to import the telemetry state
    from shared.lib.python.telemetry.state import GoKartState
    print("Successfully imported GoKartState")
    
    # Try to import the telemetry store
    from shared.lib.python.telemetry.store import TelemetryStore
    print("Successfully imported TelemetryStore")
    
    # Try to import the CAN interface
    from shared.lib.python.can.interface import CANInterfaceWrapper
    print("Successfully imported CANInterfaceWrapper")
    
    print("All imports successful!")
    
except ImportError as e:
    print(f"Error importing: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1) 