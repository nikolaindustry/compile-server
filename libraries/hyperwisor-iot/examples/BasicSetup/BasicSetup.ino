/**
 * BasicSetup Example
 * 
 * The simplest possible example to get started with Hyperwisor-IOT.
 * This example demonstrates the minimal code needed to:
 * - Initialize the device
 * - Connect to WiFi (via saved credentials or AP mode)
 * - Maintain real-time connection
 * 
 * First-time setup:
 * 1. Upload this sketch to your ESP32
 * 2. Connect to "NIKOLAINDUSTRY_Setup" WiFi (password: 0123456789)
 * 3. Configure your WiFi and device credentials
 * 4. Device will restart and connect automatically
 */

#include <hyperwisor-iot.h>

HyperwisorIOT device;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Hyperwisor-IOT Basic Setup ===\n");
  
  // Initialize the device
  // If no credentials are saved, it will start in AP mode for provisioning
  device.begin();
  
  Serial.println("Device initialized!");
  Serial.println("Device ID: " + device.getDeviceId());
}

void loop() {
  // Keep the connection alive and handle incoming messages
  device.loop();
}
