/**
 * CANInterface.cpp - Implementation of cross-platform CAN interface
 */

#include "CANInterface.h"

// Platform-specific implementations
bool CANInterface::begin(long baudRate, const char* canDevice) {
#ifdef PLATFORM_ARDUINO
  // Arduino implementation using Arduino-CAN library
  if (!CAN.begin(baudRate)) {
    return false;
  }
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
#elif defined PLATFORM_LINUX
  if (m_socket >= 0) {
    close(m_socket);
    m_socket = -1;
    printf("Debug: Socket closed\n");
  }
#endif
}

bool CANInterface::sendMessage(const CANMessage& msg) {
#ifdef PLATFORM_ARDUINO
  // Arduino implementation
  if (!CAN.beginPacket(msg.id)) {
    return false;
  }
  
  CAN.write(msg.data, msg.length);
  return CAN.endPacket();
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
  
  msg.id = frame.can_id;
  msg.length = frame.can_dlc;
  memcpy(msg.data, frame.data, frame.can_dlc);
  
  printf("Debug: Received CAN frame - ID: 0x%X, DLC: %d, Data:", 
         msg.id, msg.length);
  for (int i = 0; i < msg.length; i++) {
    printf(" %02X", msg.data[i]);
  }
  printf("\n");
  
  // Extract message details for more helpful debugging
  if (msg.length >= 8) {
    // First byte: bits 0-6 = component type, bit 7 = message type
    int msgType = (msg.data[0] >> 7) & 0x01;
    int compType = msg.data[0] & 0x7F;
    
    // Extract component ID and command ID from bytes 2 and 3
    int compId = msg.data[2];
    int cmdId = msg.data[3];
    
    // Extract value type from byte 4 (high nibble)
    int valType = (msg.data[4] >> 4) & 0x0F;
    
    // Extract value (last 3 bytes)
    int value = (msg.data[5] << 16) | (msg.data[6] << 8) | msg.data[7];
    
    printf("Debug: Parsed message - Type: %s, Component: %d, ID: %d, CMD: %d, Value Type: %d, Value: %d\n",
           msgType == 0 ? "COMMAND" : "STATUS", compType, compId, cmdId, valType, value);
  }
  
  return true;
#endif
  
  return false;
} 