#ifndef MOCK_CAN_INTERFACE_H
#define MOCK_CAN_INTERFACE_H

#include "../CANInterface.h"
#include <vector>
#include <queue>

/**
 * MockCANInterface - A mock implementation for testing
 * This does not inherit from CANInterface to avoid breaking the real implementation
 */
class MockCANInterface {
public:
    MockCANInterface() : m_initialized(false) {}

    // Mock methods matching CANInterface interface
    bool begin(long baudRate = 500000, const char* canDevice = "can0") {
        m_initialized = true;
        return true;
    }

    void end() {
        m_initialized = false;
        clearSentMessages();
        clearPendingReceiveMessages();
    }

    bool sendMessage(const CANMessage& msg) {
        if (!m_initialized) return false;
        m_sentMessages.push_back(msg);
        return true;
    }

    bool receiveMessage(CANMessage& msg) {
        if (!m_initialized || m_pendingReceiveMessages.empty()) return false;
        
        msg = m_pendingReceiveMessages.front();
        m_pendingReceiveMessages.pop();
        return true;
    }

    bool messageAvailable() {
        return m_initialized && !m_pendingReceiveMessages.empty();
    }

    // Test helper methods
    void queueReceiveMessage(const CANMessage& msg) {
        m_pendingReceiveMessages.push(msg);
    }

    const std::vector<CANMessage>& getSentMessages() const {
        return m_sentMessages;
    }

    void clearSentMessages() {
        m_sentMessages.clear();
    }

    void clearPendingReceiveMessages() {
        // Clear the queue by swapping with an empty queue
        std::queue<CANMessage> empty;
        std::swap(m_pendingReceiveMessages, empty);
    }

    bool isInitialized() const {
        return m_initialized;
    }

private:
    bool m_initialized;
    std::vector<CANMessage> m_sentMessages;
    std::queue<CANMessage> m_pendingReceiveMessages;
};

#endif // MOCK_CAN_INTERFACE_H 