/**
 * CANInterface.h - Cross-platform CAN interface for go-kart platform
 * Supports both Raspberry Pi (using SocketCAN) and Arduino (using Arduino-CAN)
 */

#ifndef CAN_INTERFACE_H
#define CAN_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>

// Platform detection
#if defined(ARDUINO) && !defined(PLATFORM_ESP32)
  // Arduino platform
  #include <Arduino.h>
  #include <CAN.h>
  #include <MCP2515.h>    // Include the autowp MCP2515 library header
  // #define PLATFORM_ESP32 // Rely on build flag
#elif defined(ESP8266) || defined(ESP32) || defined(PLATFORM_ESP32)
  // ESP32 platform using MCP2515 via SPI
  #include <Arduino.h>
  #include <SPI.h>        // Required for MCP2515 library
  #include <MCP2515.h>    // Include the autowp MCP2515 library header
#elif defined(__APPLE__) || defined(__darwin__)
  // macOS/Darwin platform (using UDP Multicast for virtual CAN)
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <errno.h>
  #include <sys/socket.h>
  #include <sys/ioctl.h>
  #include <net/if.h>
  #include <netinet/in.h> // Added for INET sockets
  #include <arpa/inet.h>  // Added for inet_addr
  #define PLATFORM_DARWIN
#else
// #elif defined(__linux__) || defined(__unix__)
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
// #else
//   #error "Unsupported platform - must be either Arduino or Linux"
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
   * Constructor for ESP32/MCP2515
   */
  CANInterface(int csPin, int intPin);

  /**
   * Default constructor for other platforms
   */
  CANInterface();

  /**
   * Initialize the CAN interface
   * 
   * @param baudRate The CAN bus speed (typically 500000 for 500kbps)
   * @param canDevice The CAN device name (used only on Linux, e.g., "can0")
   * @param csPin The CS pin for the CAN transceiver (used only on Arduino)
   * @param intPin The INT pin for the CAN transceiver (used only on Arduino)
   * @return true on success, false on failure
   */
  bool begin(long baudRate = 500000, const char* canDevice = "can0", int csPin = -1, int intPin = -1);
  
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
#ifdef PLATFORM_ESP32
  MCP2515 m_mcp2515; // Instance of the MCP2515 library object
  int m_csPin;       // Store CS pin for MCP2515 constructor
  int m_intPin;      // Store INT pin 
#elif defined PLATFORM_LINUX
  int m_socket;
  struct sockaddr_can m_addr;
  struct ifreq m_ifr;
#elif defined(PLATFORM_DARWIN)
  int m_socket;                // UDP socket file descriptor
  struct sockaddr_in m_multicast_addr; // Multicast group address/port
  struct sockaddr_in m_local_addr;     // Local address/port to bind to
#endif
};

#endif // CAN_INTERFACE_H 