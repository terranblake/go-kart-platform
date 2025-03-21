#!/usr/bin/env python3
# PlatformIO component builder script for Go-Kart Platform
# This script helps set up the correct environment for building components

import os
import sys
from SCons.Script import DefaultEnvironment

def main():
    # Get PlatformIO build environment
    env = DefaultEnvironment()
    
    # Get the custom component name
    custom_component = env.GetProjectOption("custom_component", "")
    
    # Get the component type from build flags
    component_type = None
    build_flags = env.get("BUILD_FLAGS", [])
    for flag in build_flags:
        if isinstance(flag, str) and "COMPONENT_TYPE=" in flag:
            component_type = flag.split("=")[1].strip()
            break
    
    print(f"Building {custom_component} (type: {component_type})")
    
    # Verify the component directory exists
    component_dir = os.path.join(env.get("PROJECT_DIR"), custom_component)
    if not os.path.isdir(component_dir):
        print(f"Error: Component directory not found: {component_dir}")
        env.Exit(1)
    
    # For sensor variants, handle special case
    if custom_component == "sensors":
        # Extract sensor type from build flags
        sensor_type = None
        for flag in build_flags:
            if isinstance(flag, str) and "SENSOR_TYPE=" in flag:
                sensor_type = flag.split("=")[1].strip()
                break
        
        if sensor_type:
            print(f"Building sensor variant: {sensor_type}")
            
            # Get all available variants by scanning directory
            variants_dir = os.path.join(component_dir, "variants")
            if not os.path.isdir(variants_dir):
                print(f"Error: Variants directory not found: {variants_dir}")
                env.Exit(1)
            
            # Discover available variants
            available_variants = {}
            for variant in os.listdir(variants_dir):
                variant_path = os.path.join(variants_dir, variant)
                if os.path.isdir(variant_path):
                    available_variants[variant.upper()] = variant
            
            # Try to match sensor type with variant
            variant_dir = None
            sensor_key = sensor_type.replace("SENSOR_", "")
            
            # Method 1: Direct match
            if sensor_key in available_variants:
                variant_dir = available_variants[sensor_key]
            # Method 2: Partial match
            else:
                for key, value in available_variants.items():
                    if key in sensor_key or sensor_key in key:
                        variant_dir = value
                        break
            
            if variant_dir:
                variant_path = os.path.join(variants_dir, variant_dir)
                print(f"Using variant directory: {variant_path}")
            else:
                print(f"Warning: No matching variant found for {sensor_type}")
                print(f"Available variants: {list(available_variants.values())}")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())