"""
Animation module for LED strip control.

This module provides functionality to manage, preview, and transmit
animation data to LED controllers over CAN.
"""

from .manager import AnimationManager
from .protocol import AnimationProtocol

__all__ = ['AnimationManager', 'AnimationProtocol']
