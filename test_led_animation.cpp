/**
 * LED Animation Test using the actual CrossPlatformCAN library
 * This will send animation data to the Arduino using the same protocol implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

// Define platform before including library
#define PLATFORM_LINUX

// Include the CrossPlatformCAN library
#include "components/common/lib/CrossPlatformCAN/ProtobufCANInterface.h"

// Define the protocol constants manually since we can't include the .pb.h files
// These match the values in the Arduino code
#define kart_common_MessageType_COMMAND 0
#define kart_common_ComponentType_LIGHTS 0
#define kart_lights_LightComponentId_ALL 255
#define kart_lights_LightCommandId_MODE 0
#define kart_lights_LightCommandId_ANIMATION_START 10
#define kart_lights_LightCommandId_ANIMATION_FRAME 11
#define kart_lights_LightCommandId_ANIMATION_END 12
#define kart_common_ValueType_UINT8 2
#define kart_lights_LightModeValue_ON 1
#define kart_lights_LightModeValue_OFF 0

int main(int argc, char** argv) {
    printf("LED Animation Test Program\n");
    printf("Using CrossPlatformCAN library directly\n");

    // Create the CAN interface with ID 0x001
    ProtobufCANInterface canInterface(0x001);
    
    // Initialize the CAN interface
    if (!canInterface.begin(500000, "can0")) {
        printf("Failed to initialize CAN interface\n");
        return 1;
    }
    
    printf("CAN interface initialized successfully\n");

    // Step 1: Turn on the lights
    printf("Turning on lights...\n");
    canInterface.sendMessage(
        kart_common_MessageType_COMMAND,
        kart_common_ComponentType_LIGHTS,
        kart_lights_LightComponentId_ALL,
        kart_lights_LightCommandId_MODE,
        kart_common_ValueType_UINT8,
        kart_lights_LightModeValue_ON);
    sleep(1);
    
    // Step 2: Start animation sequence
    printf("Starting animation sequence...\n");
    canInterface.sendMessage(
        kart_common_MessageType_COMMAND,
        kart_common_ComponentType_LIGHTS,
        kart_lights_LightComponentId_ALL,
        kart_lights_LightCommandId_ANIMATION_START,
        kart_common_ValueType_UINT8,
        0);
    sleep(1);
    
    // Step 3: Send animation frame
    printf("Sending animation frame...\n");
    
    // Create a test frame with 3 LEDs
    uint8_t frameData[14]; // 2-byte header + 4 bytes per LED * 3 LEDs
    
    // Frame header
    frameData[0] = 0;      // Frame index
    frameData[1] = 3;      // Number of LEDs
    
    // LED 0: Red
    frameData[2] = 0;      // Position 0
    frameData[3] = 255;    // R
    frameData[4] = 0;      // G
    frameData[5] = 0;      // B
    
    // LED 1: Green
    frameData[6] = 1;      // Position 1
    frameData[7] = 0;      // R
    frameData[8] = 255;    // G
    frameData[9] = 0;      // B
    
    // LED 2: Blue
    frameData[10] = 2;     // Position 2
    frameData[11] = 0;     // R
    frameData[12] = 0;     // G
    frameData[13] = 255;   // B
    
    // Send the frame via binary data handler
    canInterface.sendBinaryData(
        kart_common_MessageType_COMMAND,
        kart_common_ComponentType_LIGHTS,
        kart_lights_LightComponentId_ALL,
        kart_lights_LightCommandId_ANIMATION_FRAME,
        kart_common_ValueType_UINT8,
        frameData,
        sizeof(frameData));
    sleep(1);
    
    // Step 4: End animation with frame count = 1
    printf("Ending animation sequence...\n");
    canInterface.sendMessage(
        kart_common_MessageType_COMMAND,
        kart_common_ComponentType_LIGHTS,
        kart_lights_LightComponentId_ALL,
        kart_lights_LightCommandId_ANIMATION_END,
        kart_common_ValueType_UINT8,
        1);  // 1 frame
    
    // Wait to observe animation
    printf("Waiting 5 seconds for animation to play...\n");
    sleep(5);
    
    // Step 5: Turn off lights
    printf("Turning off lights...\n");
    canInterface.sendMessage(
        kart_common_MessageType_COMMAND,
        kart_common_ComponentType_LIGHTS,
        kart_lights_LightComponentId_ALL,
        kart_lights_LightCommandId_MODE,
        kart_common_ValueType_UINT8,
        kart_lights_LightModeValue_OFF);
    
    printf("Test completed successfully!\n");
    
    return 0;
} 