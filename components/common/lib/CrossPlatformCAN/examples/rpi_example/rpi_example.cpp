/**
 * Raspberry Pi example for CrossPlatformCAN library
 * 
 * This example shows how to use the ProtobufCANInterface on Raspberry Pi.
 * 
 * Prerequisites:
 * 1. CAN interface configured: 
 *    sudo ip link set can0 type can bitrate 500000
 *    sudo ip link set up can0
 * 
 * 2. Build this example:
 *    g++ -o rpi_example rpi_example.cpp ../../CANInterface.cpp ../../ProtobufCANInterface.cpp -I../.. -I../../../.. -std=c++11
 * 
 * 3. Run:
 *    ./rpi_example
 */

#include <iostream>
#include <signal.h>
#include <unistd.h>
#include "ProtobufCANInterface.h"

// Define a node ID for this device
#define NODE_ID 0x02

// Create a CAN interface instance
ProtobufCANInterface canInterface(NODE_ID);

// Flag to control program execution
volatile bool running = true;

// Signal handler to handle Ctrl+C
void signalHandler(int signum) {
    std::cout << "Interrupt signal (" << signum << ") received.\n";
    running = false;
}

// Handler for received messages
void messageHandler(kart_common_MessageType msg_type, 
                   kart_common_ComponentType comp_type,
                   uint8_t component_id, uint8_t command_id, 
                   kart_common_ValueType value_type, int32_t value) {
    // Print received message
    std::cout << "Received message: "
              << "Type=" << (int)msg_type
              << ", Component=" << (int)comp_type
              << ", ID=" << (int)component_id
              << ", Command=" << (int)command_id
              << ", Value=" << value << std::endl;
    
    // Example: If this is a lights command, echo the state
    if (comp_type == kart_common_ComponentType_LIGHTS && 
        command_id == 1 && 
        value_type == kart_common_ValueType_BOOLEAN) {
        std::cout << "Light state changed to: " << (value ? "ON" : "OFF") << std::endl;
    }
}

int main() {
    // Set up signal handling for clean shutdown
    signal(SIGINT, signalHandler);
    
    std::cout << "Raspberry Pi CAN Interface Example" << std::endl;
    
    // Initialize CAN interface (500kbps, device "can0")
    if (!canInterface.begin(500000, "can0")) {
        std::cerr << "Failed to initialize CAN interface!" << std::endl;
        return 1;
    }
    
    // Register message handler
    canInterface.registerHandler(kart_common_ComponentType_LIGHTS, // Component type
                               0x01,                             // Component ID
                               0x01,                             // Command ID
                               messageHandler);                  // Handler function
    
    std::cout << "CAN Interface initialized successfully!" << std::endl;
    
    // Main loop
    int counter = 0;
    while (running) {
        // Process incoming CAN messages
        canInterface.process();
        
        // Example: Send a message every second
        static time_t lastSendTime = 0;
        time_t currentTime = time(NULL);
        
        if (currentTime - lastSendTime >= 1) {
            lastSendTime = currentTime;
            
            // Toggle state
            bool ledState = (counter++ % 2) == 0;
            
            // Send the toggle command
            canInterface.sendMessage(
                kart_common_MessageType_COMMAND,     // Message type: Command
                kart_common_ComponentType_LIGHTS,    // Component type: Lights
                0x01,                               // Component ID: 1
                0x01,                               // Command ID: 1 (Toggle)
                kart_common_ValueType_BOOLEAN,      // Value type: Boolean
                ledState                            // Value: true/false
            );
            
            std::cout << "Sent toggle command with value: " << ledState << std::endl;
        }
        
        // Sleep a bit to avoid consuming too much CPU
        usleep(10000); // 10ms
    }
    
    std::cout << "Shutting down..." << std::endl;
    return 0;
} 