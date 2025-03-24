#include "ProtobufCANInterface.h"
#include <iostream>

int main() {
    std::cout << "Testing ProtobufCANInterface in non-Arduino environment" << std::endl;
    
    // Create a CAN interface instance with node ID 1
    ProtobufCANInterface can(1);
    
    // Initialize with default baud rate
    can.begin();
    
    // Send a test message
    bool result = can.sendMessage(
        kart_common_MessageType_COMMAND,
        kart_common_ComponentType_LIGHTS,
        1,  // component_id (e.g., front lights)
        0,  // command_id (e.g., mode)
        kart_common_ValueType_UINT8,
        1   // value (e.g., ON)
    );
    
    std::cout << "Message send result: " << (result ? "Success" : "Failed") << std::endl;
    
    // Process any incoming messages (would be a no-op in this environment)
    can.process();
    
    return 0;
} 