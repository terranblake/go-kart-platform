# Nanopb Protocol Bindings

These files were generated from the Protocol Buffer definitions in `/c/Users/terran/Documents/go-kart-platform/protocol/kart.proto`.

## Usage

```c
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
```

Generated on Sat, Mar 22, 2025  5:23:05 PM
