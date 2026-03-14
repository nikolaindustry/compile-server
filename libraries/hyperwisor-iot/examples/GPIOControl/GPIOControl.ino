/**
 * GPIOControl Example
 * 
 * This example demonstrates remote GPIO control.
 * The library handles GPIO_MANAGEMENT commands automatically,
 * but this example shows how to:
 * - Save and restore GPIO states across reboots
 * - Add custom logic for GPIO changes
 * - Report GPIO status back to the dashboard
 */

#include <hyperwisor-iot.h>

HyperwisorIOT device;

// Define your GPIO pins
const int LED_PIN = 2;      // Built-in LED on most ESP32 boards
const int RELAY_PIN = 12;   // Example relay pin

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== GPIO Control Example ===\n");
  
  // Configure GPIO pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  
  // Restore saved GPIO states from last session
  device.restoreAllGPIOStates();
  Serial.println("Restored GPIO states from storage");
  
  // Set up command handler for custom GPIO logic
  device.setUserCommandHandler([](JsonObject& msg) {
    String from = msg["from"].as<String>();
    
    if (!msg.containsKey("payload")) return;
    JsonObject payload = msg["payload"];
    
    // Use helper function to find GPIO commands
    JsonObject gpioCmd = device.findCommand(payload, "GPIO_MANAGEMENT");
    if (!gpioCmd.isNull()) {
      // The library handles basic GPIO commands automatically
      // Add any custom logic here, like logging or notifications
      
      JsonArray actions = gpioCmd["actions"];
      for (JsonObject action : actions) {
        String actionName = action["action"].as<String>();
        int pin = action["params"]["gpio"] | 0;
        String status = action["params"]["status"] | "LOW";
        
        Serial.println("GPIO Command received:");
        Serial.println("  Pin: " + String(pin));
        Serial.println("  Action: " + actionName);
        Serial.println("  Status: " + status);
        
        // Save GPIO state for persistence across reboots
        int state = (status == "HIGH") ? HIGH : LOW;
        device.saveGPIOState(pin, state);
      }
      
      // Send confirmation back
      device.sendTo(from, [](JsonObject& response) {
        response["status"] = "success";
        response["message"] = "GPIO updated";
      });
    }
    
    // Handle custom GET_GPIO_STATUS command
    JsonObject statusCmd = device.findCommand(payload, "GET_GPIO_STATUS");
    if (!statusCmd.isNull()) {
      device.sendTo(from, [](JsonObject& response) {
        response["command"] = "GPIO_STATUS";
        response["led"] = digitalRead(LED_PIN);
        response["relay"] = digitalRead(RELAY_PIN);
      });
    }
  });
  
  device.begin();
  
  Serial.println("Device initialized!");
  Serial.println("GPIO pins ready for remote control");
  Serial.println("Device ID: " + device.getDeviceId());
}

void loop() {
  device.loop();
  
  // You can also control GPIOs locally
  // and save the state for persistence
  
  // Example: Blink LED every 2 seconds (optional, for testing)
  /*
  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 2000) {
    lastBlink = millis();
    int newState = !digitalRead(LED_PIN);
    digitalWrite(LED_PIN, newState);
    device.saveGPIOState(LED_PIN, newState);
  }
  */
}
