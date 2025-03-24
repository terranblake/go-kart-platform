"""
CrossPlatformCAN Python Interface

This package provides a Python interface to the CrossPlatformCAN library.
"""

from .can_interface import (
    CANInterface,
    MessageType,
    ComponentType,
    ValueType,
    MessageHandlerCallback
)

__all__ = [
    'CANInterface',
    'MessageType',
    'ComponentType',
    'ValueType',
    'MessageHandlerCallback'
] 