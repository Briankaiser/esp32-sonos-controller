# Testing Framework Specification

## Purpose

Establishes the project's ability to verify firmware behaviour in isolation on the host machine and end-to-end on the physical device, including a mock Sonos server that allows protocol logic to be tested without real speakers.

## Requirements

### Requirement: Host Test Suite Runs Without Any Connected Hardware

The project SHALL include a test environment that compiles selected firmware modules for the host machine and runs their tests without a connected microcontroller, display, or network device. Running the host test command SHALL produce pass/fail output for every host-side test case.

#### Scenario: Host tests pass on a clean checkout

- **WHEN** a developer runs `pio test -e test_native` on a machine with no ESP32 connected
- **THEN** the command exits 0
- **AND** pass/fail counts are reported for every test case
- **AND** no serial port, display, or PSRAM is accessed

#### Scenario: Host test detects a logic regression

- **WHEN** a change introduces an incorrect value in the layout slot computation
- **THEN** the host test for that computation fails
- **AND** the failed assertion identifies the slot and the expected versus actual coordinates

#### Scenario: Host tests run in CI without hardware

- **WHEN** the CI pipeline runs
- **THEN** the host test job completes without any connected device
- **AND** a failing test causes the CI job to fail

### Requirement: Device Tests Validate Key On-Hardware Behaviours

The project SHALL include a test environment that flashes a test image to the primary board and collects pass/fail results over the serial port. Device tests SHALL cover at minimum: display bring-up, touch registration, and the sustained frame-rate budget.

#### Scenario: Display bring-up passes

- **WHEN** `pio test -e test_device` is run with a CrowPanel connected
- **THEN** the device test for display bring-up passes
- **AND** the result is reported over serial

#### Scenario: Frame-rate budget is confirmed

- **WHEN** the device frame-rate test runs
- **THEN** the measured sustained frame rate is reported over serial
- **AND** the test passes only if the sustained rate meets or exceeds 55 frames per second

#### Scenario: Touch registers a point

- **WHEN** the touch registration test runs
- **THEN** at least one touch point is read from the CST816 digitizer within the test timeout
- **AND** the reported coordinates fall within the display bounds

### Requirement: Mock Sonos Server Decouples Protocol Tests From Real Speakers

The project SHALL include a mock Sonos server script that responds to SSDP discovery requests and SOAP command requests with configurable canned responses. The mock server SHALL record which commands it received so tests can assert on interaction behaviour.

#### Scenario: SSDP discovery returns a mock speaker

- **WHEN** the mock server is running and a discovery request is sent to 239.255.255.250:1900
- **THEN** the mock server replies with a valid UPnP device description for a zone player
- **AND** the description includes the correct service control URLs

#### Scenario: SOAP play command is acknowledged

- **WHEN** a SOAP `Play` action is sent to the mock server's AVTransport control URL
- **THEN** the mock server responds with a valid SOAP success envelope
- **AND** the mock server records the received command so the test can assert it was sent

#### Scenario: Mock server records and reports received commands

- **WHEN** a test sends multiple SOAP commands in sequence
- **THEN** the mock server exposes the ordered list of received commands
- **AND** a test can assert on the command sequence without touching a real speaker
