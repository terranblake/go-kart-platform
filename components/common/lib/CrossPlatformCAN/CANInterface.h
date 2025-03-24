/**
 * CANInterface.h - Cross-platform CAN interface for go-kart platform
 * Supports both Raspberry Pi (using SocketCAN) and Arduino (using Arduino-CAN)
 */

#ifndef CAN_INTERFACE_H
#define CAN_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

// Platform detection
#if defined(ARDUINO)
  // Arduino platform
  #include <Arduino.h>
  #include <SPI.h>
  // We'll use the standard Arduino-CAN library
  #include <CAN.h>
  #define PLATFORM_ARDUINO
#elif defined(__linux__) || defined(__unix__)
  // Linux platform (Raspberry Pi)
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  #include <net/if.h>
  #include <sys/socket.h>
  #include <sys/ioctl.h>
  #include <linux/can.h>
  #include <linux/can/raw.h>
  #define PLATFORM_LINUX
#else
  #error "Unsupported platform - must be either Arduino or Linux"
#endif

// Common CAN message structure
typedef struct {
  uint32_t id;       // CAN ID
  uint8_t length;    // Data length (0-8)
  uint8_t data[8];   // Data bytes
} CANMessage;

class CANInterface {
public:
  /**
   * Initialize the CAN interface
   * 
   * @param baudRate The CAN bus speed (typically 500000 for 500kbps)
   * @param canDevice The CAN device name (used only on Linux, e.g., "can0")
   * @return true on success, false on failure
   */
  bool begin(long baudRate = 500000, const char* canDevice = "can0");
  
  /**
   * Close the CAN interface
   */
  void end();
  
  /**
   * Send a CAN message
   * 
   * @param msg The message to send
   * @return true on success, false on failure
   */
  bool sendMessage(const CANMessage& msg);
  
  /**
   * Receive a CAN message (non-blocking)
   * 
   * @param msg The message structure to fill
   * @return true if a message was received, false if no message available
   */
  bool receiveMessage(CANMessage& msg);
  
  /**
   * Check if a message is available to be read
   * 
   * @return true if a message is available, false otherwise
   */
  bool messageAvailable();

private:
#ifdef PLATFORM_LINUX
  int m_socket;
  struct sockaddr_can m_addr;
  struct ifreq m_ifr;
#endif
};

#endif // CAN_INTERFACE_H 