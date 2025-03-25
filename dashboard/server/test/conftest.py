import pytest
import json
from unittest.mock import patch, mock_open
import sys
import os
from pathlib import Path

# Add the server directory to Python path
server_dir = Path(__file__).parent.parent
if str(server_dir) not in sys.path:
    sys.path.insert(0, str(server_dir))

import app