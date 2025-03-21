#!/usr/bin/env python
import os
import sys
from SCons.Script import DefaultEnvironment

# Get PlatformIO build environment
env = DefaultEnvironment()

# Access the build flag defined in platformio.ini
component_type = env.GetProjectOption("build_flags")

# Filter for the COMPONENT_TYPE value
component_type_value = None
for flag in component_type:
    if flag.startswith("-D COMPONENT_TYPE="):
        component_type_value = flag.split("=")[1]
        break

if not component_type_value:
    print("Error: COMPONENT_TYPE build flag not found")
    sys.exit(1)

custom_component = env.GetProjectOption("custom_component")
print(f"Building component type: {component_type_value} for component: {custom_component}")