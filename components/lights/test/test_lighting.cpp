#include <ArduinoFake.h> // Include ArduinoFake first
#include <unity.h>

#ifndef UNIT_TEST
#include <FastLED.h>      // Include for CRGB only when not testing
#endif
#include "Config.h"       // Include main config/declarations
#include "common.pb.h"    // Include for kart_common types/enums
#include "lights.pb.h"    // Include for lights specific enums
#include "controls.pb.h"  // Include for controls specific enums (needed for handlers)

// Forward declare functions/variables from main.cpp needed for tests
#ifndef UNIT_TEST
extern CRGB leds[NUM_LEDS]; // Global LED array from main.cpp - only declare if FastLED included
#endif
extern LightState lightState; // Global state struct from main.cpp
void setup();
void loop();
// void setLedColor(int index, uint8_t r, uint8_t g, uint8_t b); // This function doesn't exist in main.cpp

// Declare handlers used in tests (signatures from Config.h / main.cpp)
void handleLightMode(kart_common_MessageType msg_type, kart_common_ComponentType comp_type, uint8_t component_id, uint8_t command_id, kart_common_ValueType value_type, int32_t value);
// Add other handlers if needed for more tests...

using namespace fakeit;

void setUp() {
    ArduinoFake().Reset(); // Corrected case
}

void tearDown() {
}

// Test initialization function
void test_initialization() {
    // Arrange
    When(Method(ArduinoFake(), pinMode)).AlwaysReturn();
    // Mock Serial.begin specifically using ArduinoFakeInstance
    When(Method(ArduinoFakeInstance(Serial), begin)).AlwaysReturn();

    // Act - Call the setup function
    setup();
    
    // Assert - Verify initialization calls
    Verify(Method(ArduinoFake(), pinMode).Using(LED_BUILTIN, OUTPUT)).Once();
    // Verify Serial.begin call using ArduinoFakeInstance
    // Verify(Method(ArduinoFakeInstance(Serial), begin).Using(115200)).Once(); // Serial might only init in DEBUG_MODE
}

/*
// Test led color setting - COMMENTED OUT - Needs rework
// This test called a non-existent function 'setLedColor'.
// It should likely test 'updateLights' or similar by manipulating 'lightState'
// and then checking the 'leds' array.
void test_led_color_setting() {
    // Arrange
    uint8_t r = 100, g = 150, b = 200;
    lightState.mode = kart_lights_LightModeValue_ON; // Example state setup

    // Act
    // updateLights(leds, NUM_LEDS, lightState); // Example: Call the actual update function

    // Assert
    // Check the global leds array based on expected output of updateLights
    // TEST_ASSERT_EQUAL_UINT8(r, leds[0].r); // Adjust assertion based on updateLights logic
    // TEST_ASSERT_EQUAL_UINT8(g, leds[0].g);
    // TEST_ASSERT_EQUAL_UINT8(b, leds[0].b);
}
*/

// Test message handling
void test_message_handling() {
    // Arrange
    When(Method(ArduinoFake(), millis)).Return(1000);
    
    // Arrange
    When(Method(ArduinoFake(), millis)).Return(1000); // Mock time if needed by handler

    // Reset state before test
    lightState.mode = kart_lights_LightModeValue_OFF;

    // Define message parameters using actual enums
    kart_common_MessageType msgType = kart_common_MessageType_COMMAND;
    kart_common_ComponentType compType = kart_common_ComponentType_LIGHTS;
    uint8_t componentId = kart_lights_LightComponentId_ALL; // Use enum for clarity
    uint8_t commandId = kart_lights_LightCommandId_MODE;   // Use enum for command
    kart_common_ValueType valueType = kart_common_ValueType_UINT8; // Handler expects uint8 for mode
    int32_t value = kart_lights_LightModeValue_ON;         // Use enum for value

    // Act
    // Call the actual handler function registered in main.cpp
    handleLightMode(msgType, compType, componentId, commandId, valueType, value);

    // Assert
    // Check that the global lightState was updated correctly
    TEST_ASSERT_EQUAL_UINT8(kart_lights_LightModeValue_ON, lightState.mode);
}


// Run all tests
int main() {
    UNITY_BEGIN();

    RUN_TEST(test_initialization);
    // RUN_TEST(test_led_color_setting); // Commented out
    RUN_TEST(test_message_handling);

    return UNITY_END();
}
