# Go-Kart Platform TODO List

This document tracks ongoing tasks and future improvements for the Go-Kart Platform project.

## High Priority

- [x] Fix binary data handling in CrossPlatformCAN implementation
  - [x] Update HandlerEntry struct to use a union for handlers
  - [x] Fix data layout inconsistency between send and receive in binary frames
- [ ] Implement automated tests for binary data transmission
- [ ] Deploy changes to Raspberry Pi for production testing

## Medium Priority

- [ ] Improve error handling and recovery for binary data transmission failures
- [ ] Add timeout mechanism for incomplete binary data frames
- [ ] Implement data validation in binary frames (CRC or similar)
- [ ] Create visualization tools for CAN bus traffic analysis

## Low Priority

- [ ] Optimize binary data format for more efficient transmission
- [ ] Add compression support for large binary payloads
- [ ] Create documentation with examples for all binary data use cases
- [ ] Implement message prioritization for critical binary data 