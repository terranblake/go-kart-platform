# Python Protocol Bindings

These files were generated from the Protocol Buffer definitions in `/c/Users/terran/Documents/go-kart-platform/protocol/kart.proto`.

## Usage

```python
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
```

Generated on Sat, Mar 22, 2025  5:23:05 PM
