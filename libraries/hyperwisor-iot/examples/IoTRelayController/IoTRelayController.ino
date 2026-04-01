/**
 * IoT Relay Controller with Temperature Monitoring - Hyperwisor IoT
 * 
 * A simple IoT-based relay controller using Hyperwisor cloud platform.
 * Control up to 8 relays remotely via Hyperwisor dashboard with temperature monitoring.
 * 
 * Features:
 * - Pure IoT cloud control - no automatic temperature control
 * - Real-time relay control via Hyperwisor dashboard
 * - Temperature monitoring and data logging
 * - Individual relay state feedback
 * - Active-LOW relay support (common relay modules)
 * - No schedules, no rules - manual cloud control only
 * 
 * Hardware:
 * - ESP32 board
 * - 8-channel relay module (active-LOW or active-HIGH)
 * - DS18B20 temperature sensors (optional, for monitoring)
 * - Connect relay pins to GPIOs defined below
 * - Connect DS18B20 sensors to ONE_WIRE_PIN with 4.7kΩ pull-up resistor
 * 
 * Usage:
 * - Upload code and power on ESP32
 * - Device connects to Hyperwisor cloud automatically
 * - Control relays from Hyperwisor dashboard widgets
 * - Monitor relay states and temperature in real-time
 * - Temperature data logged to dashboard for historical tracking
 */

#include <hyperwisor-iot.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// ============== CONFIGURATION ==============

// One-Wire pin for DS18B20 temperature sensors (optional)
#define ONE_WIRE_PIN 2

String apikey = "";
String secretkey = "";
String productId = "";
String deviceId = "";
String userId = "";
String msgfrom = "";

// Relay configuration
const int NUM_RELAYS = 8;
const int RELAY_PINS[] = {32, 33, 25, 26, 27, 4, 16, 17};  // 8 relay channels

// Active-LOW relay configuration (relay turns ON when GPIO is LOW)
// Most common relay modules are active-LOW
const bool ACTIVE_LOW_RELAYS = true;  // Set to false for active-high relays

// Dashboard and Widget IDs - Replace with your actual IDs
String TARGET_ID = "your-dashboard-id";  // Your Hyperwisor dashboard ID
String CONFIG_ID = "your-config-id";     // Data logger config ID (for send_Sensor_Data_logger)

// Relay control widgets (all 8 relays)
String RELAY_WIDGET_IDS[] = {
  "widget-relay-1", "widget-relay-2", "widget-relay-3", "widget-relay-4",
  "widget-relay-5", "widget-relay-6", "widget-relay-7", "widget-relay-8"
};

// Temperature sensor widgets (for data logging and display)
String TEMP_WIDGET_IDS[] = {
  "widget-temp-1", "widget-temp-2", "widget-temp-3", "widget-temp-4",
  "widget-temp-5", "widget-temp-6", "widget-temp-7", "widget-temp-8"
};

// Update intervals
const unsigned long STATUS_UPDATE_INTERVAL = 2000;  // Send status every 2 seconds
const unsigned long TEMP_UPDATE_INTERVAL = 5000;    // Send temperature every 5 seconds

// ============== GLOBAL VARIABLES ==============

HyperwisorIOT device;

// Temperature sensor setup
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);
uint8_t sensorCount = 0;

// State tracking
unsigned long lastStatusUpdate = 0;
unsigned long lastTempUpdate = 0;
bool relayStates[NUM_RELAYS] = {false, false, false, false, false, false, false, false};
bool lastRelayStates[NUM_RELAYS] = {false, false, false, false, false, false, false, false};

// ============== HELPER FUNCTIONS ==============

/**
 * Initialize relay pins
 */
void initRelays() {
  Serial.println("\n=== Initializing Relays ===");
  
  for (int i = 0; i < NUM_RELAYS; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    // Start with relays OFF (HIGH for active-low, LOW for active-high)
    digitalWrite(RELAY_PINS[i], ACTIVE_LOW_RELAYS ? HIGH : LOW);
    relayStates[i] = false;
    lastRelayStates[i] = false;
    
    Serial.print("Relay ");
    Serial.print(i + 1);
    Serial.print(" on GPIO ");
    Serial.print(RELAY_PINS[i]);
    Serial.println(ACTIVE_LOW_RELAYS ? " (Active-LOW)" : " (Active-HIGH)");
  }
  
  Serial.println("Relays initialized successfully\n");
}

/**
 * Initialize DS18B20 temperature sensors (optional)
 */
void initSensors() {
  Serial.println("\n=== Initializing Temperature Sensors ===");
  
  sensors.begin();
  sensorCount = sensors.getDeviceCount();
  
  Serial.print("Found ");
  Serial.print(sensorCount);
  Serial.println(" DS18B20 sensor(s)");
  
  if (sensorCount == 0) {
    Serial.println("ℹ️  No temperature sensors found - running in relay-only mode");
    Serial.println("   You can still monitor temperature if sensors are added later");
  } else {
    // Set resolution for all sensors to 12 bits (highest accuracy)
    for (uint8_t i = 0; i < sensorCount; i++) {
      DeviceAddress addr;
      if (sensors.getAddress(addr, i)) {
        sensors.setResolution(addr, 12);  // Highest accuracy
      }
    }
    Serial.println("✓ Temperature sensors ready for monitoring and data logging");
  }
  Serial.println();
}

/**
 * Set relay state
 * 
 * @param relayIndex Index of relay (0 to NUM_RELAYS-1)
 * @param newState true to turn ON, false to turn OFF
 */
void setRelay(int relayIndex, bool newState) {
  if (relayIndex < 0 || relayIndex >= NUM_RELAYS) return;
  
  relayStates[relayIndex] = newState;
  
  // Active-LOW: LOW = ON, HIGH = OFF
  // Active-HIGH: HIGH = ON, LOW = OFF
  digitalWrite(RELAY_PINS[relayIndex], (newState && ACTIVE_LOW_RELAYS) ? LOW : HIGH);
  
  Serial.printf("Relay #%d (GPIO %d): %s\n", 
               relayIndex + 1, RELAY_PINS[relayIndex], newState ? "ON" : "OFF");
}

/**
 * Send relay status to dashboard widgets
 */
void updateRelayWidgets() {
  for (int i = 0; i < NUM_RELAYS; i++) {
    // Only send if state changed
    if (relayStates[i] != lastRelayStates[i]) {
      String stateStr = relayStates[i] ? "ON" : "OFF";
      device.updateWidget(TARGET_ID, RELAY_WIDGET_IDS[i], stateStr);
      
      Serial.printf("Relay #%d: %s -> Widget: %s\n", 
                    i + 1, stateStr.c_str(), RELAY_WIDGET_IDS[i].c_str());
      
      lastRelayStates[i] = relayStates[i];
    }
  }
}

/**
 * Send temperature readings to dashboard widgets for data logging
 * Uses both updateWidget (for display) and send_Sensor_Data_logger (for logging)
 */
void updateTemperatureWidgets() {
  // Skip if no sensors available
  if (sensorCount == 0) {
    return;
  }
  
  sensors.requestTemperatures();
  
  // Prepare sensor data array for data logger
  struct SensorData {
    const char* name;
    float value;
  };
  
  SensorData sensorReadings[8];
  uint8_t validSensorCount = 0;
  
  // Read all sensors
  for (uint8_t i = 0; i < sensorCount && i < 8; i++) {
    float tempC = sensors.getTempCByIndex(i);
    
    if (tempC != -127.0) {
      // Update individual temperature widget for real-time display
      device.updateWidget(TARGET_ID, TEMP_WIDGET_IDS[i], tempC);
      
      Serial.printf("Temp Sensor #%d: %.1f°C -> Widget: %s\n", 
                    i + 1, tempC, TEMP_WIDGET_IDS[i].c_str());
      
      // Store for batch data logging
      sensorReadings[validSensorCount].name = strdup(("Sensor_" + String(i + 1)).c_str());
      sensorReadings[validSensorCount].value = tempC;
      validSensorCount++;
    }
  }
  
  // Send all sensor data to data logger in one call
  if (validSensorCount > 0) {
    // Build initializer list dynamically with proper key-value pairs
    switch(validSensorCount) {
      case 1:
        device.send_Sensor_Data_logger(TARGET_ID, CONFIG_ID, {
          {sensorReadings[0].name, sensorReadings[0].value}
        });
        break;
      case 2:
        device.send_Sensor_Data_logger(TARGET_ID, CONFIG_ID, {
          {sensorReadings[0].name, sensorReadings[0].value},
          {sensorReadings[1].name, sensorReadings[1].value}
        });
        break;
      case 3:
        device.send_Sensor_Data_logger(TARGET_ID, CONFIG_ID, {
          {sensorReadings[0].name, sensorReadings[0].value},
          {sensorReadings[1].name, sensorReadings[1].value},
          {sensorReadings[2].name, sensorReadings[2].value}
        });
        break;
      case 4:
        device.send_Sensor_Data_logger(TARGET_ID, CONFIG_ID, {
          {"Sensor_1", sensorReadings[0].value},
          {"Sensor_2", sensorReadings[1].value},
          {"Sensor_3", sensorReadings[2].value},
          {"Sensor_4", sensorReadings[3].value}
        });
        break;
      default:
        // For 5+ sensors, use multiple calls or custom approach
        for (uint8_t i = 0; i < validSensorCount; i++) {
          device.updateWidget(TARGET_ID, "datalog-" + String(i), sensorReadings[i].value);
        }
        break;
    }
    
    Serial.println("Temperature data logged via send_Sensor_Data_logger");
  }
}

// ============== CLOUD COMMAND HANDLER ==============

void setupCloudCommandHandler() {
  device.setUserCommandHandler([](JsonObject& msg) {
    String from = msg["from"].as<String>();
    
    if (!msg.containsKey("payload")) return;
    JsonObject payload = msg["payload"];
    
    Serial.println("\n=== Cloud Command Received ===");
    serializeJson(payload, Serial);
    Serial.println();
    
    // Handle RELAY_CONTROL command for individual relay control (8 channels)
    JsonObject relayCmd = device.findCommand(payload, "RELAY_CONTROL");
    if (!relayCmd.isNull()) {
      JsonArray actions = relayCmd["actions"];
      
      for (JsonObject action : actions) {
        String actionName = action["action"].as<String>();
        JsonObject params = action["params"];
        
        // Support multiple parameter names for flexibility
        int relayChannel = -1;
        
        // Try different parameter names
        if (params.containsKey("channel")) {
          relayChannel = params["channel"] | -1;
        } else if (params.containsKey("relay")) {
          relayChannel = params["relay"] | -1;
        } else if (params.containsKey("relayNumber")) {
          relayChannel = params["relayNumber"] | -1;
        } else if (params.containsKey("relay_index")) {
          relayChannel = params["relay_index"] | -1;
        }
        
        // Validate channel number
        if (relayChannel >= 0 && relayChannel < NUM_RELAYS) {
          bool newState = false;
          
          // Determine desired state from action name or params
          if (actionName.indexOf("ON") >= 0 || actionName.indexOf("On") >= 0 || actionName == "HIGH") {
            newState = true;
          } else if (actionName.indexOf("OFF") >= 0 || actionName.indexOf("Off") >= 0 || actionName == "LOW") {
            newState = false;
          }
          
          // Also check params for explicit state
          if (params.containsKey("state")) {
            String stateStr = params["state"].as<String>();
            newState = (stateStr == "ON" || stateStr == "on" || stateStr == "1" || stateStr == "true" || stateStr == "HIGH");
          }
          
          // Update relay state
          setRelay(relayChannel, newState);
          
          // Update relay widget immediately
          String stateStr = newState ? "ON" : "OFF";
          device.updateWidget(TARGET_ID, RELAY_WIDGET_IDS[relayChannel], stateStr);
          
          // Send confirmation (capture variables by value)
          int responseChannel = relayChannel;
          bool responseState = newState;
          device.sendTo(from, [responseChannel, responseState](JsonObject& response) {
            response["status"] = "success";
            response["message"] = "Relay state updated";
            response["channel"] = responseChannel;
            response["state"] = responseState ? "ON" : "OFF";
          });
        } else {
          Serial.printf("Invalid relay channel: %d (valid: 0-%d)\n", relayChannel, NUM_RELAYS - 1);
        }
      }
    }
    
    // Handle MANUAL_RELAY_CONTROL command (legacy support)
    JsonObject manualCmd = device.findCommand(payload, "MANUAL_RELAY_CONTROL");
    if (!manualCmd.isNull()) {
      JsonArray actions = manualCmd["actions"];
      
      for (JsonObject action : actions) {
        String actionName = action["action"].as<String>();
        JsonObject params = action["params"];
        
        int relayIndex = params["relayIndex"] | params["relay"] | -1;
        
        if (relayIndex >= 0 && relayIndex < NUM_RELAYS) {
          if (actionName == "RELAY_ON" || actionName.indexOf("ON") >= 0) {
            setRelay(relayIndex, true);
          }
          else if (actionName == "RELAY_OFF" || actionName.indexOf("OFF") >= 0) {
            setRelay(relayIndex, false);
          }
          
          // Update relay widget immediately
          updateRelayWidgets();
          
          // Send confirmation
          device.sendTo(from, [](JsonObject& response) {
            response["status"] = "success";
            response["message"] = "Relay state updated";
          });
        }
      }
    }
    
    // Handle GET_STATUS command
    JsonObject statusCmd = device.findCommand(payload, "GET_STATUS");
    if (!statusCmd.isNull()) {
      device.sendTo(from, [](JsonObject& response) {
        response["command"] = "SYSTEM_STATUS";
        response["relayCount"] = NUM_RELAYS;
        response["sensorCount"] = sensorCount;
        response["uptime"] = millis() / 1000;
        response["freeHeap"] = ESP.getFreeHeap();
        
        // Add current time info
        response["timeSynced"] = (time(nullptr) >= 1000000000L);
        response["currentTime"] = device.getNetworkDateTime();
        
        // Add relay states for ALL 8 relays
        JsonArray states = response.createNestedArray("relayStates");
        for (int i = 0; i < NUM_RELAYS; i++) {
          states.add(relayStates[i]);
        }
        
        // Add current temperatures (if sensors available)
        JsonArray temps = response.createNestedArray("temperatures");
        if (sensorCount > 0) {
          for (uint8_t i = 0; i < sensorCount && i < 8; i++) {
            temps.add(sensors.getTempCByIndex(i));
          }
        }
      });
    }
  });
}

// ============== ARDUINO SETUP & LOOP ==============

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n========================================");
  Serial.println("IoT Relay Controller with Temperature Monitoring");
  Serial.println("Powered by Hyperwisor IoT");
  Serial.println("========================================\n");
  
  Serial.println("✓ IoT Control Mode with Data Logging");
  Serial.println("Manual cloud control + Temperature monitoring");
  Serial.println("All relays controllable via Hyperwisor dashboard");
  Serial.println("Temperature data logged via send_Sensor_Data_logger");
  Serial.println("========================================\n");
  
  // Initialize hardware
  initRelays();
  initSensors();  // Initialize temperature sensors
  
  // Setup cloud command handler
  setupCloudCommandHandler();
  
  // Time and date functions
  device.setTimezone("IST");
  
  // Initialize Hyperwisor device
  device.begin();
  
  deviceId = device.getDeviceId();
  Serial.print("Device ID: ");
  Serial.println(deviceId);

  userId = device.getUserId();
  Serial.print("User ID: ");
  Serial.println(userId);

  TARGET_ID = device.getUserId();
  
  Serial.println("\nDevice initialized successfully!");
  Serial.println("Device ID: " + device.getDeviceId());
  Serial.println("Dashboard: " + TARGET_ID);
  if (sensorCount > 0) {
    Serial.println("\nSystem ready - Monitoring temperature and controlling relays\n");
  } else {
    Serial.println("\nSystem ready - Waiting for cloud commands (no sensors detected)\n");
  }
}

void loop() {
  // Handle Hyperwisor connectivity
  device.loop();
  
  // Request temperature readings (non-blocking)
  if (sensorCount > 0) {
    sensors.requestTemperatures();
  }
  
  // Update relay widgets (send state changes to dashboard)
  updateRelayWidgets();
  
  // Send temperature data to dashboard for data logging
  if (millis() - lastTempUpdate >= TEMP_UPDATE_INTERVAL) {
    lastTempUpdate = millis();
    if (sensorCount > 0) {
      updateTemperatureWidgets();
    }
  }
  
  // Send periodic status update
  if (millis() - lastStatusUpdate >= STATUS_UPDATE_INTERVAL) {
    lastStatusUpdate = millis();
    
    // Print status to serial
    Serial.println("\n--- Status Update ---");
    for (int i = 0; i < NUM_RELAYS; i++) {
      Serial.printf("Relay #%d (GPIO %d): %s\n",
                   i + 1, RELAY_PINS[i], relayStates[i] ? "ON" : "OFF");
    }
    
    // Print temperature readings
    if (sensorCount > 0) {
      Serial.println("\nTemperature Readings:");
      for (uint8_t i = 0; i < sensorCount && i < 8; i++) {
        float tempC = sensors.getTempCByIndex(i);
        if (tempC != -127.0) {
          Serial.printf("  Sensor #%d: %.1f°C\n", i + 1, tempC);
        }
      }
      Serial.println("  → Data sent to logger every 5 seconds");
    }
    
    Serial.println("---------------------\n");
  }
  
  delay(100);  // Small delay to prevent watchdog trigger
}
