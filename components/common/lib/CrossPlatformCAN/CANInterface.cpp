/**
 * CANInterface.cpp - Implementation of cross-platform CAN interface
 */

#include "CANInterface.h"

#ifdef PLATFORM_ESP32
CAN_device_t CAN_cfg;
#endif

// Platform-specific implementations
bool CANInterface::begin(long baudRate, const char* canDevice, int csPin, int intPin) {
#ifdef PLATFORM_ARDUINO && !defined PLATFORM_ESP32
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
  // Configure CAN parameters *before* calling CANInit
  CAN_cfg.speed = CAN_SPEED_1000KBPS; 
  CAN_cfg.tx_pin_id = (gpio_num_t)csPin; 
  CAN_cfg.rx_pin_id = (gpio_num_t)intPin;
  CAN_cfg.rx_queue = xQueueCreate(10, sizeof(CAN_frame_t));

  // Assume ESP32Can uses the globally accessible CAN_cfg struct
  // Call CANInit with zero arguments as indicated by the error message
  int initResult = ESP32Can.CANInit(); 
  
  // Check the return value (assuming 0 for success, similar to Arduino CAN)
  if (initResult != 0) { 
    Serial.printf("Error initializing ESP32CAN, result code: %d\n", initResult);
    // Clean up queue if init failed
    if (CAN_cfg.rx_queue != NULL) { vQueueDelete(CAN_cfg.rx_queue); CAN_cfg.rx_queue = NULL; }
    return false;
  }

  Serial.printf("ESP32CAN initialized successfully (TX: %d, RX: %d)\n", csPin, intPin);

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
  // Implement ESP32 CAN stop
  esp_err_t err = ESP32Can.CANStop();
  if (err != ESP_OK) {
    Serial.printf("Error stopping ESP32CAN: %s\n", esp_err_to_name(err));
  } else {
    Serial.println("ESP32CAN stopped.");
  }
  // Free the queue
  if (CAN_cfg.rx_queue != NULL) {
    vQueueDelete(CAN_cfg.rx_queue);
    CAN_cfg.rx_queue = NULL; 
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
#ifdef PLATFORM_ARDUINO && !defined PLATFORM_ESP32
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
  // ESP32 implementation

  CAN_frame_t tx_frame;
  tx_frame.FIR.B.FF = CAN_frame_std;
  tx_frame.MsgID = msg.id;
  tx_frame.FIR.B.DLC = msg.length;
  memcpy(tx_frame.data.u8, msg.data, msg.length);

  Serial.println("Transmitting CAN message");
  
  try {
    return ESP32Can.CANWriteFrame(&tx_frame);
  } catch (const std::exception& e) {
    Serial.println("Error transmitting CAN message");
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
  // Check if messages are waiting in the ESP32 queue
  if (CAN_cfg.rx_queue == NULL) return false;
  return (uxQueueMessagesWaiting(CAN_cfg.rx_queue) > 0);
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
  // Implement ESP32 receive message
  if (CAN_cfg.rx_queue == NULL) return false;

  CAN_frame_t rx_frame;
  // Read frame from queue (non-blocking)
  if (xQueueReceive(CAN_cfg.rx_queue, &rx_frame, 0) == pdPASS) {
    msg.id = rx_frame.MsgID;
    // Ensure DLC is within valid range before copying
    if (rx_frame.FIR.B.DLC > 8) {
       Serial.printf("Error: Received invalid DLC (%d) for ID 0x%X\n", rx_frame.FIR.B.DLC, rx_frame.MsgID);
       return false; // Invalid DLC
    }
    msg.length = rx_frame.FIR.B.DLC;
    memcpy(msg.data, rx_frame.data.u8, msg.length);
    return true;
  }
  return false; // No message received
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