/**
 * Manual Provisioning Example
 * 
 * This example demonstrates how to manually set WiFi credentials and device
 * configuration directly in the setup() function, without relying on AP mode
 * provisioning.
 * 
 * This is useful for:
 * - Factory configuration of devices
 * - Pre-configured devices for specific deployments
 * - Testing and development
 * - Cases where AP provisioning is not desired
 */

#include <hyperwisor-iot.h>

HyperwisorIOT device;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Manual Provisioning Example ===\n");
  
  // Check if credentials already exist
  if (device.hasCredentials()) {
    Serial.println("Credentials already exist in storage.");
    Serial.println("If you want to reset them, uncomment device.clearCredentials()");
    // Uncomment the line below to clear existing credentials
    // device.clearCredentials();
  } else {
    Serial.println("No credentials found. Setting them manually...");
  }
  
  // Method 1: Set all credentials at once (recommended)
  device.setCredentials(
    "YourWiFiSSID",       // WiFi SSID
    "YourWiFiPassword",   // WiFi Password
    "device-001",         // Device ID (unique identifier for your device)
    "user-123"            // User ID (optional - can be empty string)
  );
  
  /* Method 2: Set credentials individually
  device.setWiFiCredentials("YourWiFiSSID", "YourWiFiPassword");
  device.setDeviceId("device-001");
  device.setUserId("user-123");
  */
  
  // Set API keys for backend communication (optional)
  device.setApiKeys("your-api-key", "your-secret-key");
  
  // Initialize the device - it will now use the manually set credentials
  device.begin();
  
  Serial.println("\nDevice initialized with manual credentials!");
  Serial.println("Device ID: " + device.getDeviceId());
  Serial.println("User ID: " + device.getUserId());
}

void loop() {
  device.loop();
  
  // Your application logic here
}
