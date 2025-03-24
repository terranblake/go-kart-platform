# distutils: language = c++
# cython: language_level=3

"""
Cython wrapper for the C++ ProtobufCANInterface
"""

from libcpp.string cimport string
from libcpp cimport bool
from libcpp.vector cimport vector
from libc.stdint cimport uint8_t, uint32_t, int32_t

# Declare the C++ class with its methods for Cython
cdef extern from "ProtobufCANInterface.h":
    # Enums from common.pb.h
    cdef enum kart_common_MessageType:
        kart_common_MessageType_COMMAND = 0
        kart_common_MessageType_STATUS = 1
        kart_common_MessageType_ACK = 2
        kart_common_MessageType_ERROR = 3
    
    cdef enum kart_common_ComponentType:
        kart_common_ComponentType_LIGHTS = 0
        kart_common_ComponentType_MOTORS = 1
        kart_common_ComponentType_SENSORS = 2
        kart_common_ComponentType_BATTERY = 3
        kart_common_ComponentType_CONTROLS = 4
    
    cdef enum kart_common_ValueType:
        kart_common_ValueType_BOOLEAN = 0
        kart_common_ValueType_INT8 = 1
        kart_common_ValueType_UINT8 = 2
        kart_common_ValueType_INT16 = 3
        kart_common_ValueType_UINT16 = 4
        kart_common_ValueType_INT24 = 5
        kart_common_ValueType_UINT24 = 6
        kart_common_ValueType_FLOAT16 = 7
    
    # Declare the ProtobufCANInterface class
    cdef cppclass ProtobufCANInterface:
        ProtobufCANInterface(uint32_t nodeId) nogil except +
        bool begin(long baudrate) nogil except +
        bool sendMessage(kart_common_MessageType message_type, kart_common_ComponentType component_type,
                         uint8_t component_id, uint8_t command_id, kart_common_ValueType value_type, int32_t value) nogil except +
        void process() nogil except +
        
        # Utility methods
        @staticmethod
        uint8_t packHeader(kart_common_MessageType type, kart_common_ComponentType component) nogil except +
        @staticmethod
        void unpackHeader(uint8_t header, kart_common_MessageType &type, kart_common_ComponentType &component) nogil except +
        @staticmethod
        uint32_t packValue(kart_common_ValueType type, int32_t value) nogil except +
        @staticmethod
        int32_t unpackValue(kart_common_ValueType type, uint32_t packed_value) nogil except +

# Define Python-accessible enums
class MessageType:
    COMMAND = kart_common_MessageType_COMMAND
    STATUS = kart_common_MessageType_STATUS
    ACK = kart_common_MessageType_ACK
    ERROR = kart_common_MessageType_ERROR

class ComponentType:
    LIGHTS = kart_common_ComponentType_LIGHTS
    MOTORS = kart_common_ComponentType_MOTORS
    SENSORS = kart_common_ComponentType_SENSORS
    BATTERY = kart_common_ComponentType_BATTERY
    CONTROLS = kart_common_ComponentType_CONTROLS

class ValueType:
    BOOLEAN = kart_common_ValueType_BOOLEAN
    INT8 = kart_common_ValueType_INT8
    UINT8 = kart_common_ValueType_UINT8
    INT16 = kart_common_ValueType_INT16
    UINT16 = kart_common_ValueType_UINT16
    INT24 = kart_common_ValueType_INT24
    UINT24 = kart_common_ValueType_UINT24
    FLOAT16 = kart_common_ValueType_FLOAT16

# Python wrapper class
cdef class PyCANInterface:
    cdef ProtobufCANInterface* _can_interface
    
    def __cinit__(self, uint32_t node_id):
        self._can_interface = new ProtobufCANInterface(node_id)
    
    def __dealloc__(self):
        if self._can_interface != NULL:
            del self._can_interface
    
    def begin(self, long baudrate=500000):
        """Initialize the CAN interface with the given baudrate"""
        return self._can_interface.begin(baudrate)
    
    def send_message(self, message_type, component_type, component_id, 
                    command_id, value_type, value):
        """Send a CAN message with the given parameters"""
        return self._can_interface.sendMessage(
            <kart_common_MessageType>message_type,
            <kart_common_ComponentType>component_type,
            <uint8_t>component_id,
            <uint8_t>command_id,
            <kart_common_ValueType>value_type,
            <int32_t>value
        )
    
    def process(self):
        """Process incoming CAN messages"""
        self._can_interface.process()
    
    @staticmethod
    def pack_header(message_type, component_type):
        """Pack message type and component type into a header byte"""
        return ProtobufCANInterface.packHeader(
            <kart_common_MessageType>message_type,
            <kart_common_ComponentType>component_type
        )
    
    @staticmethod
    def unpack_header(header):
        """Unpack a header byte into message type and component type"""
        cdef kart_common_MessageType message_type
        cdef kart_common_ComponentType component_type
        ProtobufCANInterface.unpackHeader(<uint8_t>header, message_type, component_type)
        return message_type, component_type
    
    @staticmethod
    def pack_value(value_type, value):
        """Pack a value according to its type"""
        return ProtobufCANInterface.packValue(
            <kart_common_ValueType>value_type,
            <int32_t>value
        )
    
    @staticmethod
    def unpack_value(value_type, packed_value):
        """Unpack a value according to its type"""
        return ProtobufCANInterface.unpackValue(
            <kart_common_ValueType>value_type,
            <uint32_t>packed_value
        ) 