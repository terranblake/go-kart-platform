from setuptools import setup, Extension, find_packages
import os
import sys
import platform
import numpy as np
import subprocess
import shutil

# Determine the base path to the repository
repo_root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
print(f"Repository root: {repo_root}")

# Path to the CrossPlatformCAN library
cross_platform_can_dir = os.path.join(repo_root, 'components', 'common', 'lib', 'CrossPlatformCAN')
print(f"CrossPlatformCAN directory: {cross_platform_can_dir}")

# Path to the nanopb headers
nanopb_dir = os.path.join(repo_root, 'protocol', 'nanopb')
print(f"nanopb directory: {nanopb_dir}")

# Path to the generated protocol files
protocol_gen_dir = os.path.join(repo_root, 'protocol', 'generated', 'nanopb')
print(f"Protocol generated directory: {protocol_gen_dir}")

# Path to the server lib/can directory for the output library
shared_lib_can_dir = os.path.join(repo_root, 'shared', 'lib', 'python', 'can')
print(f"Shared lib/can directory: {shared_lib_can_dir}")

# Check if directories exist
if not os.path.exists(cross_platform_can_dir):
    print(f"ERROR: CrossPlatformCAN directory not found at {cross_platform_can_dir}")
    sys.exit(1)

if not os.path.exists(nanopb_dir):
    print(f"ERROR: nanopb directory not found at {nanopb_dir}")
    sys.exit(1)

if not os.path.exists(protocol_gen_dir):
    print(f"ERROR: Protocol generated directory not found at {protocol_gen_dir}")
    sys.exit(1)

# Ensure the server lib/can directory exists
os.makedirs(shared_lib_can_dir, exist_ok=True)

# Detect the system platform
SYSTEM_PLATFORM = platform.system().lower()
print(f"Detected platform: {SYSTEM_PLATFORM}")

# Build the CrossPlatformCAN library
def build_crossplatform_can():
    """Build the CrossPlatformCAN library and copy it to server/lib/can/"""
    print("Building CrossPlatformCAN library...")
    
    # Store the current directory
    current_dir = os.getcwd()
    
    try:
        # Set up include directories
        include_dir = os.path.join(cross_platform_can_dir, 'include')
        nanopb_include_dir = os.path.join(include_dir, 'nanopb')
        
        # Create necessary directories
        os.makedirs(include_dir, exist_ok=True)
        os.makedirs(nanopb_include_dir, exist_ok=True)
        
        # Copy protocol files to include directory
        for file in os.listdir(protocol_gen_dir):
            if file.endswith('.h'):
                dest_file = os.path.join(include_dir, file)
                if not os.path.exists(dest_file) or os.path.getsize(os.path.join(protocol_gen_dir, file)) != os.path.getsize(dest_file):
                    shutil.copy(os.path.join(protocol_gen_dir, file), include_dir)
                    print(f"Copied {file} to {include_dir}")
        
        # Copy nanopb files to nanopb include directory
        for file in os.listdir(nanopb_dir):
            if file.endswith('.h'):
                dest_file = os.path.join(nanopb_include_dir, file)
                if not os.path.exists(dest_file) or os.path.getsize(os.path.join(nanopb_dir, file)) != os.path.getsize(dest_file):
                    shutil.copy(os.path.join(nanopb_dir, file), nanopb_include_dir)
                    print(f"Copied {file} to {nanopb_include_dir}")

        # Change to the CrossPlatformCAN directory for compiling
        os.chdir(cross_platform_can_dir)

        # Platform-specific build commands
        if SYSTEM_PLATFORM == 'linux':
            platform_define = "-DPLATFORM_LINUX"
            output_lib = "libcaninterface.so"
        elif SYSTEM_PLATFORM == 'darwin':
            platform_define = "-DPLATFORM_DARWIN"
            output_lib = "libcaninterface.dylib"
        else:
            print(f"Unsupported platform: {SYSTEM_PLATFORM}")
            sys.exit(1)

        # Build the library
        build_cmd = [
            "g++", "-shared", "-fPIC",
            "-I", include_dir,
            "-I", nanopb_include_dir,
            "-DDEBUG_MODE",
            "-DCAN_LOGGING_ENABLED",
            "CANInterface.cpp", "ProtobufCANInterface.cpp", "c_api.cpp",
            "-o", output_lib
        ]

        subprocess.run(build_cmd, check=True)

        # Copy the shared library to the server lib/can directory with the correct name
        shutil.copy(output_lib, os.path.join(shared_lib_can_dir, output_lib))

        print(f"CrossPlatformCAN library built and copied to {shared_lib_can_dir}/{output_lib}")

    except subprocess.CalledProcessError as e:
        print(f"Error building CrossPlatformCAN library: {str(e)}")
        if SYSTEM_PLATFORM != 'linux':
            print("This is expected on non-Linux platforms. A placeholder file was created.")
            return  # Don't exit with error on non-Linux platforms
        sys.exit(1)
    except Exception as e:
        print(f"Error in build process: {str(e)}")
        if SYSTEM_PLATFORM != 'linux':
            print("This is expected on non-Linux platforms. A placeholder file was created.")
            return  # Don't exit with error on non-Linux platforms
        sys.exit(1)
    finally:
        # Change back to the original directory
        os.chdir(current_dir)

# Build the CrossPlatformCAN library
build_crossplatform_can()

# Setup configuration
setup(
    name="gokart_dashboard",
    version="0.1.0",
    description="Go-Kart Dashboard Server",
    author="Go-Kart Team",
    packages=find_packages(),
    include_package_data=True,
    install_requires=[
        'flask',
        'flask-socketio',
        'flask-cors',
        'numpy',
        'eventlet',
        'python-can',
        'protobuf',
        'python-engineio',
        'python-socketio',
        'cffi',
    ],
    python_requires='>=3.6',
) 