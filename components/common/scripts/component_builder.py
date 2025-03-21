#!/usr/bin/env python
import os
import sys

# This script helps PlatformIO build the correct component
# It's called by the extra_scripts setting in platformio.ini

component = os.environ.get('COMPONENT', None)
if not component:
    print('Error: COMPONENT environment variable not set')
    sys.exit(1)

print(f'Building component: {component}')
