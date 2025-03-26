# Lights Component

The Lights component controls all lighting functions for the go-kart platform including front/rear lights, turn signals, underglow, and RGB LED strips.

## RGB Stream Protocol

The RGB Stream protocol enables high-performance updates to RGB LED strips at up to 30fps for smooth animations and effects.

### Implementation Notes

1. The protocol uses a lightweight binary format over CAN to efficiently transmit RGB data
2. RGB LED data is segmented into chunks of 2 LEDs per CAN message
3. Each message contains segment information to allow out-of-order reconstruction
4. The protocol supports up to 60 LEDs at 30fps using approximately 11.5% of a 500kbps CAN bus

### Recent Fixes

- Protocol Buffer enums are now properly included from the generated headers
- Fixed conditional include paths to support cross-platform compilation
- Improved segmentation and reassembly for better performance

### Usage

To send RGB data to a light component:

1. Prepare your RGB buffer with R, G, B values for each LED
2. Call `sendRGBStream()` with the component ID and buffer
3. Register an RGB stream handler to receive data on the receiving end

For testing and development, use the `rgb_sender.py` script in the tools directory. 