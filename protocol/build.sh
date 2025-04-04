#!/bin/bash
# Protocol Buffer compilation script for Go-Kart Platform
# This script compiles multiple Protocol Buffer definitions into various language bindings

set -e  # Exit on error

# Directory paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NANOPB_DIR="${SCRIPT_DIR}/nanopb"
OUTPUT_DIR="${SCRIPT_DIR}/generated"
COMPONENT_INCLUDE_DIR="${SCRIPT_DIR}/../components/common/lib/CrossPlatformCAN/include"

# Verify nanopb.proto exists
NANOPB_PROTO_DIR="${NANOPB_DIR}/generator/proto"
if [ ! -f "${NANOPB_PROTO_DIR}/nanopb.proto" ]; then
    echo "Error: nanopb.proto not found at ${NANOPB_PROTO_DIR}/nanopb.proto"
    echo "Make sure nanopb submodule is initialized: git submodule update --init --recursive"
    exit 1
fi

# Required tools
PROTOC="protoc"
NANOPB_GENERATOR="${NANOPB_DIR}/generator/nanopb_generator.py"

# Check for required tools
if ! command -v ${PROTOC} &> /dev/null; then
    echo "Error: protoc is not installed. Please install Protocol Buffers compiler."
    echo "Visit https://github.com/protocolbuffers/protobuf/releases for installation."
    exit 1
fi

if [ ! -f "${NANOPB_GENERATOR}" ]; then
    echo "Error: nanopb_generator.py not found at ${NANOPB_GENERATOR}."
    echo "Please initialize nanopb submodule or download it from https://github.com/nanopb/nanopb"
    exit 1
fi

# Python 3 check
python3 --version > /dev/null 2>&1 || { 
    echo "Error: Python 3 is required but not installed."; 
    exit 1; 
}

# Create output directories if they don't exist
mkdir -p "${OUTPUT_DIR}/python"
mkdir -p "${OUTPUT_DIR}/nanopb"
mkdir -p "${COMPONENT_INCLUDE_DIR}"

# Get all proto files
PROTO_FILES=$(find "${SCRIPT_DIR}" -maxdepth 1 -name "*.proto")

if [ -z "${PROTO_FILES}" ]; then
    echo "No .proto files found in ${SCRIPT_DIR}"
    exit 1
fi

echo "Found the following protocol files:"
for proto in ${PROTO_FILES}; do
    echo "  - $(basename ${proto}) ${proto}"
done

echo ""

for proto in ${PROTO_FILES}; do
    echo "[$(basename ${proto})] Compiling Protocol Buffer definitions..."

    PROTO_FILE="${SCRIPT_DIR}/$(basename ${proto})"

    # Generate Python bindings
    echo "[$(basename ${proto})] Generating Python bindings..."
    ${PROTOC} -I="${SCRIPT_DIR}" -I="${NANOPB_DIR}/generator/proto" \
        --python_out="${OUTPUT_DIR}/python" "${PROTO_FILE}"
    echo "[$(basename ${proto})] Python bindings generated successfully at ${OUTPUT_DIR}/python"

    # Generate nanopb bindings
    echo "[$(basename ${proto})] Generating nanopb (C/C++) bindings at ${OUTPUT_DIR}/nanopb..."
    python "${NANOPB_DIR}/generator/nanopb_generator.py" \
        "${PROTO_FILE}" \
        -I "${SCRIPT_DIR}" \
        -I "${NANOPB_DIR}/generator/proto" \
        -D "${OUTPUT_DIR}/nanopb"

    echo "[$(basename ${proto})] Nanopb bindings generated successfully at ${OUTPUT_DIR}/nanopb"
    echo ""
done

# Copy the generated headers to the components include directory
echo "Copying generated headers to components directory..."
cp -f "${OUTPUT_DIR}/nanopb/"*.h "${COMPONENT_INCLUDE_DIR}/"
echo "Headers copied to ${COMPONENT_INCLUDE_DIR}"

echo "Protocol compilation complete! yay!"
exit 0