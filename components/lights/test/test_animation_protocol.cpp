#include <Arduino.h>
#include <unity.h>
#include <FastLED.h>
#include "../include/Config.h"

// Mock objects
CRGB leds[NUM_LEDS];
LightState lightState;
AnimationState animationState;
AnimationConfig animationConfig;
bool updateFrontLights = true;

// Function declarations
void resetAnimationState();
void handleAnimationControl(const void* data, size_t length, kart_common_ComponentType component, uint8_t component_id, uint8_t command_id);
void handleAnimationConfig(const void* data, size_t length, kart_common_ComponentType component, uint8_t component_id, uint8_t command_id);
void processAnimationMessage(const void* data, size_t length, kart_common_ComponentType component, uint8_t component_id, uint8_t command_id);

// Setup function runs before each test
void setUp(void) {
  // Initialize our test state
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
  FastLED.clear();
  
  // Clear light state
  lightState.mode = kart_lights_LightModeValue_OFF;
  lightState.brightness = DEFAULT_BRIGHTNESS;
  lightState.turnLeft = 0;
  lightState.turnRight = 0;
  lightState.hazard = 0;
  lightState.braking = 0;
  lightState.sweepPosition = 0;
  lightState.animation = 0;
  
  // Set default animation config
  animationConfig.fps = DEFAULT_ANIMATION_FPS;
  animationConfig.frameDuration = 1000 / DEFAULT_ANIMATION_FPS;
  animationConfig.loopAnimation = true;
  animationConfig.brightness = DEFAULT_BRIGHTNESS;
  
  // Reset animation state
  resetAnimationState();
  
  // Set update front lights to true for testing
  updateFrontLights = true;
}

// Test animation control handler
void test_handle_animation_control(void) {
  // Setup animation state with some frames
  animationState.frameCount = 10;
  animationState.active = false;
  lightState.mode = kart_lights_LightModeValue_OFF;
  
  // Create test data for start command
  uint8_t start_cmd[] = {1}; // 1 = Start
  
  // Call handler with start command
  handleAnimationControl(start_cmd, sizeof(start_cmd), kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_ANIMATION_CONTROL);
  
  // Verify animation started
  TEST_ASSERT_EQUAL_INT(true, animationState.active);
  TEST_ASSERT_EQUAL_INT(0, animationState.currentFrame);
  TEST_ASSERT_EQUAL_INT(kart_lights_LightModeValue_ANIMATION, lightState.mode);
  
  // Create test data for pause command
  uint8_t pause_cmd[] = {2}; // 2 = Pause
  
  // Call handler with pause command
  handleAnimationControl(pause_cmd, sizeof(pause_cmd), kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_ANIMATION_CONTROL);
  
  // Verify animation paused
  TEST_ASSERT_EQUAL_INT(false, animationState.active);
  
  // Create test data for resume command
  uint8_t resume_cmd[] = {3}; // 3 = Resume
  
  // Call handler with resume command
  handleAnimationControl(resume_cmd, sizeof(resume_cmd), kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_ANIMATION_CONTROL);
  
  // Verify animation resumed
  TEST_ASSERT_EQUAL_INT(true, animationState.active);
  
  // Create test data for stop command
  uint8_t stop_cmd[] = {0}; // 0 = Stop
  
  // Call handler with stop command
  handleAnimationControl(stop_cmd, sizeof(stop_cmd), kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_ANIMATION_CONTROL);
  
  // Verify animation stopped
  TEST_ASSERT_EQUAL_INT(false, animationState.active);
  TEST_ASSERT_EQUAL_INT(kart_lights_LightModeValue_OFF, lightState.mode);
}

// Test animation config handler
void test_handle_animation_config(void) {
  // Create test data for FPS configuration
  uint8_t fps_config[] = {0, 60}; // Type 0 = FPS, Value = 60
  
  // Call handler with FPS configuration
  handleAnimationConfig(fps_config, sizeof(fps_config), kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_ANIMATION_CONFIG);
  
  // Verify FPS config updated
  TEST_ASSERT_EQUAL_INT(60, animationConfig.fps);
  TEST_ASSERT_EQUAL_INT(1000/60, animationConfig.frameDuration);
  
  // Create test data for loop configuration
  uint8_t loop_config[] = {1, 0}; // Type 1 = Loop, Value = 0 (disabled)
  
  // Call handler with loop configuration
  handleAnimationConfig(loop_config, sizeof(loop_config), kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_ANIMATION_CONFIG);
  
  // Verify loop config updated
  TEST_ASSERT_EQUAL_INT(false, animationConfig.loopAnimation);
  
  // Create test data for brightness configuration
  uint8_t brightness_config[] = {2, 150}; // Type 2 = Brightness, Value = 150
  
  // Call handler with brightness configuration
  handleAnimationConfig(brightness_config, sizeof(brightness_config), kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_ANIMATION_CONFIG);
  
  // Verify brightness config updated
  TEST_ASSERT_EQUAL_INT(150, animationConfig.brightness);
}

// Test animation message handler
void test_process_animation_message(void) {
  // Reset animation state
  resetAnimationState();
  
  // Create a test header packet
  // Format: [0xFF, 0xAA, frameCount(2), frameSize(2), totalSize(2), ...]
  uint8_t header_packet[20] = {
    0xFF, 0xAA,         // Header magic
    0x00, 0x02,         // Frame count (2)
    0x00, 0x06,         // Frame size (6 bytes, for 2 pixels)
    0x00, 0x0C,         // Total size (12 bytes)
    0xFF, 0x00, 0x00,   // First frame pixel 1 (red)
    0x00, 0xFF, 0x00    // First frame pixel 2 (green)
  };
  
  // Process header packet
  processAnimationMessage(header_packet, sizeof(header_packet), kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_ANIMATION_DATA);
  
  // Verify animation state was initialized
  TEST_ASSERT_EQUAL_INT(2, animationState.frameCount);
  TEST_ASSERT_EQUAL_INT(6, animationState.frameSize);
  TEST_ASSERT_EQUAL_INT(12, animationState.dataSize);
  
  // Verify first frame data was stored
  TEST_ASSERT_EQUAL_INT(0xFF, animationState.animationData[0]);
  TEST_ASSERT_EQUAL_INT(0x00, animationState.animationData[1]);
  TEST_ASSERT_EQUAL_INT(0x00, animationState.animationData[2]);
  TEST_ASSERT_EQUAL_INT(0x00, animationState.animationData[3]);
  TEST_ASSERT_EQUAL_INT(0xFF, animationState.animationData[4]);
  TEST_ASSERT_EQUAL_INT(0x00, animationState.animationData[5]);
  
  // Create continuation packet with second frame
  uint8_t continuation_packet[6] = {
    0x00, 0x00, 0xFF,   // Second frame pixel 1 (blue)
    0xFF, 0xFF, 0x00    // Second frame pixel 2 (yellow)
  };
  
  // Process continuation packet
  processAnimationMessage(continuation_packet, sizeof(continuation_packet), kart_common_ComponentType_LIGHTS, kart_lights_LightComponentId_ALL, kart_lights_LightCommandId_ANIMATION_DATA);
  
  // Verify all data was stored
  TEST_ASSERT_EQUAL_INT(12, animationState.dataSize);
  
  // Verify second frame data was stored
  TEST_ASSERT_EQUAL_INT(0x00, animationState.animationData[6]);
  TEST_ASSERT_EQUAL_INT(0x00, animationState.animationData[7]);
  TEST_ASSERT_EQUAL_INT(0xFF, animationState.animationData[8]);
  TEST_ASSERT_EQUAL_INT(0xFF, animationState.animationData[9]);
  TEST_ASSERT_EQUAL_INT(0xFF, animationState.animationData[10]);
  TEST_ASSERT_EQUAL_INT(0x00, animationState.animationData[11]);
  
  // Verify animation is active and ready to play
  TEST_ASSERT_EQUAL_INT(true, animationState.active);
  TEST_ASSERT_EQUAL_INT(0, animationState.currentFrame);
}

void setup() {
  delay(2000); // Give board time to settle
  
  UNITY_BEGIN();
  
  RUN_TEST(test_handle_animation_control);
  RUN_TEST(test_handle_animation_config);
  RUN_TEST(test_process_animation_message);
  
  UNITY_END();
}

void loop() {
  // Nothing to do
} 