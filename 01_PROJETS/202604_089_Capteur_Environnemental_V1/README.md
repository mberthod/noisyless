# Environmental Sensor Firmware Documentation

## Overview

This documentation describes the firmware for an ESP32-S3 based environmental sensor system. The system is designed to monitor various environmental parameters including air quality, light levels, sound levels, and presence detection. The firmware is built using the Arduino framework and FreeRTOS for task management.

## System Architecture

The firmware is organized into several key components:

1. **Sensor Management**: Handles reading data from various environmental sensors
2. **Network Communication**: Manages WiFi connectivity and data transmission to a server
3. **Memory Management**: Monitors and optimizes memory usage
4. **Alert System**: Detects and reports significant environmental changes
5. **OTA Updates**: Supports over-the-air firmware updates

## FreeRTOS Task Structure

The firmware uses FreeRTOS to manage concurrent operations through the following tasks:

- **Memory Task**: Monitors memory usage and performs periodic defragmentation
- **Sensor Task**: Reads data from sensors at regular intervals
- **Network Task**: Handles all network communications
- **Button Task**: Monitors the physical button for user interactions

## Key Features

- Real-time environmental monitoring
- Automatic data transmission to a central server
- Offline data storage with automatic transmission when connectivity is restored
- Memory leak detection and prevention
- Automatic firmware updates
- Alert system for significant environmental changes

## Getting Started

To use this firmware:

1. Configure your credentials in `credentials.h`
2. Upload the firmware to your ESP32-S3 device
3. The device will automatically connect to WiFi and begin monitoring

## Memory Management

The firmware includes sophisticated memory management features:

- Periodic memory status logging
- Memory leak detection
- Automatic defragmentation
- NVS (Non-Volatile Storage) cleaning

## Network Communication

The firmware supports:

- Automatic WiFi connection and reconnection
- HTTP communication with a central server
- Retry mechanisms for failed transmissions
- Offline data storage

## Alert System

The alert system can detect:

- Device removal from its mounting
- Significant environmental changes
- System errors and warnings

## OTA Updates

The firmware supports over-the-air updates:

- Automatic update checking
- Secure update process
- Version management

## Dependencies

- Arduino Framework
- FreeRTOS
- WiFi Library
- HTTPClient Library
- ArduinoJson Library
- Preferences Library 
