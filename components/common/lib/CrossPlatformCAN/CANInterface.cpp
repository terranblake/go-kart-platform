/**
 * CANInterface.cpp - Implementation of cross-platform CAN interface
 */

#include "CANInterface.h"

// --- Define Multicast Constants (for Darwin) ---
#ifdef PLATFORM_DARWIN
#define MULTICAST_GROUP "239.0.0.1"
#define MULTICAST_PORT 5555
#define CAN_MESSAGE_BUFFER_SIZE 13 // 4 (id) + 1 (len) + 8 (data)
#endif
// --------------------------------------------

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
#elif defined(PLATFORM_DARWIN)
  // macOS/Darwin implementation using UDP Multicast
  printf("macOS CAN Interface: Initializing UDP Multicast on %s:%d\n", MULTICAST_GROUP, MULTICAST_PORT);

  // 1. Create UDP socket
  m_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (m_socket < 0) {
    perror("Multicast socket creation failed");
    return false;
  }
  printf("Debug: Multicast socket created successfully (fd=%d)\n", m_socket);

  // 2. Allow multiple sockets to use the same PORT number
  int reuse = 1;
  if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char *)&reuse, sizeof(reuse)) < 0) {
    perror("Setting SO_REUSEADDR error");
    close(m_socket); m_socket = -1;
    return false;
  }
  #ifdef SO_REUSEPORT // Needed on macOS more often than Linux
  if (setsockopt(m_socket, SOL_SOCKET, SO_REUSEPORT, (char *)&reuse, sizeof(reuse)) < 0) {
     perror("Setting SO_REUSEPORT error");
     close(m_socket); m_socket = -1;
     return false;
  }
  #endif
  printf("Debug: SO_REUSEADDR/SO_REUSEPORT set\n");

  // 3. Set up local address structure
  memset((char *) &m_local_addr, 0, sizeof(m_local_addr));
  m_local_addr.sin_family = AF_INET;
  m_local_addr.sin_port = htons(MULTICAST_PORT);
  m_local_addr.sin_addr.s_addr = htonl(INADDR_ANY); // Bind to any interface

  // 4. Bind to the local address
  if (bind(m_socket, (struct sockaddr*) &m_local_addr, sizeof(m_local_addr)) < 0) {
    perror("Multicast bind failed");
    close(m_socket); m_socket = -1;
    return false;
  }
   printf("Debug: Multicast socket bound to port %d\n", MULTICAST_PORT);

  // 5. Set up multicast group address structure
  memset((char *) &m_multicast_addr, 0, sizeof(m_multicast_addr));
  m_multicast_addr.sin_family = AF_INET;
  m_multicast_addr.sin_port = htons(MULTICAST_PORT);
  m_multicast_addr.sin_addr.s_addr = inet_addr(MULTICAST_GROUP);

  // 6. Join the multicast group
  struct ip_mreq mreq;
  mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP);
  mreq.imr_interface.s_addr = htonl(INADDR_ANY); // Use default interface
  if (setsockopt(m_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*) &mreq, sizeof(mreq)) < 0) {
    perror("Adding multicast group error");
    close(m_socket); m_socket = -1;
    return false;
  }
  printf("Debug: Joined multicast group %s\n", MULTICAST_GROUP);

  // 7. Enable loopback (receive own messages)
  char loopch = 1;
  if(setsockopt(m_socket, IPPROTO_IP, IP_MULTICAST_LOOP, (char *)&loopch, sizeof(loopch)) < 0) {
      perror("Setting IP_MULTICAST_LOOP error");
      // Non-fatal, continue anyway
  } else {
      printf("Debug: Multicast loopback enabled.\n");
  }

  // 8. Set multicast TTL (optional, default is 1 for local network)
  // char ttl = 1; // Example: TTL of 1
  // setsockopt(m_socket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl));

  // 9. Set non-blocking mode for reads
  int flags = fcntl(m_socket, F_GETFL, 0);
  if (flags < 0 || fcntl(m_socket, F_SETFL, flags | O_NONBLOCK) < 0) {
      perror("Setting non-blocking mode failed");
      close(m_socket); m_socket = -1;
      return false;
  }
  printf("Debug: Multicast socket set to non-blocking mode\n");

  printf("macOS CAN Interface: UDP Multicast Initialized Successfully.\n");
  return true;
#elif defined(PLATFORM_LINUX)
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
#elif defined(PLATFORM_DARWIN)
  printf("macOS CAN Interface: Shutting down UDP Multicast.\n");
  if (m_socket >= 0) {
    // Leave the multicast group
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(MULTICAST_GROUP);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(m_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*) &mreq, sizeof(mreq)) < 0) {
      perror("Dropping multicast group error");
    } else {
        printf("Debug: Left multicast group %s\n", MULTICAST_GROUP);
    }
    // Close the socket
    close(m_socket);
    m_socket = -1;
    printf("Debug: Multicast socket closed\n");
  }
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
  return CAN.endPacket();
#elif defined PLATFORM_ESP32
  // ESP32 implementation using MCP2515
  struct can_frame frame; // Use the library's frame structure

  frame.can_id = msg.id;
  // Add flags if needed (e.g., frame.can_id |= CAN_EFF_FLAG for extended ID)
  frame.can_dlc = msg.length;
  memcpy(frame.data, msg.data, msg.length);

#if CAN_LOGGING_ENABLED // Added conditional compilation
  Serial.println("Transmitting CAN message via MCP2515...");
#endif

  // Use sendMessage, check against MCP2515::ERROR_OK
  MCP2515::ERROR result = m_mcp2515.sendMessage(&frame);

#if CAN_LOGGING_ENABLED // Added conditional compilation
  if (result == MCP2515::ERROR_OK) {
    Serial.println("MCP2515 message sent successfully.");
  } else {
    Serial.printf("MCP2515 send failed with error: %d\n", result);
  }
#endif

  return (result == MCP2515::ERROR_OK);
#elif defined(PLATFORM_DARWIN)
  if (m_socket < 0) {
    printf("ERROR: sendMessage called on invalid socket\n");
    return false;
  }

  // Serialize CANMessage to buffer
  unsigned char buffer[CAN_MESSAGE_BUFFER_SIZE];
  uint32_t net_id = htonl(msg.id); // Use network byte order for ID
  memcpy(buffer, &net_id, 4);
  buffer[4] = msg.length;
  memcpy(buffer + 5, msg.data, 8); // Copy up to 8 data bytes

  // Send via UDP multicast
  ssize_t bytes_sent = sendto(m_socket, buffer, CAN_MESSAGE_BUFFER_SIZE, 0,
                              (struct sockaddr*) &m_multicast_addr, sizeof(m_multicast_addr));

  if (bytes_sent < 0) {
    perror("Multicast sendto failed");
    return false;
  } else if (bytes_sent != CAN_MESSAGE_BUFFER_SIZE) {
    printf("ERROR: Incomplete multicast send (%zd of %d bytes)\n", bytes_sent, CAN_MESSAGE_BUFFER_SIZE);
    return false;
  }

  #if DEBUG_MODE // Optional debug print
  printf("Debug: Multicast sent %zd bytes - ID: 0x%X, DLC: %d\n", bytes_sent, msg.id, msg.length);
  #endif

  return true;
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
#elif defined(PLATFORM_DARWIN)
  if (m_socket < 0) return false;

  fd_set readSet;
  FD_ZERO(&readSet);
  FD_SET(m_socket, &readSet);

  struct timeval timeout = {0, 0}; // Zero timeout for non-blocking check
  int result = select(m_socket + 1, &readSet, NULL, NULL, &timeout);

  if (result < 0) {
    perror("Multicast select error");
    return false;
  }
  return (result > 0) && FD_ISSET(m_socket, &readSet);
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
#elif defined(PLATFORM_DARWIN)
  if (m_socket < 0) return false;

  unsigned char buffer[CAN_MESSAGE_BUFFER_SIZE];
  struct sockaddr_in sender_addr;
  socklen_t addr_len = sizeof(sender_addr);

  ssize_t bytes_received = recvfrom(m_socket, buffer, CAN_MESSAGE_BUFFER_SIZE, 0,
                                    (struct sockaddr*) &sender_addr, &addr_len);

  if (bytes_received < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return false; // No data available (non-blocking)
    }
    perror("Multicast recvfrom error");
    return false;
  }

  if (bytes_received != CAN_MESSAGE_BUFFER_SIZE) {
    printf("Debug: Incomplete multicast message received (%zd bytes)\n", bytes_received);
    return false;
  }

  // Deserialize buffer to CANMessage
  uint32_t net_id;
  memcpy(&net_id, buffer, 4);
  msg.id = ntohl(net_id); // Convert ID back to host byte order
  msg.length = buffer[4];
  if (msg.length > 8) msg.length = 8; // Sanity check DLC
  memcpy(msg.data, buffer + 5, msg.length);

  #if DEBUG_MODE // Optional debug print
  printf("Debug: Multicast received %zd bytes - ID: 0x%X, DLC: %d\n", bytes_received, msg.id, msg.length);
  #endif

  return true;
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