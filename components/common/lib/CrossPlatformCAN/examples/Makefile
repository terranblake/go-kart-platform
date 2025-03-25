# Makefile for CrossPlatformCAN tests

CXX = g++
CXXFLAGS = -std=c++11 -Wall -I.. -I../include -DPLATFORM_LINUX

# Target for header test
header_test: header_test.cpp ../ProtobufCANInterface.cpp ../CANInterface.cpp
	$(CXX) $(CXXFLAGS) -o header_test header_test.cpp ../ProtobufCANInterface.cpp ../CANInterface.cpp
	@echo "Running header test..."
	@./header_test

# Clean build artifacts
clean:
	rm -f header_test

.PHONY: clean 