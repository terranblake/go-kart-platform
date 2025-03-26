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

// Function declarations for functions we'll test
void resetAnimationState();
void displayAnimationFrame(uint32_t frameIndex);
void updateAnimation();

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

// Test resetAnimationState function
void test_reset_animation_state(void) {
  // First set some values in animation state
  animationState.active = true;
  animationState.frameCount = 10;
  animationState.currentFrame = 5;
  animationState.lastFrameTime = 1000;
  animationState.dataSize = 100;
  animationState.frameSize = 20;
  
  // Set some data in the animation buffer
  for (int i = 0; i < 100; i++) {
    animationState.animationData[i] = i % 255;
  }
  
  // Now reset and verify all values are reset
  resetAnimationState();
  
  TEST_ASSERT_EQUAL_INT(false, animationState.active);
  TEST_ASSERT_EQUAL_INT(0, animationState.frameCount);
  TEST_ASSERT_EQUAL_INT(0, animationState.currentFrame);
  TEST_ASSERT_EQUAL_INT(0, animationState.lastFrameTime);
  TEST_ASSERT_EQUAL_INT(0, animationState.dataSize);
  TEST_ASSERT_EQUAL_INT(0, animationState.frameSize);
  
  // Check that buffer is cleared
  for (int i = 0; i < 10; i++) {
    TEST_ASSERT_EQUAL_INT(0, animationState.animationData[i]);
  }
}

// Test displayAnimationFrame function
void test_display_animation_frame(void) {
  // Prepare animation data (3 bytes per LED for RGB)
  const uint16_t testFrameSize = (FRONT_END_LED - FRONT_START_LED + 1) * 3;
  
  // Setup animation state
  animationState.active = true;
  animationState.frameCount = 2;
  animationState.frameSize = testFrameSize;
  
  // Fill first frame with red data
  for (int i = 0; i < testFrameSize; i += 3) {
    animationState.animationData[i] = 255;     // R
    animationState.animationData[i + 1] = 0;   // G
    animationState.animationData[i + 2] = 0;   // B
  }
  
  // Fill second frame with blue data
  for (int i = 0; i < testFrameSize; i += 3) {
    animationState.animationData[testFrameSize + i] = 0;       // R
    animationState.animationData[testFrameSize + i + 1] = 0;   // G
    animationState.animationData[testFrameSize + i + 2] = 255; // B
  }
  
  // Clear current LEDs
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
  
  // Display first frame
  displayAnimationFrame(0);
  
  // Check that front LEDs are red
  for (int i = FRONT_START_LED; i <= FRONT_END_LED; i++) {
    TEST_ASSERT_EQUAL_INT(255, leds[i].r);
    TEST_ASSERT_EQUAL_INT(0, leds[i].g);
    TEST_ASSERT_EQUAL_INT(0, leds[i].b);
  }
  
  // Display second frame
  displayAnimationFrame(1);
  
  // Check that front LEDs are blue
  for (int i = FRONT_START_LED; i <= FRONT_END_LED; i++) {
    TEST_ASSERT_EQUAL_INT(0, leds[i].r);
    TEST_ASSERT_EQUAL_INT(0, leds[i].g);
    TEST_ASSERT_EQUAL_INT(255, leds[i].b);
  }
  
  // Now test with rear lights
  updateFrontLights = false;
  
  // Reset animation state to create new test frames
  resetAnimationState();
  
  // Prepare animation data for rear lights
  const uint16_t rearTestFrameSize = (REAR_END_LED - REAR_START_LED + 1) * 3;
  
  // Setup animation state
  animationState.active = true;
  animationState.frameCount = 2;
  animationState.frameSize = rearTestFrameSize;
  
  // Fill first frame with green data
  for (int i = 0; i < rearTestFrameSize; i += 3) {
    animationState.animationData[i] = 0;     // R
    animationState.animationData[i + 1] = 255;   // G
    animationState.animationData[i + 2] = 0;   // B
  }
  
  // Fill second frame with purple data
  for (int i = 0; i < rearTestFrameSize; i += 3) {
    animationState.animationData[rearTestFrameSize + i] = 255;       // R
    animationState.animationData[rearTestFrameSize + i + 1] = 0;   // G
    animationState.animationData[rearTestFrameSize + i + 2] = 255; // B
  }
  
  // Clear current LEDs
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB::Black;
  }
  
  // Display first frame (green)
  displayAnimationFrame(0);
  
  // Check that rear LEDs are green
  for (int i = REAR_START_LED; i <= REAR_END_LED; i++) {
    TEST_ASSERT_EQUAL_INT(0, leds[i].r);
    TEST_ASSERT_EQUAL_INT(255, leds[i].g);
    TEST_ASSERT_EQUAL_INT(0, leds[i].b);
  }
  
  // Display second frame (purple)
  displayAnimationFrame(1);
  
  // Check that rear LEDs are purple
  for (int i = REAR_START_LED; i <= REAR_END_LED; i++) {
    TEST_ASSERT_EQUAL_INT(255, leds[i].r);
    TEST_ASSERT_EQUAL_INT(0, leds[i].g);
    TEST_ASSERT_EQUAL_INT(255, leds[i].b);
  }
}

// Helper function to implement the test animation function
void test_update_anim_helper(void) {
  // Mock the millis() function by updating lastFrameTime
  // so it appears enough time has passed for a frame update
  animationState.lastFrameTime = millis() - (animationConfig.frameDuration + 10);
  
  // Call the animation update function
  updateAnimation();
}

// Test updateAnimation function
void test_update_animation(void) {
  // Prepare animation data
  const uint16_t testFrameSize = (FRONT_END_LED - FRONT_START_LED + 1) * 3;
  
  // Setup animation state
  animationState.active = true;
  animationState.frameCount = 3;
  animationState.currentFrame = 0;
  animationState.frameSize = testFrameSize;
  animationState.lastFrameTime = millis();
  
  // Setup animation config
  animationConfig.fps = 10;
  animationConfig.frameDuration = 100; // 100ms per frame
  animationConfig.loopAnimation = true;
  
  // Fill frames with different colors
  // Frame 0: Red
  for (int i = 0; i < testFrameSize; i += 3) {
    animationState.animationData[i] = 255;     // R
    animationState.animationData[i + 1] = 0;   // G
    animationState.animationData[i + 2] = 0;   // B
  }
  
  // Frame 1: Green
  for (int i = 0; i < testFrameSize; i += 3) {
    animationState.animationData[testFrameSize + i] = 0;       // R
    animationState.animationData[testFrameSize + i + 1] = 255; // G
    animationState.animationData[testFrameSize + i + 2] = 0;   // B
  }
  
  // Frame 2: Blue
  for (int i = 0; i < testFrameSize; i += 3) {
    animationState.animationData[2 * testFrameSize + i] = 0;       // R
    animationState.animationData[2 * testFrameSize + i + 1] = 0;   // G
    animationState.animationData[2 * testFrameSize + i + 2] = 255; // B
  }
  
  // Set current mode to animation
  lightState.mode = kart_lights_LightModeValue_ANIMATION;
  
  // Call update (should move to frame 1)
  test_update_anim_helper();
  
  // Verify we moved to frame 1
  TEST_ASSERT_EQUAL_INT(1, animationState.currentFrame);
  
  // Verify LEDs show green (frame 1)
  TEST_ASSERT_EQUAL_INT(0, leds[FRONT_START_LED].r);
  TEST_ASSERT_EQUAL_INT(255, leds[FRONT_START_LED].g);
  TEST_ASSERT_EQUAL_INT(0, leds[FRONT_START_LED].b);
  
  // Test animation looping
  animationState.currentFrame = 2;
  
  // Update animation (should loop back to frame 0)
  test_update_anim_helper();
  
  // Verify we looped back to frame 0
  TEST_ASSERT_EQUAL_INT(0, animationState.currentFrame);
  
  // Verify LEDs show red (frame 0)
  TEST_ASSERT_EQUAL_INT(255, leds[FRONT_START_LED].r);
  TEST_ASSERT_EQUAL_INT(0, leds[FRONT_START_LED].g);
  TEST_ASSERT_EQUAL_INT(0, leds[FRONT_START_LED].b);
  
  // Test animation stopping at end when not looping
  animationConfig.loopAnimation = false;
  animationState.currentFrame = 2;
  
  // Update animation (should stop at end)
  test_update_anim_helper();
  
  // Verify animation stopped
  TEST_ASSERT_EQUAL_INT(false, animationState.active);
  TEST_ASSERT_EQUAL_INT(kart_lights_LightModeValue_OFF, lightState.mode);
}

void setup() {
  delay(2000); // Give board time to settle
  
  UNITY_BEGIN();
  
  RUN_TEST(test_reset_animation_state);
  RUN_TEST(test_display_animation_frame);
  RUN_TEST(test_update_animation);
  
  UNITY_END();
}

void loop() {
  // Nothing to do
} 