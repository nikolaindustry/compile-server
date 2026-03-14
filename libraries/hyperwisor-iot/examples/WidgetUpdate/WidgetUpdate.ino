/**
 * WidgetUpdate Example
 * 
 * This example demonstrates how to send data to dashboard widgets.
 * You can update widgets with:
 * - String values
 * - Numeric values (float/int)
 * - Arrays of values (for charts/graphs)
 * 
 * Prerequisites:
 * - Device must be provisioned and connected
 * - You need a target ID (usually your app/dashboard ID)
 * - You need a widget ID from your dashboard
 */

#include <hyperwisor-iot.h>

HyperwisorIOT device;

// Replace with your actual target and widget IDs
String targetId = "your-dashboard-id";
String temperatureWidgetId = "widget-temperature";
String humidityWidgetId = "widget-humidity";
String chartWidgetId = "widget-chart";

unsigned long lastUpdate = 0;
const unsigned long updateInterval = 5000; // Update every 5 seconds

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Widget Update Example ===\n");
  
  device.begin();
  
  Serial.println("Device initialized!");
  Serial.println("Will send widget updates every 5 seconds...");
}

void loop() {
  device.loop();
  
  // Send updates at regular intervals
  if (millis() - lastUpdate >= updateInterval) {
    lastUpdate = millis();
    
    // Simulate sensor readings
    float temperature = 20.0 + random(0, 100) / 10.0;  // 20.0 - 30.0
    float humidity = 40.0 + random(0, 300) / 10.0;     // 40.0 - 70.0
    
    // Update widget with a float value
    device.updateWidget(targetId, temperatureWidgetId, temperature);
    Serial.println("Sent temperature: " + String(temperature));
    
    // Update widget with a string value
    device.updateWidget(targetId, humidityWidgetId, String(humidity) + "%");
    Serial.println("Sent humidity: " + String(humidity) + "%");
    
    // Update widget with an array (for charts/graphs)
    std::vector<float> chartData = {23.5, 24.0, 24.5, 25.0, 24.8, temperature};
    device.updateWidget(targetId, chartWidgetId, chartData);
    Serial.println("Sent chart data array");
    
    Serial.println("---");
  }
}
