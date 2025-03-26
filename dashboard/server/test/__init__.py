"""
Test module for the Dashboard Server.
This module ensures that the server directory is in the Python path.
"""
import sys
import os
import pathlib

# Add the server directory to the Python path
server_dir = pathlib.Path(__file__).parent.parent.absolute()
if str(server_dir) not in sys.path:
    sys.path.insert(0, str(server_dir))
