#!/bin/bash
# Protocol Buffer compilation script for Go-Kart Platform
# This script compiles the Protocol Buffer definitions into various language bindings

set -e  # Exit on error

# Directory paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROTO_DIR="${SCRIPT_DIR}"
NANOPB_DIR="${SCRIPT_DIR}/nanopb"
OUTPUT_DIR="${SCRIPT_DIR}/generated"
PROTO_FILE="${SCRIPT_DIR}/kart.proto"

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

echo "Compiling Protocol Buffer definitions..."

# Generate Python bindings
echo "Generating Python bindings..."
${PROTOC} -I="${PROTO_DIR}" -I="${NANOPB_DIR}/generator/proto" \
    --python_out="${OUTPUT_DIR}/python" "${PROTO_FILE}"
echo "Python bindings generated successfully at ${OUTPUT_DIR}/python"

# Generate nanopb bindings
echo "Generating nanopb (C/C++) bindings at ${OUTPUT_DIR}/nanopb..."
python "${NANOPB_DIR}/generator/nanopb_generator.py" \
    "${PROTO_FILE}" \
    -I "${PROTO_DIR}" \
    -I "${NANOPB_DIR}/generator/proto" \
    -D "${OUTPUT_DIR}/nanopb"
echo "Nanopb bindings generated successfully at ${OUTPUT_DIR}/nanopb"

# Create README files
echo "Creating documentation..."

# Python README
cat > "${OUTPUT_DIR}/python/README.md" << EOF
# Python Protocol Bindings

These files were generated from the Protocol Buffer definitions in \`${PROTO_FILE}\`.

## Usage

\`\`\`python
import kart_pb2

# Create a message
message = kart_pb2.KartMessage()
message.component_type = kart_pb2.COMPONENT_LIGHTS
message.timestamp = int(time.time() * 1000)  # Current time in milliseconds
message.sequence = 1
message.node_id = 10

# Access light command
light_cmd = message.light_command
light_cmd.component_id = kart_pb2.LIGHT_FRONT
light_cmd.mode = kart_pb2.LIGHT_MODE_HIGH

# Serialize to binary
binary_data = message.SerializeToString()

# Deserialize from binary
received_message = kart_pb2.KartMessage()
received_message.ParseFromString(binary_data)
\`\`\`

Generated on $(date)
EOF

# Nanopb README
cat > "${OUTPUT_DIR}/nanopb/README.md" << EOF
# Nanopb Protocol Bindings

These files were generated from the Protocol Buffer definitions in \`${PROTO_FILE}\`.

## Usage

\`\`\`c
#include "kart.pb.h"

// Create a message buffer
uint8_t buffer[128];
pb_ostream_t ostream;
KartMessage message = KartMessage_init_zero;

// Set message fields
message.component_type = ComponentType_COMPONENT_LIGHTS;
message.timestamp = millis();
message.sequence = 1;
message.node_id = 10;

// Set light command
message.which_command = KartMessage_light_command_tag;
message.command.light_command.component_id = LightComponent_LIGHT_FRONT;
message.command.light_command.which_command = LightCommand_mode_tag;
message.command.light_command.command.mode = LightMode_LIGHT_MODE_HIGH;

// Encode the message
ostream = pb_ostream_from_buffer(buffer, sizeof(buffer));
pb_encode(&ostream, KartMessage_fields, &message);

// Message is now in buffer with size ostream.bytes_written
\`\`\`

Generated on $(date)
EOF

echo "Protocol compilation complete!"
exit 0