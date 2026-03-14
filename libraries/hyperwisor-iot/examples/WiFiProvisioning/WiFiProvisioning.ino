/**
 * WiFiProvisioning Example
 * 
 * This example demonstrates the WiFi provisioning flow:
 * - Check if credentials exist
 * - Show provisioning status
 * - Handle the AP mode setup process
 * 
 * How it works:
 * 1. On first boot, device enters AP mode
 * 2. User connects to "NIKOLAINDUSTRY_Setup" WiFi
 * 3. User provides WiFi credentials via the provisioning app
 * 4. Device saves credentials and restarts
 * 5. Device connects to the configured WiFi
 */

#include <hyperwisor-iot.h>

HyperwisorIOT device;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== WiFi Provisioning Example ===\n");
  
  // Check current provisioning status
  if (device.hasCredentials()) {
    Serial.println("Device is already provisioned.");
    Serial.println("Credentials found in storage.");
    
    // Uncomment below to force re-provisioning
    // device.clearCredentials();
    // Serial.println("Credentials cleared. Restart to enter AP mode.");
  } else {
    Serial.println("Device is NOT provisioned.");
    Serial.println("Starting AP mode for configuration...");
    Serial.println("");
    Serial.println("To provision this device:");
    Serial.println("1. Connect to WiFi: NIKOLAINDUSTRY_Setup");
    Serial.println("2. Password: 0123456789");
    Serial.println("3. Use the Hyperwisor app to configure");
  }
  
  // Initialize device - enters AP mode if not provisioned
  device.begin();
  
  if (device.hasCredentials()) {
    Serial.println("\nConnected successfully!");
    Serial.println("Device ID: " + device.getDeviceId());
    Serial.println("User ID: " + device.getUserId());
  }
}

void loop() {
  device.loop();
}
