#include "../include/AnimationProtocol.h"

AnimationProtocol::AnimationProtocol() 
    : streamState(STREAM_STATE_IDLE),
      currentFrame(0),
      numFrames(0),
      fps(30),
      targetNumLeds(0),
      brightness(DEFAULT_BRIGHTNESS),
      mode(MODE_OFF),
      frameBufferPos(0),
      expectedFrameSize(0),
      leds(nullptr),
      numLeds(0),
      lastFrameTime(0),
      frameInterval(33), // Default to ~30fps (33ms)
      receivedFrames(0),
      droppedFrames(0),
      memoryUsage(0),
      frameReady(false)
{
    // Initialize frame buffer to zeros
    memset(frameBuffer, 0, MAX_FRAME_BUFFER);
}

void AnimationProtocol::begin(CRGB* ledArray, uint16_t ledCount) {
    leds = ledArray;
    numLeds = ledCount;
    reset();
    
#if DEBUG_MODE
    Serial.println(F("Animation protocol initialized"));
    Serial.print(F("Max frame size: "));
    Serial.print(MAX_FRAME_SIZE);
    Serial.print(F(" bytes, Buffer: "));
    Serial.print(MAX_FRAME_BUFFER);
    Serial.println(F(" bytes"));
#endif
}

bool AnimationProtocol::processMessage(uint32_t canId, uint8_t* data, uint8_t length) {
    // Check if it's an animation message
    if (canId == ANIM_CTRL_ID) {
        handleControlMessage(data, length);
        return true;
    } else if (canId == ANIM_DATA_ID) {
        handleDataMessage(data, length);
        return true;
    }
    
    // Not an animation message
    return false;
}

void AnimationProtocol::handleControlMessage(uint8_t* data, uint8_t length) {
    if (length < 1) return;
    
    uint8_t cmd = data[0];
    
    switch (cmd) {
        case CMD_STREAM_START:
            if (length >= 5) {
                // Extract parameters: numFrames, fps, numLeds (2 bytes)
                uint8_t numFramesVal = data[1];
                uint8_t fpsVal = data[2];
                uint16_t numLedsVal = data[3] | (uint16_t(data[4]) << 8);
                startStream(numFramesVal, fpsVal, numLedsVal);
            }
            break;
            
        case CMD_STREAM_END:
            endStream();
            break;
            
        case CMD_FRAME_START:
            if (length >= 4) {
                // Extract parameters: frameNum, frameSize (2 bytes)
                uint8_t frameNum = data[1];
                uint16_t frameSize = data[2] | (uint16_t(data[3]) << 8);
                startFrame(frameNum, frameSize);
            }
            break;
            
        case CMD_FRAME_END:
            if (length >= 2) {
                uint8_t frameNum = data[1];
                endFrame(frameNum);
            }
            break;
            
        case CMD_CONFIG:
            if (length >= 2) {
                handleConfigMessage(data + 1, length - 1);
            }
            break;
            
        default:
            // Unknown command
            break;
    }
}

void AnimationProtocol::handleConfigMessage(uint8_t* data, uint8_t length) {
    if (length < 1) return;
    
    uint8_t configId = data[0];
    
    switch (configId) {
        case CONFIG_FPS:
            if (length >= 2) {
                fps = data[1];
                frameInterval = fps > 0 ? (1000 / fps) : 33; // 33ms ~= 30fps
#if DEBUG_MODE
                Serial.print(F("Set FPS: "));
                Serial.print(fps);
                Serial.print(F(" ("));
                Serial.print(frameInterval);
                Serial.println(F("ms)"));
#endif
            }
            break;
            
        case CONFIG_NUM_LEDS:
            if (length >= 3) {
                targetNumLeds = data[1] | (uint16_t(data[2]) << 8);
#if DEBUG_MODE
                Serial.print(F("Set num LEDs: "));
                Serial.println(targetNumLeds);
#endif
            }
            break;
            
        case CONFIG_BRIGHTNESS:
            if (length >= 2) {
                brightness = data[1];
                FastLED.setBrightness(brightness);
#if DEBUG_MODE
                Serial.print(F("Set brightness: "));
                Serial.println(brightness);
#endif
            }
            break;
            
        case CONFIG_MODE:
            if (length >= 2) {
                mode = data[1];
#if DEBUG_MODE
                Serial.print(F("Set mode: "));
                Serial.println(mode);
#endif
            }
            break;
            
        default:
            // Unknown config parameter
            break;
    }
}

void AnimationProtocol::handleDataMessage(uint8_t* data, uint8_t length) {
    // Only process data if we're in frame reception mode
    if (streamState != STREAM_STATE_FRAME) {
#if DEBUG_MODE
        Serial.println(F("Received data outside frame context"));
#endif
        return;
    }
    
    // Check if we have space in the buffer
    if (frameBufferPos + length > MAX_FRAME_BUFFER) {
#if DEBUG_MODE
        Serial.println(F("Frame buffer overflow"));
#endif
        droppedFrames++;
        return;
    }
    
    // Copy data to frame buffer
    memcpy(frameBuffer + frameBufferPos, data, length);
    frameBufferPos += length;
    
    // Check if we've received the complete frame
    if (frameBufferPos >= expectedFrameSize) {
#if DEBUG_MODE
        Serial.print(F("Received complete frame: "));
        Serial.print(frameBufferPos);
        Serial.print(F("/"));
        Serial.print(expectedFrameSize);
        Serial.println(F(" bytes"));
#endif

        // Mark frame as ready for processing
        frameReady = true;
    }
}

void AnimationProtocol::startStream(uint8_t numFramesVal, uint8_t fpsVal, uint16_t numLedsVal) {
    // Only start a new stream if we're idle
    if (streamState != STREAM_STATE_IDLE) {
        endStream(); // End the current stream first
    }
    
    numFrames = numFramesVal;
    fps = fpsVal;
    targetNumLeds = numLedsVal;
    
    // Limit target LEDs to actual LED strip length
    if (targetNumLeds > numLeds) {
        targetNumLeds = numLeds;
    }
    
    // Calculate frame interval in milliseconds
    frameInterval = fps > 0 ? (1000 / fps) : 33; // Default to ~30fps
    
    streamState = STREAM_STATE_ACTIVE;
    receivedFrames = 0;
    droppedFrames = 0;
    
    FastLED.setBrightness(brightness);
    mode = MODE_ANIMATION;
    
#if DEBUG_MODE
    Serial.print(F("Starting animation stream: "));
    Serial.print(numFrames);
    Serial.print(F(" frames, "));
    Serial.print(fps);
    Serial.print(F(" FPS, "));
    Serial.print(targetNumLeds);
    Serial.println(F(" LEDs"));
#endif
}

void AnimationProtocol::endStream() {
    if (streamState == STREAM_STATE_IDLE) {
        return; // Already idle
    }
    
    streamState = STREAM_STATE_IDLE;
    frameBufferPos = 0;
    currentFrame = 0;
    frameReady = false;
    
#if DEBUG_MODE
    Serial.println(F("Ending animation stream"));
    Serial.print(F("Received frames: "));
    Serial.print(receivedFrames);
    Serial.print(F(" Dropped frames: "));
    Serial.println(droppedFrames);
#endif
    
    // Send acknowledgment message (if needed in the future)
}

void AnimationProtocol::startFrame(uint8_t frameNum, uint16_t frameSize) {
    if (streamState != STREAM_STATE_ACTIVE) {
#if DEBUG_MODE
        Serial.println(F("Can't start frame: no active stream"));
#endif
        return;
    }
    
    // Check if frame size is reasonable
    if (frameSize > MAX_FRAME_BUFFER) {
#if DEBUG_MODE
        Serial.print(F("Frame too large: "));
        Serial.print(frameSize);
        Serial.print(F(" > "));
        Serial.println(MAX_FRAME_BUFFER);
#endif
        droppedFrames++;
        return;
    }
    
    currentFrame = frameNum;
    expectedFrameSize = frameSize;
    frameBufferPos = 0;
    frameReady = false;
    streamState = STREAM_STATE_FRAME;
    
#if DEBUG_MODE
    Serial.print(F("Starting frame "));
    Serial.print(frameNum);
    Serial.print(F(", "));
    Serial.print(frameSize);
    Serial.println(F(" bytes"));
#endif
}

void AnimationProtocol::endFrame(uint8_t frameNum) {
    if (streamState != STREAM_STATE_FRAME) {
#if DEBUG_MODE
        Serial.println(F("Can't end frame: no active frame"));
#endif
        return;
    }
    
    // Check if frame number matches
    if (frameNum != currentFrame) {
#if DEBUG_MODE
        Serial.print(F("Frame number mismatch: expected "));
        Serial.print(currentFrame);
        Serial.print(F(", got "));
        Serial.println(frameNum);
#endif
    }
    
    // Process frame data if it's ready
    if (frameReady) {
        processFrameData();
        receivedFrames++;
    } else {
#if DEBUG_MODE
        Serial.print(F("Incomplete frame: "));
        Serial.print(frameBufferPos);
        Serial.print(F("/"));
        Serial.print(expectedFrameSize);
        Serial.println(F(" bytes"));
#endif
        droppedFrames++;
    }
    
    // Return to active stream state
    streamState = STREAM_STATE_ACTIVE;
    frameBufferPos = 0;
    frameReady = false;
    
    // Send frame end acknowledgment
    // We'll implement this if needed in the future
    
#if DEBUG_MODE
    Serial.print(F("Ending frame "));
    Serial.println(frameNum);
#endif
}

void AnimationProtocol::processFrameData() {
    // Process the binary frame data and update the LEDs
    // Frame data format is [R0, G0, B0, R1, G1, B1, ...]
    
    // Calculate number of complete RGB values we have
    uint16_t numPixels = frameBufferPos / 3;
    
    // Limit to the actual number of LEDs
    if (numPixels > targetNumLeds) {
        numPixels = targetNumLeds;
    }
    
    if (numPixels > numLeds) {
        numPixels = numLeds;
    }
    
    // Apply the frame data to the LEDs
    for (uint16_t i = 0; i < numPixels; i++) {
        uint16_t pixelStart = i * 3;
        leds[i].r = frameBuffer[pixelStart];
        leds[i].g = frameBuffer[pixelStart + 1];
        leds[i].b = frameBuffer[pixelStart + 2];
    }
    
    // Show the frame immediately
    FastLED.show();
    
    // Update timestamp for this frame
    lastFrameTime = millis();
}

bool AnimationProtocol::update() {
    // This should be called in the main loop
    // Returns true if the animation is still active
    
    if (streamState == STREAM_STATE_IDLE) {
        return false; // No active animation
    }
    
    // Check for timeout
    // In most cases we won't need this since we're displaying frames
    // immediately when they're received
    
    return true;
}

bool AnimationProtocol::isAnimationActive() const {
    return streamState != STREAM_STATE_IDLE;
}

void AnimationProtocol::reset() {
    streamState = STREAM_STATE_IDLE;
    currentFrame = 0;
    frameBufferPos = 0;
    frameReady = false;
    receivedFrames = 0;
    droppedFrames = 0;
    
    // Clear all LEDs
    if (leds && numLeds > 0) {
        fill_solid(leds, numLeds, CRGB::Black);
        FastLED.show();
    }
    
#if DEBUG_MODE
    Serial.println(F("Animation protocol reset"));
#endif
}

uint16_t AnimationProtocol::getMemoryUsage() const {
    // This is a rough estimate of dynamic memory usage
    return sizeof(AnimationProtocol) + frameBufferPos;
}

uint8_t AnimationProtocol::getFPS() const {
    return fps;
}

uint16_t AnimationProtocol::getReceivedFrames() const {
    return receivedFrames;
}

uint16_t AnimationProtocol::getDroppedFrames() const {
    return droppedFrames;
}
