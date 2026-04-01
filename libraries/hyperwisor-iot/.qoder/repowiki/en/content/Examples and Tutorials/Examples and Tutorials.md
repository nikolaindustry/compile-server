# Examples and Tutorials

<cite>
**Referenced Files in This Document**
- [README.md](file://README.md)
- [library.properties](file://library.properties)
- [hyperwisor-iot.h](file://src/hyperwisor-iot.h)
- [hyperwisor-iot.cpp](file://src/hyperwisor-iot.cpp)
- [BasicSetup.ino](file://examples/BasicSetup/BasicSetup.ino)
- [WiFiProvisioning.ino](file://examples/WiFiProvisioning/WiFiProvisioning.ino)
- [Manual_Provisioning_Example.ino](file://examples/Manual_Provisioning_Example/Manual_Provisioning_Example.ino)
- [Conditional_Provisioning_Example.ino](file://examples/Conditional_Provisioning_Example/Conditional_Provisioning_Example.ino)
- [CommandHandler.ino](file://examples/CommandHandler/CommandHandler.ino)
- [GPIOControl.ino](file://examples/GPIOControl/GPIOControl.ino)
- [WidgetUpdate.ino](file://examples/WidgetUpdate/WidgetUpdate.ino)
- [SensorDataLogger.ino](file://examples/SensorDataLogger/SensorDataLogger.ino)
- [SmartHomeSwitch.ino](file://examples/SmartHomeSwitch/SmartHomeSwitch.ino)
- [ThreeDWidgetControl.ino](file://examples/ThreeDWidgetControl/ThreeDWidgetControl.ino)
- [AttitudeWidget_MPU6050.ino](file://examples/AttitudeWidget_MPU6050/AttitudeWidget_MPU6050.ino)
- [i2c_PCF8574_relay.ino](file://examples/i2c_PCF8574_relay/i2c_PCF8574_relay.ino)
- [hydrophonics_controller.ino](file://examples/hydrophonics/hydrophonics_controller.ino)
- [README.md](file://examples/hydrophonics/README.md)
</cite>

## Table of Contents
1. [Introduction](#introduction)
2. [Project Structure](#project-structure)
3. [Core Components](#core-components)
4. [Architecture Overview](#architecture-overview)
5. [Detailed Component Analysis](#detailed-component-analysis)
6. [Dependency Analysis](#dependency-analysis)
7. [Performance Considerations](#performance-considerations)
8. [Troubleshooting Guide](#troubleshooting-guide)
9. [Conclusion](#conclusion)
10. [Appendices](#appendices)

## Introduction
This document provides a comprehensive, hands-on guide to implementing practical examples with the Hyperwisor-IOT Arduino library for ESP32. It covers foundational setups, command handling, widget updates, and advanced use cases such as smart home switching, sensor logging, 3D visualization, I2C relay control, and comprehensive hydroponic/aquaponic automation. Each example includes step-by-step implementation walkthroughs, expected behaviors, integration guidance with sensors/actuators/cloud services, troubleshooting tips, and performance optimization advice.

## Project Structure
The repository organizes examples by feature area and the core library header/source files. The examples demonstrate:
- Basic connectivity and provisioning
- Command parsing and custom handlers
- Widget updates for dashboards
- Sensor logging and 3D model control
- Smart home switching and I2C relay control
- Attitude visualization using IMU sensors
- **New**: Comprehensive hydroponic/aquaponic controller with offline automation, NTP-based scheduling, and thermostat control

```mermaid
graph TB
subgraph "Library"
H["hyperwisor-iot.h"]
C["hyperwisor-iot.cpp"]
end
subgraph "Examples"
E1["BasicSetup.ino"]
E2["WiFiProvisioning.ino"]
E3["Manual_Provisioning_Example.ino"]
E4["Conditional_Provisioning_Example.ino"]
E5["CommandHandler.ino"]
E6["GPIOControl.ino"]
E7["WidgetUpdate.ino"]
E8["SensorDataLogger.ino"]
E9["SmartHomeSwitch.ino"]
E10["ThreeDWidgetControl.ino"]
E11["AttitudeWidget_MPU6050.ino"]
E12["i2c_PCF8574_relay.ino"]
E13["hydrophonics_controller.ino"]
end
H --> C
E1 --> H
E2 --> H
E3 --> H
E4 --> H
E5 --> H
E6 --> H
E7 --> H
E8 --> H
E9 --> H
E10 --> H
E11 --> H
E12 --> H
E13 --> H
```

**Diagram sources**
- [hyperwisor-iot.h:1-190](file://src/hyperwisor-iot.h#L1-L190)
- [hyperwisor-iot.cpp:1-800](file://src/hyperwisor-iot.cpp#L1-L800)
- [BasicSetup.ino:1-39](file://examples/BasicSetup/BasicSetup.ino#L1-L39)
- [WiFiProvisioning.ino:1-58](file://examples/WiFiProvisioning/WiFiProvisioning.ino#L1-L58)
- [Manual_Provisioning_Example.ino:1-65](file://examples/Manual_Provisioning_Example/Manual_Provisioning_Example.ino#L1-L65)
- [Conditional_Provisioning_Example.ino:1-69](file://examples/Conditional_Provisioning_Example/Conditional_Provisioning_Example.ino#L1-L69)
- [CommandHandler.ino:1-96](file://examples/CommandHandler/CommandHandler.ino#L1-L96)
- [GPIOControl.ino:1-105](file://examples/GPIOControl/GPIOControl.ino#L1-L105)
- [WidgetUpdate.ino:1-68](file://examples/WidgetUpdate/WidgetUpdate.ino#L1-L68)
- [SensorDataLogger.ino:1-77](file://examples/SensorDataLogger/SensorDataLogger.ino#L1-L77)
- [SmartHomeSwitch.ino:1-328](file://examples/SmartHomeSwitch/SmartHomeSwitch.ino#L1-L328)
- [ThreeDWidgetControl.ino:1-85](file://examples/ThreeDWidgetControl/ThreeDWidgetControl.ino#L1-L85)
- [AttitudeWidget_MPU6050.ino:1-95](file://examples/AttitudeWidget_MPU6050/AttitudeWidget_MPU6050.ino#L1-L95)
- [i2c_PCF8574_relay.ino:1-116](file://examples/i2c_PCF8574_relay/i2c_PCF8574_relay.ino#L1-L116)
- [hydrophonics_controller.ino:1-665](file://examples/hydrophonics/hydrophonics_controller.ino#L1-L665)

**Section sources**
- [README.md:1-173](file://README.md#L1-L173)
- [library.properties:1-11](file://library.properties#L1-L11)

## Core Components
This section highlights the core capabilities exposed by the library and how the examples leverage them.

- Initialization and provisioning
  - begin(): Initializes WiFi and real-time connection; falls back to AP mode if no credentials are found.
  - AP mode provisioning via embedded HTTP server and DNS redirection.
  - Manual provisioning helpers: setCredentials(), setWiFiCredentials(), setDeviceId(), setUserId(), clearCredentials(), hasCredentials().
- Real-time messaging
  - setUserCommandHandler(): registers a user-defined callback to process custom commands.
  - sendTo(): sends structured JSON payloads to a target.
  - setupMessageHandler(): processes built-in commands (e.g., GPIO_MANAGEMENT, OTA, DEVICE_STATUS).
- Widget APIs
  - updateWidget(): updates dashboard widgets with string, numeric, or array values.
  - updateFlightAttitude(): updates a flight attitude meter.
  - updateWidgetPosition(): adjusts widget layout.
  - updateCountdown(): updates countdown timers.
  - updateHeatMap(): updates heat map data.
  - update3DModel()/update3DWidget(): controls 3D models and materials.
- Sensor and data logging
  - send_Sensor_Data_logger(): logs sensor data with a configId and structured values.
- GPIO state management
  - saveGPIOState(), loadGPIOState(), restoreAllGPIOStates(): persist GPIO states across reboots.
- Time and NTP
  - initNTP(), setTimezone(), getNetworkTime()/getNetworkDate()/getNetworkDateTime(): manage time.
- Database and cloud operations
  - setApiKeys(), insertDatainDatabase()/insertDatainDatabaseWithResponse(), onboardDevice()/onboardDeviceWithResponse(), authenticateUser()/authenticateUserWithResponse(), sendSMS()/sendSMSWithResponse(): integrate with backend services.

**Section sources**
- [hyperwisor-iot.h:39-187](file://src/hyperwisor-iot.h#L39-L187)
- [hyperwisor-iot.cpp:13-137](file://src/hyperwisor-iot.cpp#L13-L137)
- [hyperwisor-iot.cpp:313-405](file://src/hyperwisor-iot.cpp#L313-L405)
- [hyperwisor-iot.cpp:521-714](file://src/hyperwisor-iot.cpp#L521-L714)
- [hyperwisor-iot.cpp:730-800](file://src/hyperwisor-iot.cpp#L730-L800)

## Architecture Overview
The examples follow a consistent pattern:
- Device initialization with begin() and continuous loop() maintenance.
- Optional AP provisioning for first-time setup.
- Real-time message handling via WebSocket; built-in and user-defined commands.
- Dashboard widget updates and sensor data logging.
- Advanced integrations with sensors, actuators, and cloud services.

```mermaid
sequenceDiagram
participant Dev as "Device Sketch"
participant Lib as "HyperwisorIOT"
participant RT as "Realtime Server"
participant Dash as "Dashboard/App"
Dev->>Lib : begin()
Lib->>Lib : connectToWiFi() or startAPMode()
alt Connected
Lib->>RT : realtime.begin(deviceId)
Lib->>Lib : setupMessageHandler()
else AP Mode
Lib->>Dash : Provisioning UI (HTTP/DNS)
end
Dash-->>Lib : JSON Commands (built-in/custom)
Lib->>Dev : invoke setUserCommandHandler(msg)
Dev->>Lib : sendTo(target, builder)
Lib->>RT : websocket send
RT-->>Dash : Widget updates / responses
```

**Diagram sources**
- [hyperwisor-iot.cpp:13-137](file://src/hyperwisor-iot.cpp#L13-L137)
- [hyperwisor-iot.cpp:313-405](file://src/hyperwisor-iot.cpp#L313-L405)
- [CommandHandler.ino:25-85](file://examples/CommandHandler/CommandHandler.ino#L25-L85)
- [WidgetUpdate.ino:44-66](file://examples/WidgetUpdate/WidgetUpdate.ino#L44-L66)

## Detailed Component Analysis

### BasicSetup Tutorial
- Purpose: Minimal working example to initialize the device and maintain connectivity.
- Steps:
  1. Include the library and instantiate the device object.
  2. Call begin() in setup(); it will attempt saved credentials or start AP provisioning.
  3. Call loop() continuously to keep connections alive.
- Expected behavior:
  - On first boot without credentials, device starts AP mode and serves a provisioning page.
  - After successful provisioning, device connects to WiFi and initializes real-time communication.
- Circuit considerations:
  - No external components required; relies on ESP32's integrated radio and flash storage.
- Adaptation guidelines:
  - Replace placeholder IDs in later examples with actual target/widget IDs from your dashboard.

**Section sources**
- [BasicSetup.ino:1-39](file://examples/BasicSetup/BasicSetup.ino#L1-L39)
- [hyperwisor-iot.cpp:13-137](file://src/hyperwisor-iot.cpp#L13-L137)

### WiFi Provisioning Flow
- Purpose: Demonstrate AP provisioning and credential lifecycle.
- Steps:
  1. Check hasCredentials() to determine provisioning state.
  2. If missing, device starts AP mode and serves a provisioning page.
  3. After saving credentials, device restarts and connects automatically.
- Expected behavior:
  - AP mode times out after a period and reboots to prevent stuck states.
  - Provisioning success/error pages return to the app via deep link.
- Troubleshooting:
  - If AP mode persists, verify provisioning form submission and credentials storage.
  - Ensure the device can reach the internet post-provisioning for real-time features.

**Section sources**
- [WiFiProvisioning.ino:1-58](file://examples/WiFiProvisioning/WiFiProvisioning.ino#L1-L58)
- [hyperwisor-iot.cpp:141-185](file://src/hyperwisor-iot.cpp#L141-L185)

### Manual Provisioning
- Purpose: Pre-configure credentials and optional API keys programmatically.
- Steps:
  1. Use setCredentials() to write SSID, password, device ID, and optional user ID.
  2. Optionally call setApiKeys() for backend operations.
  3. Call begin() to connect immediately.
- Expected behavior:
  - Device boots directly into STA mode without AP provisioning.
  - Useful for factory-fresh or locked-down deployments.

**Section sources**
- [Manual_Provisioning_Example.ino:1-65](file://examples/Manual_Provisioning_Example/Manual_Provisioning_Example.ino#L1-L65)
- [hyperwisor-iot.cpp:432-518](file://src/hyperwisor-iot.cpp#L432-L518)

### Conditional Provisioning
- Purpose: Combine manual and automatic provisioning for flexible deployments.
- Steps:
  1. Detect missing credentials at startup.
  2. Optionally set manual credentials for first boot.
  3. Initialize device; fallback to AP mode if manual provisioning is disabled.
- Expected behavior:
  - Provides a graceful fallback strategy for diverse deployment scenarios.

**Section sources**
- [Conditional_Provisioning_Example.ino:1-69](file://examples/Conditional_Provisioning_Example/Conditional_Provisioning_Example.ino#L1-L69)

### CommandHandler for Custom Command Processing
- Purpose: Receive and process custom commands from the dashboard/app.
- Steps:
  1. Register a user command handler with setUserCommandHandler().
  2. Parse payload.commands[] and handle custom command/action pairs.
  3. Use sendTo() to respond to the sender.
- Expected behavior:
  - Built-in commands (e.g., GPIO_MANAGEMENT, OTA, DEVICE_STATUS) are handled automatically; custom commands are routed to your handler.
- Integration tips:
  - Use findCommand()/findAction()/findParams() helpers to simplify parsing.

```mermaid
sequenceDiagram
participant Dash as "Dashboard"
participant Lib as "HyperwisorIOT"
participant App as "User Handler"
Dash->>Lib : JSON payload with commands[]
Lib->>App : setUserCommandHandler(msg)
App->>Lib : sendTo(from, responseBuilder)
Lib-->>Dash : Response payload
```

**Diagram sources**
- [hyperwisor-iot.cpp:313-405](file://src/hyperwisor-iot.cpp#L313-L405)
- [CommandHandler.ino:25-85](file://examples/CommandHandler/CommandHandler.ino#L25-L85)

**Section sources**
- [CommandHandler.ino:1-96](file://examples/CommandHandler/CommandHandler.ino#L1-L96)
- [hyperwisor-iot.h:142-146](file://src/hyperwisor-iot.h#L142-L146)

### WidgetUpdate Demonstrations
- Purpose: Update dashboard widgets with various data types.
- Steps:
  1. In loop(), periodically compute or simulate values.
  2. Call updateWidget(targetId, widgetId, value) with:
     - String values (e.g., "XX.X%")
     - Numeric values (float/int)
     - Array values (for charts/graphs)
- Expected behavior:
  - Widgets reflect live updates; arrays render as series data.
- Integration tips:
  - Use a fixed update interval to balance responsiveness and bandwidth.

**Section sources**
- [WidgetUpdate.ino:1-68](file://examples/WidgetUpdate/WidgetUpdate.ino#L1-L68)
- [hyperwisor-iot.cpp:551-598](file://src/hyperwisor-iot.cpp#L551-L598)

### SmartHomeSwitch for Complex Hardware Control
- Purpose: Bidirectional control of relays with offline persistence and online synchronization.
- Hardware:
  - 7 relays on GPIO pins 32, 33, 27, 19, 18, 25, 26.
  - 7 physical switches on GPIO pins 36, 39, 34, 35, 4, 5, 23 with debouncing.
- Steps:
  1. Initialize relay pins and restore previous states from Preferences.
  2. Set up user command handler to process RELAY_CONTROL and widget-based commands.
  3. Handle physical switch presses with debouncing and toggle relay states.
  4. Persist relay states to Preferences and optionally push updates to the dashboard.
- Expected behavior:
  - Power loss resume restores relay states.
  - Cloud and local control are synchronized.
- Circuit considerations:
  - Ensure proper relay wiring and flyback diodes for inductive loads.
- Integration tips:
  - Map widget IDs to relay numbers dynamically for dashboard control.

```mermaid
flowchart TD
Start(["Loop"]) --> ReadInputs["Read Physical Switches"]
ReadInputs --> Debounce{"Debounced Change?"}
Debounce --> |No| Sleep["Small Delay"] --> LoopBack["Loop"]
Debounce --> |Yes| Toggle["Toggle Relay State"]
Toggle --> Persist["Persist to Preferences"]
Persist --> CloudUpdate{"WiFi Connected?"}
CloudUpdate --> |Yes| SendUpdate["Send Widget Update"]
CloudUpdate --> |No| Skip["Skip Update"]
SendUpdate --> LoopBack
Skip --> LoopBack
```

**Diagram sources**
- [SmartHomeSwitch.ino:241-327](file://examples/SmartHomeSwitch/SmartHomeSwitch.ino#L241-L327)

**Section sources**
- [SmartHomeSwitch.ino:1-328](file://examples/SmartHomeSwitch/SmartHomeSwitch.ino#L1-L328)
- [hyperwisor-iot.cpp:64-137](file://src/hyperwisor-iot.cpp#L64-L137)

### SensorDataLogger for Sensor Integration
- Purpose: Periodically log sensor readings to the platform for visualization.
- Steps:
  1. In loop(), at a fixed interval, read simulated or real sensors.
  2. Call send_Sensor_Data_logger(targetId, configId, {{"key", value}, ...}).
- Expected behavior:
  - Structured sensor data appears in charts/graphs on the dashboard.
- Integration tips:
  - Use realistic sensor drivers (e.g., I2C/Temperature/Humidity) and calibrate values.

**Section sources**
- [SensorDataLogger.ino:1-77](file://examples/SensorDataLogger/SensorDataLogger.ino#L1-L77)
- [hyperwisor-iot.cpp:535-549](file://src/hyperwisor-iot.cpp#L535-L549)

### ThreeDWidgetControl for 3D Visualization
- Purpose: Control multiple 3D models within a 3D Widget (position, rotation, scale, material).
- Steps:
  1. Prepare a vector of ThreeDModelUpdate structures.
  2. Populate modelId, position[], rotation[], scale[], and material properties.
  3. Call update3DWidget(targetId, widgetId, updates) periodically.
- Expected behavior:
  - Models animate independently with different transforms and materials.
- Integration tips:
  - Use small update intervals to keep motion smooth; throttle when off Wi-Fi.

**Section sources**
- [ThreeDWidgetControl.ino:1-85](file://examples/ThreeDWidgetControl/ThreeDWidgetControl.ino#L1-L85)
- [hyperwisor-iot.h:24-35](file://src/hyperwisor-iot.h#L24-L35)
- [hyperwisor-iot.cpp:686-714](file://src/hyperwisor-iot.cpp#L686-L714)

### AttitudeWidgetControl with MPU6050
- Purpose: Drive a Flight Attitude Widget using accelerometer/gyroscope data.
- Steps:
  1. Initialize I2C and MPU6050 with custom SDA/SCL pins.
  2. In loop(), at a fixed interval, read acceleration and compute roll/pitch.
  3. Call updateFlightAttitude(targetId, widgetId, roll, pitch).
- Expected behavior:
  - Dashboard displays real-time attitude visualization.
- Integration tips:
  - Calibrate sensor offsets and filter noise for stable readings.

**Section sources**
- [AttitudeWidget_MPU6050.ino:1-95](file://examples/AttitudeWidget_MPU6050/AttitudeWidget_MPU6050.ino#L1-L95)

### I2C Relay Control Implementation (PCF8574)
- Purpose: Control I2C expanders (e.g., PCF8574) via real-time commands.
- Steps:
  1. Initialize I2C and PCF8574 expander with desired pin modes.
  2. Set API keys for backend operations (if needed).
  3. Register a user command handler to parse Control_Relay commands and act on Relay_1_ON/OFF and Relay_2_ON/OFF actions.
  4. Toggle expander pins to control relays.
- Expected behavior:
  - Dashboard can remotely turn relays on/off; device responds to commands.
- Integration tips:
  - Ensure correct I2C pull-ups and address selection; verify expander wiring.

**Section sources**
- [i2c_PCF8574_relay.ino:1-116](file://examples/i2c_PCF8574_relay/i2c_PCF8574_relay.ino#L1-L116)

### Hydrophonics Controller - Comprehensive Automation System
- Purpose: **NEW** Production-ready controller for hydroponic/aquaponic systems with offline automation, NTP-based scheduling, and thermostat control.
- Key Features:
  - **Offline Operation**: Complete automation runs locally on ESP32 without cloud connectivity
  - **NTP-Based Scheduling**: Time-of-day scheduling using network time synchronization
  - **Looping Timers**: Repeating ON/OFF cycles for pumps, aerators, and misting systems
  - **Local Thermostat Control**: Temperature-based relay control using DS18B20 sensors
  - **Persistent Memory**: All settings saved to NVS flash storage
  - **Priority System**: Manual commands override all automations
  - **Boot-Time Enforcement**: Handles schedules that were active during power loss
- Hardware Requirements:
  - ESP32 Development Board (any variant with WiFi)
  - 8-Channel Relay Module (active-low or active-high supported)
  - DS18B20 Temperature Sensors (optional, up to 8 sensors)
  - 4.7kΩ Resistor (for DS18B20 data line pull-up)
- Architecture:
  - **Schedules** (NTP Time) → **Timers** (millis()) → **Thermostat** (DS18B20) → **Relay Control** (8 Channels)
  - **NVS Persistent Memory** stores widget IDs, timer settings, and schedules
- Steps:
  1. Initialize NVS storage and load persistent settings
  2. Set up 8 relays with configurable pin mapping and polarity
  3. Initialize DS18B20 temperature sensors with automatic resolution
  4. Register user command handler for cloud-based configuration
  5. Implement priority-based automation evaluation
  6. Run continuous loops for timers, schedules, and temperature monitoring
- Expected Behavior:
  - All automations persist across power cycles
  - Schedules continue running during WiFi outages
  - Manual cloud commands instantly override all local automations
  - Temperature monitoring prevents equipment damage
- Integration Tips:
  - Use appropriate hysteresis (0.5-2.0°C) to prevent rapid cycling
  - Monitor NVS wear - avoid excessive config saves (>10k writes)
  - Implement watchdog timers for critical applications
  - Add manual override buttons in cloud UI

```mermaid
flowchart TD
Start(["Boot/Power Loss"]) --> LoadNVS["Load NVS Settings"]
LoadNVS --> InitHW["Initialize Relays & Sensors"]
InitHW --> RegisterHandler["Register Cloud Handler"]
RegisterHandler --> Priority{"Manual Command?"}
Priority --> |Yes| DisableAuto["Disable All Automations"] --> ApplyManual["Apply Manual Control"]
Priority --> |No| CheckSchedules["Check Time Schedules"]
CheckSchedules --> InWindow{"In Scheduled Window?"}
InWindow --> |Yes| ApplySchedule["Apply Schedule Mode<br/>RELAY_ON/OFF/THERMOSTAT/TIMER"]
InWindow --> |No| DisableSched["Disable Schedule Mode"]
DisableSched --> CheckThermo["Check Thermostat Logic"]
ApplySchedule --> CheckThermo
CheckThermo --> EvalTimers["Evaluate Looping Timers"]
EvalTimers --> UpdateWidgets["Update Dashboard Widgets"]
UpdateWidgets --> LoopBack["Main Loop"]
ApplyManual --> LoopBack
```

**Diagram sources**
- [hydrophonics_controller.ino:331-411](file://examples/hydrophonics/hydrophonics_controller.ino#L331-L411)
- [hydrophonics_controller.ino:221-278](file://examples/hydrophonics/hydrophonics_controller.ino#L221-L278)
- [hydrophonics_controller.ino:280-328](file://examples/hydrophonics/hydrophonics_controller.ino#L280-L328)

**Section sources**
- [hydrophonics_controller.ino:1-665](file://examples/hydrophonics/hydrophonics_controller.ino#L1-L665)
- [README.md:1-419](file://examples/hydrophonics/README.md#L1-L419)

## Dependency Analysis
- Library dependencies:
  - ArduinoJson: JSON serialization/deserialization for payloads.
  - WebSockets: Real-time communication channel.
  - ESP32 built-ins: WiFi, WebServer, HTTPClient, Preferences, Update, DNSServer, Wire.
- Example dependencies:
  - SmartHomeSwitch: Preferences for GPIO state persistence.
  - AttitudeWidget_MPU6050: Adafruit MPU6050 and Adafruit Sensor libraries.
  - i2c_PCF8574_relay: PCF8574 library and Wire.
  - **New**: hydrophonics: OneWire, DallasTemperature, Preferences, time.h libraries.

```mermaid
graph LR
L["hyperwisor-iot.h/cpp"] --> AJ["ArduinoJson"]
L --> WS["WebSockets"]
L --> ESP["ESP32 Built-ins"]
SH["SmartHomeSwitch.ino"] --> PREF["Preferences"]
MPU["AttitudeWidget_MPU6050.ino"] --> AF_MPU["Adafruit MPU6050"]
I2C["i2c_PCF8574_relay.ino"] --> PCF["PCF8574"]
HYDRO["hydrophonics_controller.ino"] --> OW["OneWire"]
HYDRO --> DT["DallasTemperature"]
HYDRO --> PREF["Preferences"]
HYDRO --> TIME["time.h"]
```

**Diagram sources**
- [library.properties:10-11](file://library.properties#L10-L11)
- [SmartHomeSwitch.ino:20-24](file://examples/SmartHomeSwitch/SmartHomeSwitch.ino#L20-L24)
- [AttitudeWidget_MPU6050.ino:13-17](file://examples/AttitudeWidget_MPU6050/AttitudeWidget_MPU6050.ino#L13-L17)
- [i2c_PCF8574_relay.ino:1-6](file://examples/i2c_PCF8574_relay/i2c_PCF8574_relay.ino#L1-L6)
- [hydrophonics_controller.ino:8-11](file://examples/hydrophonics/hydrophonics_controller.ino#L8-L11)

**Section sources**
- [library.properties:10-11](file://library.properties#L10-L11)

## Performance Considerations
- Keep loop() lightweight:
  - Use non-blocking delays and periodic timers for updates.
  - Throttle widget updates and sensor reads to reduce bandwidth and CPU usage.
- Optimize real-time reconnections:
  - The library automatically retries WebSocket connections; avoid frequent restarts.
- Memory management:
  - Prefer stack-local buffers for small payloads; use DynamicJsonDocument for larger ones.
- I2C and sensors:
  - Use appropriate pull-up resistors and bus speeds; debounce mechanical switches.
- OTA and provisioning:
  - Limit OTA triggers to necessary updates; ensure stable Wi-Fi during transfers.
- **New**: Hydrophonics Controller optimizations:
  - Use non-blocking sensor reads with requestTemperatures() and waitForConversion(false)
  - Implement efficient NVS storage with selective updates
  - Use appropriate intervals: STATUS_UPDATE_INTERVAL (2s), TEMP_UPDATE_INTERVAL (5s), SCHEDULE_CHECK_INTERVAL (10s)
  - Implement boot-time schedule enforcement to prevent "ghost states"

## Troubleshooting Guide
- Device stuck in AP mode:
  - Cause: Provisioning page not submitted or credentials not saved.
  - Resolution: Complete provisioning via the app; verify credentials with hasCredentials().
- No real-time connectivity:
  - Cause: Wi-Fi disconnect or WebSocket errors.
  - Resolution: Check network credentials, router availability, and serial logs for reconnection attempts.
- Widget updates not appearing:
  - Cause: Incorrect targetId/widgetId or missing dashboard configuration.
  - Resolution: Confirm IDs match dashboard definitions; ensure device is online.
- GPIO state not persisting:
  - Cause: Missing Preferences initialization or incorrect pin usage.
  - Resolution: Initialize Preferences before use; verify saveGPIOState() calls after state changes.
- 3D widget anomalies:
  - Cause: Invalid model IDs or malformed material properties.
  - Resolution: Validate modelId strings and numeric ranges for position/rotation/scale.
- I2C relay not responding:
  - Cause: Wrong I2C address, missing pull-ups, or incorrect pin modes.
  - Resolution: Verify expander address and wiring; confirm pin directions and logic levels.
- **New**: Hydrophonics Controller issues:
  - Schedules not executing at correct times: Verify NTP time sync is working and timezone is set correctly.
  - Timer not cycling: Check timer activation state and ensure onTimeMs/offTimeMs are set correctly.
  - Thermostat not responding: Verify DS18B20 sensor detection and correct sensorIndex mapping.
  - Manual override not working: Ensure cloud commands properly disable all automations.
  - NVS storage corruption: Check for excessive writes and consider implementing write batching.

**Section sources**
- [hyperwisor-iot.cpp:46-137](file://src/hyperwisor-iot.cpp#L46-L137)
- [WiFiProvisioning.ino:27-52](file://examples/WiFiProvisioning/WiFiProvisioning.ino#L27-L52)
- [SmartHomeSwitch.ino:118-149](file://examples/SmartHomeSwitch/SmartHomeSwitch.ino#L118-L149)
- [hydrophonics_controller.ino:348-411](file://examples/hydrophonics/hydrophonics_controller.ino#L348-L411)

## Conclusion
The Hyperwisor-IOT library simplifies ESP32 IoT development by handling provisioning, real-time communication, and dashboard integrations out of the box. These examples demonstrate how to progressively build from basic connectivity to advanced hardware control, sensor logging, 3D visualization, and comprehensive automation systems. The new hydroponic/aquaponic controller showcases production-ready offline automation with NTP-based scheduling, demonstrating how to create robust, fail-safe systems that operate independently of cloud connectivity while maintaining seamless integration with Hyperwisor's dashboard platform. By following the step-by-step guides, applying the troubleshooting tips, and optimizing for performance, you can adapt these patterns to a wide range of applications including agriculture, aquaculture, industrial control, and environmental monitoring.

## Appendices

### Integration Patterns with Third-Party Sensors, Actuators, and Cloud Services
- Sensors:
  - Use established libraries (e.g., Adafruit sensor suites) and feed values into updateWidget() or send_Sensor_Data_logger().
  - **New**: DS18B20 sensors with OneWire protocol for precise temperature monitoring in hydroponic systems.
- Actuators:
  - Relay modules, motor drivers, and solenoids can be controlled via GPIO or I2C expanders; persist states and synchronize with dashboard widgets.
  - **New**: 8-channel relay modules for comprehensive hydroponic/aquaponic control.
- Cloud services:
  - Use setApiKeys() and database/cloud APIs to store and retrieve data; authenticate users and onboard devices as needed.
  - **New**: NTP time synchronization enables precise scheduling without external RTC modules.

### Advanced Automation Patterns
- **Priority-Based Control**: Manual cloud commands override all local automations for safety and emergency situations.
- **Boot-Time Enforcement**: System checks current time against schedules on startup to prevent "ghost states" where relays should have been ON but weren't.
- **Non-Blocking Operations**: Efficient use of millis() timing prevents blocking operations while maintaining responsive control.
- **Persistent State Management**: NVS storage ensures settings survive power cycles and WiFi outages.