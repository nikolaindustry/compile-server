/**
 * Universal IoT Relay & Temperature Controller (Production Ready)
 * Features: Non-Blocking Sensors, Dynamic Mapping, NVS Persistent Memory, 
 * Local Thermostat, Looping Timers, Time-of-Day Scheduler, and Full Debugging.
 */

#include <hyperwisor-iot.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Preferences.h>
#include <time.h> 

// ============== CONFIGURATION ==============
#define ONE_WIRE_PIN 2
const int NUM_RELAYS = 8;
const int RELAY_PINS[] = {32, 33, 25, 26, 27, 4, 16, 17};
const bool ACTIVE_LOW_RELAYS = true;

String TARGET_ID = ""; 
String DATA_LOGGER_ID = ""; 

// Dynamic Caches
String RELAY_WIDGET_IDS[NUM_RELAYS] = {"", "", "", "", "", "", "", ""};
String TEMP_WIDGET_IDS[8] = {"", "", "", "", "", "", "", ""};
String currentConfigId = ""; 
const char* SENSOR_NAMES[] = {"Sensor_1", "Sensor_2", "Sensor_3", "Sensor_4", "Sensor_5", "Sensor_6", "Sensor_7", "Sensor_8"};

// 1. Local Thermostat Structure
struct TempController {
  bool active = false;         
  int sensorIndex = -1;        
  float targetTemp = 25.0;     
  float hysteresis = 2.0;      
  bool modeHeating = true;     
};
TempController thermostats[NUM_RELAYS];

// 2. Looping Timer Structure
struct LoopTimer {
  bool active = false;
  uint32_t onTimeMs = 120000;  
  uint32_t offTimeMs = 600000; 
  unsigned long lastToggleTime = 0; 
  bool currentPhaseOn = false; 
};
LoopTimer timers[NUM_RELAYS];

// 3. Time-of-Day Schedule Structure
struct DailySchedule {
  bool active = false;
  uint8_t startHour = 0;   
  uint8_t startMin = 0;    
  uint8_t endHour = 0;     
  uint8_t endMin = 0;      
  
  String mode = "RELAY_ON"; 
  
  float targetTemp = 25.0;
  float hysteresis = 2.0;
  bool modeHeating = true;
  uint32_t onTimeMs = 120000;
  uint32_t offTimeMs = 600000;
  
  bool isExecuting = false; 
};
DailySchedule schedules[NUM_RELAYS];

// Timers
const unsigned long STATUS_UPDATE_INTERVAL = 2000;
const unsigned long TEMP_UPDATE_INTERVAL = 5000;
const unsigned long SCHEDULE_CHECK_INTERVAL = 10000; 

// ============== GLOBAL VARIABLES ==============
HyperwisorIOT device;
OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature sensors(&oneWire);
Preferences nvsStorage; 
uint8_t sensorCount = 0;

unsigned long lastStatusUpdate = 0;
unsigned long lastTempRequest = 0;
unsigned long lastTimerCheck = 0; 
unsigned long lastScheduleCheck = 0; 
bool tempConversionPending = false; 

bool relayStates[NUM_RELAYS] = {false};
bool lastRelayStates[NUM_RELAYS] = {false};

// ============== NVS MEMORY FUNCTIONS ==============

void loadPersistentData() {
  Serial.println("Loading persistent settings from NVS...");
  nvsStorage.begin("iot_config", true); 
  currentConfigId = nvsStorage.getString("configId", "");
  
  for (int i = 0; i < NUM_RELAYS; i++) {
    RELAY_WIDGET_IDS[i] = nvsStorage.getString(("rw_" + String(i)).c_str(), "");
    TEMP_WIDGET_IDS[i] = nvsStorage.getString(("tw_" + String(i)).c_str(), "");
    
    thermostats[i].active = nvsStorage.getBool(("tcA_" + String(i)).c_str(), false);
    thermostats[i].sensorIndex = nvsStorage.getInt(("tcS_" + String(i)).c_str(), -1);
    thermostats[i].targetTemp = nvsStorage.getFloat(("tcT_" + String(i)).c_str(), 25.0);
    thermostats[i].hysteresis = nvsStorage.getFloat(("tcH_" + String(i)).c_str(), 1.0);
    thermostats[i].modeHeating = nvsStorage.getBool(("tcM_" + String(i)).c_str(), true);

    timers[i].active = nvsStorage.getBool(("tmA_" + String(i)).c_str(), false);
    timers[i].onTimeMs = nvsStorage.getUInt(("tmOn_" + String(i)).c_str(), 120000);
    timers[i].offTimeMs = nvsStorage.getUInt(("tmOff_" + String(i)).c_str(), 600000);

    schedules[i].active = nvsStorage.getBool(("scA_" + String(i)).c_str(), false);
    schedules[i].startHour = nvsStorage.getUChar(("scSH_" + String(i)).c_str(), 0);
    schedules[i].startMin = nvsStorage.getUChar(("scSM_" + String(i)).c_str(), 0);
    schedules[i].endHour = nvsStorage.getUChar(("scEH_" + String(i)).c_str(), 0);
    schedules[i].endMin = nvsStorage.getUChar(("scEM_" + String(i)).c_str(), 0);
    schedules[i].mode = nvsStorage.getString(("scMd_" + String(i)).c_str(), "RELAY_ON");
    schedules[i].targetTemp = nvsStorage.getFloat(("scTT_" + String(i)).c_str(), 25.0);
    schedules[i].hysteresis = nvsStorage.getFloat(("scTH_" + String(i)).c_str(), 2.0);
    schedules[i].modeHeating = nvsStorage.getBool(("scMH_" + String(i)).c_str(), true);
    schedules[i].onTimeMs = nvsStorage.getUInt(("scTO_" + String(i)).c_str(), 120000);
    schedules[i].offTimeMs = nvsStorage.getUInt(("scTF_" + String(i)).c_str(), 600000);
  }
  nvsStorage.end();
  Serial.println("NVS Load Complete.\n");
}

void saveRelayWidgetId(int index, String newId) {
  if (RELAY_WIDGET_IDS[index] != newId) {
    RELAY_WIDGET_IDS[index] = newId;
    nvsStorage.begin("iot_config", false); 
    nvsStorage.putString(("rw_" + String(index)).c_str(), newId);
    nvsStorage.end();
  }
}

void saveTempWidgetId(int index, String newId) {
  if (TEMP_WIDGET_IDS[index] != newId) {
    TEMP_WIDGET_IDS[index] = newId;
    nvsStorage.begin("iot_config", false);
    nvsStorage.putString(("tw_" + String(index)).c_str(), newId);
    nvsStorage.end();
  }
}

void saveConfigId(String newId) {
  if (currentConfigId != newId) {
    currentConfigId = newId;
    nvsStorage.begin("iot_config", false);
    nvsStorage.putString("configId", newId);
    nvsStorage.end();
  }
}

// ============== CORE FUNCTIONS ==============

void initRelays() {
  for (int i = 0; i < NUM_RELAYS; i++) {
    pinMode(RELAY_PINS[i], OUTPUT);
    digitalWrite(RELAY_PINS[i], ACTIVE_LOW_RELAYS ? HIGH : LOW);
  }
}

void initSensors() {
  sensors.begin();
  sensorCount = sensors.getDeviceCount();
  if (sensorCount > 0) {
    for (uint8_t i = 0; i < sensorCount; i++) {
      DeviceAddress addr;
      if (sensors.getAddress(addr, i)) sensors.setResolution(addr, 12);
    }
    sensors.setWaitForConversion(false); 
  }
}

void setRelay(int index, bool state) {
  if (index < 0 || index >= NUM_RELAYS) return;
  relayStates[index] = state;
  if (ACTIVE_LOW_RELAYS) { digitalWrite(RELAY_PINS[index], state ? LOW : HIGH); } 
  else { digitalWrite(RELAY_PINS[index], state ? HIGH : LOW); }
}

void updateRelayWidgets() {
  for (int i = 0; i < NUM_RELAYS; i++) {
    if (relayStates[i] != lastRelayStates[i]) {
      if (RELAY_WIDGET_IDS[i] != "") {
        device.updateWidget(TARGET_ID, RELAY_WIDGET_IDS[i], relayStates[i] ? true : false);
      }
      lastRelayStates[i] = relayStates[i];
    }
  }
}

void fetchAndLogTemperatures(String logTarget, String configId) {
  if (sensorCount == 0 || configId == "") return;
  struct { const char* name; float value; } sensorReadings[8];
  uint8_t validCount = 0;

  for (uint8_t i = 0; i < sensorCount && i < 8; i++) {
    float tempC = sensors.getTempCByIndex(i);
    if (tempC != -127.0) {
      if (TEMP_WIDGET_IDS[i] != "") { device.updateWidget(TARGET_ID, TEMP_WIDGET_IDS[i], tempC); }
      sensorReadings[validCount++] = { SENSOR_NAMES[i], tempC };
    }
  }

  if (validCount > 0 && logTarget != "" && logTarget != TARGET_ID) {
    switch(validCount) {
      case 1: device.send_Sensor_Data_logger(logTarget, configId, {{sensorReadings[0].name, sensorReadings[0].value}}); break;
      case 2: device.send_Sensor_Data_logger(logTarget, configId, {{sensorReadings[0].name, sensorReadings[0].value}, {sensorReadings[1].name, sensorReadings[1].value}}); break;
      case 3: device.send_Sensor_Data_logger(logTarget, configId, {{sensorReadings[0].name, sensorReadings[0].value}, {sensorReadings[1].name, sensorReadings[1].value}, {sensorReadings[2].name, sensorReadings[2].value}}); break;
      case 4: device.send_Sensor_Data_logger(logTarget, configId, {{sensorReadings[0].name, sensorReadings[0].value}, {sensorReadings[1].name, sensorReadings[1].value}, {sensorReadings[2].name, sensorReadings[2].value}, {sensorReadings[3].name, sensorReadings[3].value}}); break;
      case 5: device.send_Sensor_Data_logger(logTarget, configId, {{sensorReadings[0].name, sensorReadings[0].value}, {sensorReadings[1].name, sensorReadings[1].value}, {sensorReadings[2].name, sensorReadings[2].value}, {sensorReadings[3].name, sensorReadings[3].value}, {sensorReadings[4].name, sensorReadings[4].value}}); break;
      case 6: device.send_Sensor_Data_logger(logTarget, configId, {{sensorReadings[0].name, sensorReadings[0].value}, {sensorReadings[1].name, sensorReadings[1].value}, {sensorReadings[2].name, sensorReadings[2].value}, {sensorReadings[3].name, sensorReadings[3].value}, {sensorReadings[4].name, sensorReadings[4].value}, {sensorReadings[5].name, sensorReadings[5].value}}); break;
      case 7: device.send_Sensor_Data_logger(logTarget, configId, {{sensorReadings[0].name, sensorReadings[0].value}, {sensorReadings[1].name, sensorReadings[1].value}, {sensorReadings[2].name, sensorReadings[2].value}, {sensorReadings[3].name, sensorReadings[3].value}, {sensorReadings[4].name, sensorReadings[4].value}, {sensorReadings[5].name, sensorReadings[5].value}, {sensorReadings[6].name, sensorReadings[6].value}}); break;
      case 8: device.send_Sensor_Data_logger(logTarget, configId, {{sensorReadings[0].name, sensorReadings[0].value}, {sensorReadings[1].name, sensorReadings[1].value}, {sensorReadings[2].name, sensorReadings[2].value}, {sensorReadings[3].name, sensorReadings[3].value}, {sensorReadings[4].name, sensorReadings[4].value}, {sensorReadings[5].name, sensorReadings[5].value}, {sensorReadings[6].name, sensorReadings[6].value}, {sensorReadings[7].name, sensorReadings[7].value}}); break;
    }
  }
}

// ============== HEAVY DEBUGGING ENGINES ==============

void evaluateThermostats() {
  for (int i = 0; i < NUM_RELAYS; i++) {
    if (!thermostats[i].active) continue;

    int sIdx = thermostats[i].sensorIndex;
    
    Serial.println("\n[Thermostat] Checking Relay " + String(i) + "...");
    Serial.println("  -> Assigned to Sensor Index: " + String(sIdx));

    if (sIdx < 0 || sIdx >= sensorCount) {
      Serial.println("  -> ERROR: Assigned sensor does not exist! (Only " + String(sensorCount) + " sensors detected)");
      continue; 
    }

    float currentTemp = sensors.getTempCByIndex(sIdx);
    
    if (currentTemp == -127.0) {
      Serial.println("  -> ERROR: Sensor reading -127.0 (Disconnected). Forcing relay OFF.");
      if (relayStates[i]) setRelay(i, false); 
      continue; 
    }

    float target = thermostats[i].targetTemp;
    float hyst = thermostats[i].hysteresis;
    bool isHeating = thermostats[i].modeHeating;
    bool currentState = relayStates[i];

    Serial.print("  -> Temp: "); Serial.print(currentTemp);
    Serial.print("C | Target: "); Serial.print(target);
    Serial.print("C | Hysteresis: "); Serial.print(hyst);
    Serial.print(" | Mode: "); Serial.print(isHeating ? "HEATING" : "COOLING");
    Serial.print(" | Current State: "); Serial.println(currentState ? "ON" : "OFF");

    if (isHeating) {
      if (currentTemp <= (target - hyst) && !currentState) {
        Serial.println("  -> ACTION: Too cold! Turning Relay ON.");
        setRelay(i, true);
      }
      else if (currentTemp >= (target + hyst) && currentState) {
        Serial.println("  -> ACTION: Warm enough! Turning Relay OFF.");
        setRelay(i, false);
      } else {
        Serial.println("  -> ACTION: Temperature is in the safe zone. No changes.");
      }
    } else { 
      if (currentTemp >= (target + hyst) && !currentState) {
        Serial.println("  -> ACTION: Too hot! Turning Relay ON.");
        setRelay(i, true);
      }
      else if (currentTemp <= (target - hyst) && currentState) {
        Serial.println("  -> ACTION: Cool enough! Turning Relay OFF.");
        setRelay(i, false);
      } else {
        Serial.println("  -> ACTION: Temperature is in the safe zone. No changes.");
      }
    }
  }
}

void evaluateTimers() {
  unsigned long currentMillis = millis();
  
  for (int i = 0; i < NUM_RELAYS; i++) {
    if (!timers[i].active) continue;

    Serial.println("\n[Timer] Checking Relay " + String(i) + "...");

    if (timers[i].lastToggleTime == 0) {
      timers[i].lastToggleTime = currentMillis;
      timers[i].currentPhaseOn = true;
      setRelay(i, true);
      Serial.println("  -> ACTION: Timer Initialized. Starting ON phase.");
      continue;
    }

    unsigned long elapsed = currentMillis - timers[i].lastToggleTime;

    if (timers[i].currentPhaseOn) {
      unsigned long remaining = (timers[i].onTimeMs > elapsed) ? (timers[i].onTimeMs - elapsed) : 0;
      Serial.print("  -> Phase: ON | Elapsed: "); Serial.print(elapsed / 1000);
      Serial.print("s | Target: "); Serial.print(timers[i].onTimeMs / 1000);
      Serial.print("s | Remaining: "); Serial.print(remaining / 1000); Serial.println("s");

      if (elapsed >= timers[i].onTimeMs) {
        timers[i].currentPhaseOn = false;
        timers[i].lastToggleTime = currentMillis;
        setRelay(i, false);
        Serial.println("  -> ACTION: ON time finished. Switching Relay OFF.");
      } else {
        Serial.println("  -> ACTION: Waiting for ON phase to complete.");
      }
    } else {
      unsigned long remaining = (timers[i].offTimeMs > elapsed) ? (timers[i].offTimeMs - elapsed) : 0;
      Serial.print("  -> Phase: OFF | Elapsed: "); Serial.print(elapsed / 1000);
      Serial.print("s | Target: "); Serial.print(timers[i].offTimeMs / 1000);
      Serial.print("s | Remaining: "); Serial.print(remaining / 1000); Serial.println("s");

      if (elapsed >= timers[i].offTimeMs) {
        timers[i].currentPhaseOn = true;
        timers[i].lastToggleTime = currentMillis;
        setRelay(i, true);
        Serial.println("  -> ACTION: OFF time finished. Switching Relay ON.");
      } else {
        Serial.println("  -> ACTION: Waiting for OFF phase to complete.");
      }
    }
  }
}


// Evaluates Daily Time Schedules
void evaluateSchedules() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("\n[Schedule] NTP Sync pending... Waiting for Network Time.");
    return; 
  }

  // THE FIX: Keeps track of whether we've done our initial boot-up enforcement
  static bool bootSyncComplete = false; 

  int currentMins = (timeinfo.tm_hour * 60) + timeinfo.tm_min;
  
  String hh = (timeinfo.tm_hour < 10 ? "0" : "") + String(timeinfo.tm_hour);
  String mm = (timeinfo.tm_min < 10 ? "0" : "") + String(timeinfo.tm_min);
  Serial.println("\n[Schedule] Checking Clock... Current Time is " + hh + ":" + mm);

  for (int i = 0; i < NUM_RELAYS; i++) {
    if (!schedules[i].active) continue;

    int startMins = (schedules[i].startHour * 60) + schedules[i].startMin;
    int endMins = (schedules[i].endHour * 60) + schedules[i].endMin;
    
    // Format schedule times for debug
    String sHH = (schedules[i].startHour < 10 ? "0" : "") + String(schedules[i].startHour);
    String sMM = (schedules[i].startMin < 10 ? "0" : "") + String(schedules[i].startMin);
    String eHH = (schedules[i].endHour < 10 ? "0" : "") + String(schedules[i].endHour);
    String eMM = (schedules[i].endMin < 10 ? "0" : "") + String(schedules[i].endMin);

    Serial.println("  -> Relay " + String(i) + " is scheduled from " + sHH + ":" + sMM + " to " + eHH + ":" + eMM + " (Mode: " + schedules[i].mode + ")");

    bool inWindow = false;
    if (startMins <= endMins) {
      inWindow = (currentMins >= startMins && currentMins < endMins);
    } else {
      inWindow = (currentMins >= startMins || currentMins < endMins); 
    }

    // TRIGGER IN: If we are in the window AND (we aren't executing yet OR we just booted)
    if (inWindow && (!schedules[i].isExecuting || !bootSyncComplete)) {
      Serial.println("  -> ACTION: ⏰ Time window active! Injecting Schedule logic.");
      schedules[i].isExecuting = true;
      thermostats[i].active = false;
      timers[i].active = false;

      if (schedules[i].mode == "RELAY_ON") {
        setRelay(i, true);
        Serial.println("  -> Result: Forced Relay ON.");
      } else if (schedules[i].mode == "RELAY_OFF") {
        setRelay(i, false);
        Serial.println("  -> Result: Forced Relay OFF.");
      } else if (schedules[i].mode == "THERMOSTAT") {
        thermostats[i].targetTemp = schedules[i].targetTemp;
        thermostats[i].hysteresis = schedules[i].hysteresis;
        thermostats[i].modeHeating = schedules[i].modeHeating;
        thermostats[i].active = true;
        Serial.println("  -> Result: Overridden Thermostat settings & activated.");
      } else if (schedules[i].mode == "TIMER") {
        timers[i].onTimeMs = schedules[i].onTimeMs;
        timers[i].offTimeMs = schedules[i].offTimeMs;
        timers[i].lastToggleTime = 0; 
        timers[i].active = true;
        Serial.println("  -> Result: Overridden Timer settings & activated.");
      }
    }
    
    // TRIGGER OUT: If we are OUTSIDE the window AND (we were executing OR we just booted)
    if (!inWindow && (schedules[i].isExecuting || !bootSyncComplete)) {
      Serial.println("  -> ACTION: 🛑 Outside scheduled window! Forcing automations OFF.");
      schedules[i].isExecuting = false;
      
      // Forcefully kill any ghost states that loaded from NVS
      thermostats[i].active = false;
      timers[i].active = false;
      setRelay(i, false);
    }
  }

  // Mark the boot sync as completely finished so it goes back to normal monitoring
  bootSyncComplete = true; 
}

// ============== CLOUD COMMAND HANDLER ==============
void handleCloudCommand(JsonObject& msg) {
  String from = msg["from"].as<String>();
  if (!msg.containsKey("payload")) return;
  JsonObject payload = msg["payload"];

  // --- 1. HANDLE RELAY CONTROL ---
  JsonObject relayCmd = device.findCommand(payload, "RELAY_CONTROL");
  if (!relayCmd.isNull()) {
    for (JsonObject action : relayCmd["actions"].as<JsonArray>()) {
      JsonObject params = action["params"];
      int channel = -1;
      if (params.containsKey("channel")) channel = params["channel"].as<int>();
      else if (params.containsKey("relay")) channel = params["relay"].as<int>();
      else if (params.containsKey("relay_index")) channel = params["relay_index"].as<int>();
      
      if (channel >= 0 && channel < NUM_RELAYS) {
        String stateStr = params.containsKey("state") ? params["state"].as<String>() : action["action"].as<String>();
        bool newState = (stateStr.indexOf("ON") >= 0 || stateStr == "1" || stateStr == "true" || stateStr == "HIGH");
        if (params.containsKey("widgetId")) saveRelayWidgetId(channel, params["widgetId"].as<String>());
        
        // Safety: Manual button presses disable all automations instantly
        thermostats[channel].active = false;
        timers[channel].active = false;
        schedules[channel].active = false;
        
        setRelay(channel, newState); 
        if (RELAY_WIDGET_IDS[channel] != "") { device.updateWidget(TARGET_ID, RELAY_WIDGET_IDS[channel], newState ? true : false); }
        device.sendTo(from, [channel, newState](JsonObject& response) {
          response["status"] = "success";
          response["channel"] = channel;
          response["state"] = newState ? true : false;
        });
      }
    }
  }

  // --- 2. HANDLE GET SENSOR VALUE ---
  JsonObject sensorCmd = device.findCommand(payload, "GET_SENSOR_VALUE");
  if (!sensorCmd.isNull()) {
    for (JsonObject actionObj : sensorCmd["actions"].as<JsonArray>()) {
      if (actionObj["action"].as<String>() == "ALL") {
        if (payload.containsKey("configId")) {
          saveConfigId(payload["configId"].as<String>());
          DATA_LOGGER_ID = from; 
          fetchAndLogTemperatures(DATA_LOGGER_ID, currentConfigId); 
        }
      }
    }
  }

  // --- 3. HANDLE SET THERMOSTAT ---
  JsonObject thermoCmd = device.findCommand(payload, "SET_THERMOSTAT");
  if (!thermoCmd.isNull()) {
    for (JsonObject action : thermoCmd["actions"].as<JsonArray>()) {
      JsonObject params = action["params"];
      if (params.containsKey("relay")) {
        int relay = params["relay"].as<int>();
        if (relay >= 0 && relay < NUM_RELAYS) {
          if (params.containsKey("active")) thermostats[relay].active = params["active"].as<bool>();
          if (params.containsKey("sensorIndex")) thermostats[relay].sensorIndex = params["sensorIndex"].as<int>();
          if (params.containsKey("targetTemp")) thermostats[relay].targetTemp = params["targetTemp"].as<float>();
          if (params.containsKey("hysteresis")) thermostats[relay].hysteresis = params["hysteresis"].as<float>();
          if (params.containsKey("mode")) thermostats[relay].modeHeating = (params["mode"].as<String>() == "HEATING");
          
          if (thermostats[relay].active) timers[relay].active = false;

          nvsStorage.begin("iot_config", false);
          nvsStorage.putBool(("tcA_" + String(relay)).c_str(), thermostats[relay].active);
          nvsStorage.putInt(("tcS_" + String(relay)).c_str(), thermostats[relay].sensorIndex);
          nvsStorage.putFloat(("tcT_" + String(relay)).c_str(), thermostats[relay].targetTemp);
          nvsStorage.putFloat(("tcH_" + String(relay)).c_str(), thermostats[relay].hysteresis);
          nvsStorage.putBool(("tcM_" + String(relay)).c_str(), thermostats[relay].modeHeating);
          nvsStorage.end();
          Serial.println("Saved Thermostat for Relay " + String(relay));
        }
      }
    }
  }

  // --- 4. HANDLE SET TIMER LOOP ---
  JsonObject loopCmd = device.findCommand(payload, "SET_TIMER");
  if (!loopCmd.isNull()) {
    for (JsonObject action : loopCmd["actions"].as<JsonArray>()) {
      JsonObject params = action["params"];
      if (params.containsKey("relay")) {
        int relay = params["relay"].as<int>();
        if (relay >= 0 && relay < NUM_RELAYS) {
          if (params.containsKey("active")) timers[relay].active = params["active"].as<bool>();
          if (params.containsKey("onMinutes")) timers[relay].onTimeMs = (uint32_t)(params["onMinutes"].as<float>() * 60000);
          if (params.containsKey("offMinutes")) timers[relay].offTimeMs = (uint32_t)(params["offMinutes"].as<float>() * 60000);

          timers[relay].lastToggleTime = 0; 
          if (timers[relay].active) thermostats[relay].active = false;

          nvsStorage.begin("iot_config", false);
          nvsStorage.putBool(("tmA_" + String(relay)).c_str(), timers[relay].active);
          nvsStorage.putUInt(("tmOn_" + String(relay)).c_str(), timers[relay].onTimeMs);
          nvsStorage.putUInt(("tmOff_" + String(relay)).c_str(), timers[relay].offTimeMs);
          nvsStorage.end();
          Serial.println("Saved Timer for Relay " + String(relay));
        }
      }
    }
  }

  // --- 5. HANDLE TIME SCHEDULES ---
  JsonObject scheduleCmd = device.findCommand(payload, "SET_SCHEDULE");
  if (!scheduleCmd.isNull()) {
    for (JsonObject action : scheduleCmd["actions"].as<JsonArray>()) {
      JsonObject params = action["params"];
      if (params.containsKey("relay")) {
        int relay = params["relay"].as<int>();
        if (relay >= 0 && relay < NUM_RELAYS) {
          if (params.containsKey("active")) schedules[relay].active = params["active"].as<bool>();
          if (params.containsKey("startHour")) schedules[relay].startHour = params["startHour"].as<int>();
          if (params.containsKey("startMin")) schedules[relay].startMin = params["startMin"].as<int>();
          if (params.containsKey("endHour")) schedules[relay].endHour = params["endHour"].as<int>();
          if (params.containsKey("endMin")) schedules[relay].endMin = params["endMin"].as<int>();
          if (params.containsKey("mode")) schedules[relay].mode = params["mode"].as<String>();
          
          if (params.containsKey("targetTemp")) schedules[relay].targetTemp = params["targetTemp"].as<float>();
          if (params.containsKey("hysteresis")) schedules[relay].hysteresis = params["hysteresis"].as<float>();
          if (params.containsKey("modeHeating")) schedules[relay].modeHeating = params["modeHeating"].as<bool>();
          if (params.containsKey("onMinutes")) schedules[relay].onTimeMs = (uint32_t)(params["onMinutes"].as<float>() * 60000);
          if (params.containsKey("offMinutes")) schedules[relay].offTimeMs = (uint32_t)(params["offMinutes"].as<float>() * 60000);

          schedules[relay].isExecuting = false;

          nvsStorage.begin("iot_config", false);
          nvsStorage.putBool(("scA_" + String(relay)).c_str(), schedules[relay].active);
          nvsStorage.putUChar(("scSH_" + String(relay)).c_str(), schedules[relay].startHour);
          nvsStorage.putUChar(("scSM_" + String(relay)).c_str(), schedules[relay].startMin);
          nvsStorage.putUChar(("scEH_" + String(relay)).c_str(), schedules[relay].endHour);
          nvsStorage.putUChar(("scEM_" + String(relay)).c_str(), schedules[relay].endMin);
          nvsStorage.putString(("scMd_" + String(relay)).c_str(), schedules[relay].mode);
          nvsStorage.putFloat(("scTT_" + String(relay)).c_str(), schedules[relay].targetTemp);
          nvsStorage.putFloat(("scTH_" + String(relay)).c_str(), schedules[relay].hysteresis);
          nvsStorage.putBool(("scMH_" + String(relay)).c_str(), schedules[relay].modeHeating);
          nvsStorage.putUInt(("scTO_" + String(relay)).c_str(), schedules[relay].onTimeMs);
          nvsStorage.putUInt(("scTF_" + String(relay)).c_str(), schedules[relay].offTimeMs);
          nvsStorage.end();
          
          Serial.println("Updated & Saved Daily Schedule for Relay " + String(relay));
        }
      }
    }
  }

  // --- 6. HANDLE GET UI ---
  if (payload.containsKey("command") && payload["command"].as<String>() == "get_ui") {
    for (int i = 0; i < NUM_RELAYS; i++) {
      if (RELAY_WIDGET_IDS[i] != "") device.updateWidget(TARGET_ID, RELAY_WIDGET_IDS[i], relayStates[i] ? true : false);
    }
  }

  // --- 7. SETTINGS REPORT ---
  JsonObject getSettingsCmd = device.findCommand(payload, "GET_SETTINGS");
  if (!getSettingsCmd.isNull()) {
    device.sendTo(from, [](JsonObject& response) {
      response["status"] = "success";
      response["type"] = "SETTINGS_REPORT";
      JsonArray rules = response.createNestedArray("rules");

      for (int i = 0; i < NUM_RELAYS; i++) {
        JsonObject relayObj = rules.createNestedObject();
        relayObj["relay"] = i;
        
        JsonObject thermo = relayObj.createNestedObject("thermostat");
        thermo["active"] = thermostats[i].active;
        thermo["sensorIndex"] = thermostats[i].sensorIndex;
        thermo["targetTemp"] = thermostats[i].targetTemp;
        thermo["hysteresis"] = thermostats[i].hysteresis;
        thermo["mode"] = thermostats[i].modeHeating ? "HEATING" : "COOLING";

        JsonObject timer = relayObj.createNestedObject("timer");
        timer["active"] = timers[i].active;
        timer["onMinutes"] = timers[i].onTimeMs / 60000.0;
        timer["offMinutes"] = timers[i].offTimeMs / 60000.0;
        
        JsonObject sch = relayObj.createNestedObject("schedule");
        sch["active"] = schedules[i].active;
        sch["start"] = (schedules[i].startHour < 10 ? "0" : "") + String(schedules[i].startHour) + ":" + (schedules[i].startMin < 10 ? "0" : "") + String(schedules[i].startMin);
        sch["end"] = (schedules[i].endHour < 10 ? "0" : "") + String(schedules[i].endHour) + ":" + (schedules[i].endMin < 10 ? "0" : "") + String(schedules[i].endMin);
        sch["mode"] = schedules[i].mode;
        
      }
    });
  }
}

// ============== SETUP & LOOP ==============

void setup() {
  Serial.begin(115200);
  Serial.println("\n========================================");
  Serial.println("Universal IoT Controller - Master Engine");
  Serial.println("========================================\n");

  loadPersistentData();
  initRelays();
  initSensors();
  
  device.setUserCommandHandler(handleCloudCommand);
  device.setTimezone("IST");
  device.begin();
  TARGET_ID = device.getUserId();
}

void loop() {
  device.loop(); 
  updateRelayWidgets(); 
  unsigned long currentMillis = millis();

  // 1. Evaluate Timers (Every 1 second)
  if (currentMillis - lastTimerCheck >= 1000) {
    evaluateTimers();
    lastTimerCheck = currentMillis;
  }

  // 2. Evaluate Daily Schedules (Every 10 seconds)
  if (currentMillis - lastScheduleCheck >= SCHEDULE_CHECK_INTERVAL) {
    evaluateSchedules();
    lastScheduleCheck = currentMillis;
  }

  // 3. Sensor Check & Hardware Recovery (Every 5 seconds)
  if (currentMillis - lastTempRequest >= TEMP_UPDATE_INTERVAL) {
    if (sensorCount == 0) {
      Serial.println("\n[Hardware] ⚠️ 0 Sensors detected! Attempting to reboot 1-Wire bus...");
      initSensors(); 
    }
    if (sensorCount > 0) {
      sensors.requestTemperatures(); 
      tempConversionPending = true;
    }
    lastTempRequest = currentMillis;
  }

  // 4. Thermostat Logic (Wait 800ms for Temp reading)
  if (tempConversionPending && (currentMillis - lastTempRequest >= 800)) {
    evaluateThermostats();
    if (currentConfigId != "") { 
      String target = (DATA_LOGGER_ID != "") ? DATA_LOGGER_ID : TARGET_ID;
      fetchAndLogTemperatures(target, currentConfigId);
    }
    tempConversionPending = false;
  }

  if (currentMillis - lastStatusUpdate >= STATUS_UPDATE_INTERVAL) {
    lastStatusUpdate = currentMillis;
  }
}