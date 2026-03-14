/**
 * CommandHandler Example
 * 
 * This example demonstrates how to receive and handle custom commands
 * from your dashboard or app.
 * 
 * The library automatically handles built-in commands like:
 * - GPIO_MANAGEMENT (pin control)
 * - OTA (firmware updates)
 * - DEVICE_STATUS
 * 
 * You can add your own custom command handler for application-specific logic.
 */

#include <hyperwisor-iot.h>

HyperwisorIOT device;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Command Handler Example ===\n");
  
  // Set up your custom command handler
  device.setUserCommandHandler([](JsonObject& msg) {
    // Get the sender ID
    String from = msg["from"].as<String>();
    Serial.println("Received message from: " + from);
    
    // Check if message has payload
    if (!msg.containsKey("payload")) {
      Serial.println("No payload in message");
      return;
    }
    
    JsonObject payload = msg["payload"];
    
    // Print the full payload for debugging
    Serial.print("Payload: ");
    serializeJson(payload, Serial);
    Serial.println();
    
    // Handle custom commands
    JsonArray commands = payload["commands"];
    for (JsonObject cmd : commands) {
      String command = cmd["command"].as<String>();
      
      if (command == "MY_CUSTOM_COMMAND") {
        Serial.println("Received MY_CUSTOM_COMMAND!");
        
        // Process actions within the command
        JsonArray actions = cmd["actions"];
        for (JsonObject action : actions) {
          String actionName = action["action"].as<String>();
          JsonObject params = action["params"];
          
          Serial.println("  Action: " + actionName);
          
          // Handle specific actions
          if (actionName == "SET_VALUE") {
            int value = params["value"] | 0;
            Serial.println("  Setting value to: " + String(value));
            // Your custom logic here
          }
        }
        
        // Send response back
        device.sendTo(from, [](JsonObject& response) {
          response["status"] = "success";
          response["message"] = "Command executed";
        });
      }
      else if (command == "GET_STATUS") {
        Serial.println("Received GET_STATUS command");
        
        // Send device status back
        device.sendTo(from, [](JsonObject& response) {
          response["status"] = "online";
          response["uptime"] = millis() / 1000;
          response["freeHeap"] = ESP.getFreeHeap();
        });
      }
    }
  });
  
  device.begin();
  
  Serial.println("Device initialized and listening for commands...");
  Serial.println("Device ID: " + device.getDeviceId());
}

void loop() {
  device.loop();
}
