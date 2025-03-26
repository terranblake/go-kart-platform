#ifndef CAN_C_API_H
#define CAN_C_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Handle for the CAN interface
typedef void* can_interface_t;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create a new CAN interface
 * 
 * @param node_id The ID of this node on the CAN network
 * @return Handle to the interface or NULL on failure
 */
can_interface_t can_interface_create(uint32_t node_id);

/**
 * Initialize the CAN interface
 * 
 * @param handle Handle to the interface
 * @param baudrate The CAN bus speed (typically 500000 for 500kbps)
 * @param device The CAN device name (used only on Linux, e.g., "can0")
 * @return true on success, false on failure
 */
bool can_interface_begin(can_interface_t handle, long baudrate, const char* device);

/**
 * Clean up and destroy the CAN interface
 * 
 * @param handle Handle to the interface
 */
void can_interface_destroy(can_interface_t handle);

/**
 * Send a simple message over the CAN bus
 * 
 * @param handle Handle to the interface
 * @param msg_type Type of message (0=command, 1=status)
 * @param comp_type Type of component (0=lights, 1=motors, etc.)
 * @param component_id ID of the component (or 255 for all)
 * @param command_id Command ID to send
 * @param value_type Type of value (0=int8, 1=uint8, etc.)
 * @param value Value to send
 * @return true on success, false on failure
 */
bool can_interface_send_message(
    can_interface_t handle,
    uint8_t msg_type,
    uint8_t comp_type,
    uint8_t component_id,
    uint8_t command_id,
    uint8_t value_type,
    int32_t value
);

/**
 * Start an animation sequence
 * 
 * @param handle Handle to the interface
 * @return true on success, false on failure
 */
bool can_interface_animation_start(can_interface_t handle);

/**
 * Send a single animation frame
 * 
 * @param handle Handle to the interface
 * @param frame_index Index of this frame
 * @param led_data Array of LED data (x, y, r, g, b) for each LED
 * @param led_count Number of LEDs in the frame
 * @return true on success, false on failure
 */
bool can_interface_animation_frame(
    can_interface_t handle,
    uint8_t frame_index,
    const uint8_t* led_data,
    uint8_t led_count
);

/**
 * End an animation sequence
 * 
 * @param handle Handle to the interface
 * @param total_frames Total number of frames in the animation
 * @return true on success, false on failure
 */
bool can_interface_animation_end(
    can_interface_t handle,
    uint8_t total_frames
);

/**
 * Stop the current animation
 * 
 * @param handle Handle to the interface
 * @return true on success, false on failure
 */
bool can_interface_animation_stop(can_interface_t handle);

/**
 * Process incoming messages (should be called regularly)
 * 
 * @param handle Handle to the interface
 */
void can_interface_process(can_interface_t handle);

#ifdef __cplusplus
}
#endif

#endif // CAN_C_API_H 