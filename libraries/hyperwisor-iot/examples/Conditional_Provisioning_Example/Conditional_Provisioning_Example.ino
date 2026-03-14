/**
 * Conditional Provisioning Example
 * 
 * This example shows how to combine manual provisioning with automatic
 * AP mode provisioning. It checks if credentials exist:
 * - If they exist: use them
 * - If they don't exist: set them manually OR fall back to AP mode
 * 
 * This approach is useful for:
 * - Flexible deployment scenarios
 * - First-time setup with hardcoded defaults
 * - Graceful fallback to AP mode when needed
 */

#include <hyperwisor-iot.h>

HyperwisorIOT device;

// Define whether to use manual provisioning for first boot
#define USE_MANUAL_PROVISIONING true

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Conditional Provisioning Example ===\n");
  
  // Check if credentials are already saved in NVS
  if (!device.hasCredentials()) {
    Serial.println("No credentials found in storage.");
    
    #if USE_MANUAL_PROVISIONING
      Serial.println("Using manual provisioning for first-time setup...");
      
      // Set credentials manually on first boot
      device.setCredentials(
        "YourWiFiSSID",
        "YourWiFiPassword",
        "device-001",
        "user-123"
      );
      
      Serial.println("Manual credentials configured successfully!");
    #else
      Serial.println("Manual provisioning disabled. Device will enter AP mode.");
      // If manual provisioning is not used, the device will automatically
      // enter AP mode when device.begin() is called
    #endif
  } else {
    Serial.println("Found existing credentials. Using saved configuration.");
  }
  
  // Set API keys (if needed for your application)
  device.setApiKeys("your-api-key", "your-secret-key");
  
  // Initialize the device
  device.begin();
  
  Serial.println("\nDevice initialized!");
  Serial.println("Device ID: " + device.getDeviceId());
  Serial.println("User ID: " + device.getUserId());
}

void loop() {
  device.loop();
  
  // Your application logic here
}
