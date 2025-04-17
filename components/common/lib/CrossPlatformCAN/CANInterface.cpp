/**
 * CANInterface.cpp - Implementation of cross-platform CAN interface
 */

#include "CANInterface.h"

// Constructors
#ifdef PLATFORM_ESP32
CANInterface::CANInterface(int csPin, int intPin) : 
  m_mcp2515(csPin), // Initialize MCP2515 with CS pin
  m_csPin(csPin), 
  m_intPin(intPin) 
{
  // Constructor body (if needed)
}
CANInterface::CANInterface() : 
  m_mcp2515(5) // Default CS pin if default constructor is somehow called on ESP32
{
  // Default constructor for non-ESP32 platforms or error case
  Serial.println("Warning: Default CANInterface constructor called on ESP32?");
}
#else 
// Default constructor for non-ESP32 platforms
CANInterface::CANInterface() {
  // Initialization for other platforms if needed (e.g., Linux socket = -1)
  #ifdef PLATFORM_LINUX
  m_socket = -1; 
  #endif
}
// Dummy constructor for ESP32 signature matching when ESP32 is not defined
// Needed to prevent compile errors when ProtobufCANInterface calls this constructor
// on non-ESP32 platforms.
CANInterface::CANInterface(int csPin, int intPin) {
    #ifdef PLATFORM_LINUX
    m_socket = -1;
    #endif
}
#endif

// Remove global CAN_cfg for ESP32, it's not used by MCP2515 lib
//#ifdef PLATFORM_ESP32
//CAN_device_t CAN_cfg;
//#endif

// Platform-specific implementations
bool CANInterface::begin(long baudRate, const char* canDevice, int csPin, int intPin) {
#if defined(PLATFORM_ARDUINO) && !defined(PLATFORM_ESP32)
  // Arduino implementation using Arduino-CAN library
  if (!CAN.begin(baudRate)) {
    return false;
  }
  
  if (csPin && intPin) {
    CAN.setPins(csPin, intPin);
  } else {
    Serial.println("CAN_CS_PIN or CAN_INT_PIN is not set. Using default pins (CS=2, INT=10) from CAN library");
  }

  return true;
#elif defined PLATFORM_ESP32
  // ESP32 implementation using MCP2515 library
  // Object is already constructed with csPin via CANInterface constructor
  // Store pins passed to begin() - although library uses the one from constructor primarily
  m_csPin = csPin;  
  m_intPin = intPin;

  Serial.printf("Initializing MCP2515 (CS=%d, INT=%d)...\n", m_csPin, m_intPin);

  // Reset the MCP2515 controller
  m_mcp2515.reset();

  // Set the CAN speed
  // Assumes a 16MHz crystal on the MCP2515 module
  // Library uses CAN_SPEED constants like CAN_500KBPS
  // We need to map the long baudRate to the library's constants
  // NOTE: Use the library's CAN_SPEED enum directly!
  CAN_SPEED speed_enum;
  switch(baudRate) {
    case 1000000: speed_enum = CAN_1000KBPS; break;
    case 500000:  speed_enum = CAN_500KBPS; break;
    case 250000:  speed_enum = CAN_250KBPS; break;
    case 125000:  speed_enum = CAN_125KBPS; break;
    // Add other speeds if needed
    default:      
      Serial.printf("Warning: Unsupported baudrate %ld, defaulting to 500kbps\n", baudRate);
      speed_enum = CAN_500KBPS; 
      break;
  }

  // Use the library's CAN_CLOCK enum for crystal frequency
  if (m_mcp2515.setBitrate(speed_enum, MCP_16MHZ) != MCP2515::ERROR_OK) {
    Serial.println("Error setting MCP2515 bitrate.");
    return false;
  }

  // Set MCP2515 to normal mode
  if (m_mcp2515.setNormalMode() != MCP2515::ERROR_OK) {
    Serial.println("Error setting MCP2515 to normal mode.");
    return false;
  }

  // Configure the INT pin if provided
  if (m_intPin >= 0) {
    pinMode(m_intPin, INPUT_PULLUP); // Configure INT pin as input
    // Optional: Attach interrupt if needed, but library often polls or uses checkReceive()
    Serial.printf("MCP2515 INT pin %d configured (polling recommended).\n", m_intPin);
  }

  Serial.printf("MCP2515 initialized successfully at %ld bps.\n", baudRate);
  return true;
#elif defined PLATFORM_LINUX
  // Linux implementation using SocketCAN
  if ((m_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
    perror("Socket creation failed");
    return false;
  }
  
  printf("Debug: Socket created successfully\n");
  
  strcpy(m_ifr.ifr_name, canDevice);
  if (ioctl(m_socket, SIOCGIFINDEX, &m_ifr) < 0) {
    perror("ioctl failed");
    close(m_socket);
    return false;
  }
  
  printf("Debug: Interface index obtained for %s: %d\n", 
         canDevice, m_ifr.ifr_ifindex);
  
  m_addr.can_family = AF_CAN;
  m_addr.can_ifindex = m_ifr.ifr_ifindex;
  
  if (bind(m_socket, (struct sockaddr *)&m_addr, sizeof(m_addr)) < 0) {
    perror("Bind failed");
    close(m_socket);
    return false;
  }
  
  printf("Debug: Socket bound to interface %s\n", canDevice);
  
  // Set non-blocking mode for reads
  int flags = fcntl(m_socket, F_GETFL, 0);
  fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);
  
  printf("Debug: Socket set to non-blocking mode\n");
  
  return true;
#endif
}

void CANInterface::end() {
#ifdef PLATFORM_ARDUINO
  CAN.end();
#elif defined PLATFORM_ESP32
  // MCP2515 library doesn't have an explicit end/stop function.
  // Resetting might put it in a safe state if needed.
  // m_mcp2515.reset(); 
  Serial.println("MCP2515 end() - No specific action needed by library.");
#elif defined PLATFORM_LINUX
  if (m_socket >= 0) {
    close(m_socket);
    m_socket = -1;
    printf("Debug: Socket closed\n");
  }
#endif
}

bool CANInterface::sendMessage(const CANMessage& msg) {
#if defined(PLATFORM_ARDUINO) && !defined(PLATFORM_ESP32)
  // Arduino implementation
  if (!CAN.beginPacket(msg.id)) {
    Serial.println("Failed to start CAN packet");
    return false;
  }
  
  CAN.write(msg.data, msg.length);
  Serial.println("CAN packet written");
  try {
    return CAN.endPacket();
  } catch (const std::exception& e) {
    Serial.println("Error ending CAN packet");
    return false;
  }
#elif defined PLATFORM_ESP32
  // ESP32 implementation using MCP2515
  struct can_frame frame; // Use the library's frame structure

  frame.can_id = msg.id;
  // Add flags if needed (e.g., frame.can_id |= CAN_EFF_FLAG for extended ID)
  frame.can_dlc = msg.length;
  memcpy(frame.data, msg.data, msg.length);

  Serial.println("Transmitting CAN message via MCP2515...");

  // Use sendMessage, check against MCP2515::ERROR_OK
  MCP2515::ERROR result = m_mcp2515.sendMessage(&frame);

  if (result == MCP2515::ERROR_OK) {
    Serial.println("MCP2515 message sent successfully.");
    return true;
  } else {
    Serial.printf("Error sending MCP2515 message: %d\n", result);
    return false;
  }
#elif defined PLATFORM_LINUX
  // Linux implementation
  struct can_frame frame;
  memset(&frame, 0, sizeof(frame));
  
  frame.can_id = msg.id;
  frame.can_dlc = msg.length;
  memcpy(frame.data, msg.data, msg.length);
  
  printf("Debug: Sending CAN frame - ID: 0x%X, DLC: %d, Data:", 
         frame.can_id, frame.can_dlc);
  for (int i = 0; i < frame.can_dlc; i++) {
    printf(" %02X", frame.data[i]);
  }
  printf("\n");
  
  // Check socket validity
  if (m_socket < 0) {
    printf("ERROR: Invalid socket descriptor: %d\n", m_socket);
    return false;
  }
  
  // Log socket details
  int sock_type;
  socklen_t sock_type_len = sizeof(sock_type);
  if (getsockopt(m_socket, SOL_SOCKET, SO_TYPE, &sock_type, &sock_type_len) < 0) {
    printf("ERROR: Could not get socket type: %s\n", strerror(errno));
  } else {
    printf("Debug: Socket type: %d (SOCK_RAW=%d)\n", sock_type, SOCK_RAW);
  }
  
  // Log socket error state
  int error_value;
  socklen_t error_len = sizeof(error_value);
  if (getsockopt(m_socket, SOL_SOCKET, SO_ERROR, &error_value, &error_len) < 0) {
    printf("ERROR: Could not get socket error state: %s\n", strerror(errno));
  } else if (error_value != 0) {
    printf("ERROR: Socket has pending error: %s\n", strerror(error_value));
  }
  
  // Get socket flags
  int flags = fcntl(m_socket, F_GETFL, 0);
  if (flags < 0) {
    printf("ERROR: Could not get socket flags: %s\n", strerror(errno));
  } else {
    printf("Debug: Socket flags: 0x%X (O_NONBLOCK=0x%X)\n", flags, O_NONBLOCK);
  }
  
  printf("Debug: About to write %zu bytes to socket %d\n", sizeof(struct can_frame), m_socket);
  int nbytes = write(m_socket, &frame, sizeof(struct can_frame));
  
  if (nbytes < 0) {
    printf("ERROR: CAN write failed: %s (errno=%d)\n", strerror(errno), errno);
    return false;
  } else if (nbytes != sizeof(struct can_frame)) {
    printf("ERROR: Incomplete CAN frame write: %d of %zu bytes\n", nbytes, sizeof(struct can_frame));
    return false;
  }
  
  printf("Debug: Successfully wrote %d bytes to socket\n", nbytes);
  return true;
#endif
  
  return false;
}

bool CANInterface::messageAvailable() {
#ifdef PLATFORM_ARDUINO
  return CAN.parsePacket();
#elif defined PLATFORM_ESP32
  // ESP32 implementation using MCP2515
  // checkReceive() returns ERROR_OK if a message is available
  return (m_mcp2515.checkReceive() == MCP2515::ERROR_OK);
#elif defined PLATFORM_LINUX
  fd_set readSet;
  FD_ZERO(&readSet);
  FD_SET(m_socket, &readSet);
  
  struct timeval timeout = {0, 0}; // Zero timeout for non-blocking
  return (select((m_socket + 1), &readSet, NULL, NULL, &timeout) > 0);
#endif
  
  return false;
}

bool CANInterface::receiveMessage(CANMessage& msg) {
#ifdef PLATFORM_ARDUINO
  // Check if there's a message available
  if (!CAN.parsePacket()) {
    return false;
  }
  
  // Read the packet
  msg.id = CAN.packetId();
  msg.length = 0;
  
  while (CAN.available() && msg.length < 8) {
    msg.data[msg.length++] = CAN.read();
  }
  
  return true;
#elif defined PLATFORM_ESP32
  // ESP32 implementation using MCP2515
  struct can_frame frame; // Library's frame structure

  // Use readMessage, check against MCP2515::ERROR_OK
  if (m_mcp2515.readMessage(&frame) == MCP2515::ERROR_OK) {
    // Map the received frame to the common CANMessage struct
    msg.id = frame.can_id; // Note: This might include flags (like EFF/RTR) set by the library
    // Consider masking msg.id if you only want the raw ID: msg.id = frame.can_id & CAN_SFF_MASK; (or CAN_EFF_MASK)
    msg.length = frame.can_dlc;
    memcpy(msg.data, frame.data, frame.can_dlc);
    return true;
  } else {
    // No message available or read error
    return false;
  }
#elif defined PLATFORM_LINUX
  struct can_frame frame;
  
  int nbytes = read(m_socket, &frame, sizeof(struct can_frame));
  if (nbytes < 0) {
    // No data available or error
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return false; // No data available
    }
    perror("CAN read error");
    return false;
  }
  
  if (nbytes < (int)sizeof(struct can_frame)) {
    // Incomplete frame
    printf("Debug: Incomplete CAN frame received (%d bytes)\n", nbytes);
    return false;
  }

  // Validate frame length before copying
  if (frame.can_dlc > 8) {
    printf("Debug: CAN frame received with invalid DLC (%d bytes) ID: 0x%X\n", frame.can_dlc, frame.can_id);
    return false;
  }
  
  // Copy validated frame data
  msg.id = frame.can_id;
  msg.length = frame.can_dlc;
  memcpy(msg.data, frame.data, frame.can_dlc);
  
  return true;
#endif
  
  return false;
} 