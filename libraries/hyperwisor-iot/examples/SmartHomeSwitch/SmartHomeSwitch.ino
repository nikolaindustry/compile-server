/**
 * Smart Home Switch Example (6-Channel)
 * 
 * A 6-channel smart home switch that works:
 * - OFFLINE: Control via physical buttons with state persistence
 * - ONLINE: Remote control via Hypervisor dashboard when internet is available
 * 
 * Features:
 * - 6 relay outputs with physical switch inputs
 * - Power loss resume - relay states are saved and restored after power loss
 * - Debounced switch inputs for reliable operation
 * - Automatic WiFi reconnection
 * - Real-time bidirectional control (cloud and physical)
 * 
 * Hardware Setup:
 * - Relays: Pins 32, 33, 25, 26, 18, 19
 * - Switches: Pins 36, 39, 34, 35, 4, 23
 */

#include <hyperwisor-iot.h>
#include <Preferences.h>

HyperwisorIOT device;
Preferences relayPrefs;
Preferences widgetMapping;

// --- PIN DEFINITIONS & CONFIGURATION ---
const int numRelays = 6;
const int relayPins[] = { 32, 33, 25, 26, 18, 19 };

const int numButtons = 6;
const int buttonPins[] = { 36, 39, 34, 35, 4, 23 };

// --- STATE VARIABLES ---
bool relayStates[numRelays];
int buttonLastStates[numButtons];

// --- DYNAMIC WIDGET ID MAPPING ---
String widgetIdCache[numRelays];

// Debounce delay
const int DEBOUNCE_DELAY = 50;

// --- HELPER FUNCTION TO UPDATE & SAVE RELAY STATE ---
void updateRelay(int relayNum, bool state) {
  if (relayNum < 1 || relayNum > numRelays) return;
  
  int index = relayNum - 1;
  relayStates[index] = state;
  digitalWrite(relayPins[index], state);
  
  String key = "r" + String(relayNum);
  relayPrefs.putBool(key.c_str(), state);
  
  Serial.printf("Relay %d set to %s\n", relayNum, state ? "ON" : "OFF");
}

// --- HELPER FUNCTION TO SAVE WIDGET ID MAPPING ---
void saveWidgetMapping(int relayNum, const String& widgetId) {
  if (relayNum < 1 || relayNum > numRelays) return;
  
  String key = "widget_" + String(relayNum);
  widgetMapping.begin("widget-map", false);
  widgetMapping.putString(key.c_str(), widgetId);
  widgetMapping.end();
  
  // Update cache
  widgetIdCache[relayNum - 1] = widgetId;
  
  Serial.printf("Widget mapping saved: Relay %d -> %s\n", relayNum, widgetId.c_str());
}

// --- HELPER FUNCTION TO LOAD WIDGET ID MAPPINGS ---
void loadWidgetMappings() {
  widgetMapping.begin("widget-map", true);
  
  for (int i = 1; i <= numRelays; i++) {
    String key = "widget_" + String(i);
    String widgetId = widgetMapping.getString(key.c_str(), "");
    widgetIdCache[i - 1] = widgetId;
    
    if (!widgetId.isEmpty()) {
      Serial.printf("Loaded mapping: Relay %d -> %s\n", i, widgetId.c_str());
    }
  }
  
  widgetMapping.end();
}

// --- HELPER FUNCTION TO MAP WIDGET ID TO RELAY NUMBER ---
int getRelayNumberFromWidgetId(const String& widgetId) {
  if (widgetId.isEmpty()) return 0;
  
  // Check cache first
  for (int i = 0; i < numRelays; i++) {
    if (widgetIdCache[i] == widgetId) {
      return i + 1;
    }
  }
  return 0; // Unknown widget ID
}

// --- HELPER FUNCTION TO GET WIDGET ID FROM RELAY NUMBER ---
String getWidgetIdFromRelayNumber(int relayNum) {
  if (relayNum < 1 || relayNum > numRelays) return "";
  return widgetIdCache[relayNum - 1];
}

// --- HELPER FUNCTION TO LEARN WIDGET MAPPING FROM COMMAND ---
void learnWidgetMapping(const String& widgetId, JsonObject& params) {
  int relayNum = 0;
  
  if (params.containsKey("relay")) {
    relayNum = params["relay"] | 0;
  } else if (params.containsKey("relayNumber")) {
    relayNum = params["relayNumber"] | 0;
  } else if (params.containsKey("relay_number")) {
    relayNum = params["relay_number"] | 0;
  } else if (params.containsKey("channel")) {
    relayNum = params["channel"] | 0;
  }
  
  if (relayNum >= 1 && relayNum <= numRelays) {
    // Check if this widget is already mapped
    int existingMapping = getRelayNumberFromWidgetId(widgetId);
    if (existingMapping == 0 || existingMapping != relayNum) {
      // New mapping or changed mapping
      saveWidgetMapping(relayNum, widgetId);
    }
  }
}

// --- HELPER FUNCTION TO SEND STATE UPDATE TO CLOUD ---
void sendRelayStateToCloud(int relayNum, bool state) {
  // Only send if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    String userId = device.getUserId();
    if (!userId.isEmpty() && userId != "unknown") {
      String widgetId = getWidgetIdFromRelayNumber(relayNum);
      if (!widgetId.isEmpty()) {
        device.updateWidget(userId, widgetId, state ? "ON" : "OFF");
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Smart Home Switch ===\n");
  Serial.printf("%d-Channel Switch with Offline & Online Control\n", numRelays);
  
  // --- 1. OPEN RELAY STATE STORAGE ---
  relayPrefs.begin("relay-storage", false);
  
  // Load widget ID mappings from storage
  loadWidgetMappings();
  
  // --- 2. SETUP RELAY PINS ---
  for (int i = 0; i < numRelays; i++) {
    pinMode(relayPins[i], OUTPUT);
  }
  
  // --- 3. RESTORE PREVIOUS STATES (Power Loss Resume) ---
  Serial.println("Power Loss Resume: Restoring States...");
  for (int i = 0; i < numRelays; i++) {
    String key = "r" + String(i + 1);
    relayStates[i] = relayPrefs.getBool(key.c_str(), false);
    digitalWrite(relayPins[i], relayStates[i]);
    Serial.printf("Relay %d: %d | ", i + 1, relayStates[i]);
  }
  Serial.println("\nStates Restored.");
  
  // --- 4. SETUP PHYSICAL SWITCH PINS ---
  for (int i = 0; i < numButtons; i++) {
    pinMode(buttonPins[i], INPUT);
    buttonLastStates[i] = digitalRead(buttonPins[i]);
  }
  
  // --- 5. SETUP CLOUD COMMAND HANDLER ---
  device.setUserCommandHandler([](JsonObject& msg) {
    if (!msg.containsKey("payload")) return;
    JsonObject payload = msg["payload"];
    
    // Handle commands with actions that contain widgetId in params
    JsonArray commands = payload["commands"];
    for (JsonObject cmd : commands) {
      String command = cmd["command"].as<String>();
      
      JsonArray actions = cmd["actions"];
      for (JsonObject action : actions) {
        String actionName = action["action"].as<String>();
        JsonObject params = action["params"];
        
        // Check if widgetId is in params (Hypervisor platform format)
        if (params.containsKey("widgetId")) {
          String widgetId = params["widgetId"].as<String>();
          
          // Try to learn the mapping if relay number is provided
          learnWidgetMapping(widgetId, params);
          
          // Map the arbitrary widgetId to a relay number
          int relayNum = getRelayNumberFromWidgetId(widgetId);
          
          if (relayNum >= 1 && relayNum <= numRelays) {
            // Determine state from action or params
            bool newState = false;
            
            // Check action name for ON/OFF
            if (actionName.equalsIgnoreCase("ON") || actionName.equalsIgnoreCase("on") || 
                actionName.equalsIgnoreCase("onn") || actionName == "1") {
              newState = true;
            } else if (actionName.equalsIgnoreCase("OFF") || actionName.equalsIgnoreCase("off") || 
                       actionName == "0") {
              newState = false;
            }
            // Check if there's a status or value in params
            else if (params.containsKey("status")) {
              String status = params["status"].as<String>();
              newState = (status == "ON" || status == "on" || status == "1" || status == "HIGH");
            } else if (params.containsKey("value")) {
              String value = params["value"].as<String>();
              newState = (value == "ON" || value == "on" || value == "1" || value == "true");
            }
            // If action name contains "on" or "off" string
            else if (actionName.indexOf("on") >= 0 || actionName.indexOf("On") >= 0) {
              newState = true;
            } else if (actionName.indexOf("off") >= 0 || actionName.indexOf("Off") >= 0) {
              newState = false;
            }
            
            updateRelay(relayNum, newState);
            Serial.printf("Cloud command: Widget=%s, Relay=%d, Action=%s, State=%s\n",
                         widgetId.c_str(), relayNum, actionName.c_str(), newState ? "ON" : "OFF");
          } else {
            // Widget ID not yet mapped
            Serial.printf("Unknown widgetId: %s - Send a command with 'relay' param to map it\n", widgetId.c_str());
          }
        }
        // Fallback: handle commands with relay number directly in params
        else if (params.containsKey("relay")) {
          int relayNum = params["relay"] | 0;
          String status = params["status"] | "OFF";
          bool newState = (status == "ON" || status == "1" || status == "HIGH");
          
          if (relayNum >= 1 && relayNum <= numRelays) {
            updateRelay(relayNum, newState);
          }
        }
      }
    }
  });
  
  // --- 6. INITIALIZE DEVICE (WiFi & Cloud Connection) ---
  device.begin();
  
  Serial.println("\nDevice initialized!");
  Serial.println("Device ID: " + device.getDeviceId());
  Serial.println("User ID: " + device.getUserId());
  Serial.println("\nSystem ready for offline and online control");
}

void loop() {
  // Keep the device connection alive
  device.loop();
  
  // --- HANDLE PHYSICAL SWITCHES ---
  for (int i = 0; i < numButtons; i++) {
    int currentButtonState = digitalRead(buttonPins[i]);
    if (currentButtonState != buttonLastStates[i]) {
      delay(DEBOUNCE_DELAY);
      if (digitalRead(buttonPins[i]) == currentButtonState) {
        int relayNum = i + 1;
        bool newState = !relayStates[i];
        updateRelay(relayNum, newState);
        sendRelayStateToCloud(relayNum, newState);
        buttonLastStates[i] = currentButtonState;
      }
    }
  }
  
  // Small delay to prevent excessive CPU usage
  delay(10);
}
