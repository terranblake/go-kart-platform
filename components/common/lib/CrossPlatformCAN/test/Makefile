CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -I../../../.. -I../include -I../include/nanopb

# Define the target
TARGET = test_protobuf_can

# Object files
OBJS = test_protobuf_can.o \
       ../ProtobufCANInterface.o \
       ../CANInterface.o

# Rule to build the target
all: $(TARGET)

# Link the object files together
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Define platform for test
test_protobuf_can.o: test_protobuf_can.cpp
	$(CXX) $(CXXFLAGS) -D PLATFORM_WINDOWS -c -o $@ $<

# Compile source files to object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Clean up
clean:
	rm -f $(TARGET) $(OBJS)

# Run the tests
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run 