/**
 * SensorDataLogger Example
 * 
 * This example demonstrates how to send sensor data to the 
 * Hyperwisor platform for logging and visualization.
 * 
 * The send_Sensor_Data_logger function sends structured data
 * that can be stored and displayed in charts/graphs.
 */

#include <hyperwisor-iot.h>

HyperwisorIOT device;

// Replace with your actual IDs
String targetId = "your-dashboard-id";
String configId = "sensor-config-001";

unsigned long lastReading = 0;
const unsigned long readingInterval = 10000; // Log every 10 seconds

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Sensor Data Logger Example ===\n");
  
  device.begin();
  
  Serial.println("Device initialized!");
  Serial.println("Logging sensor data every 10 seconds...");
}

void loop() {
  device.loop();
  
  // Log sensor data at regular intervals
  if (millis() - lastReading >= readingInterval) {
    lastReading = millis();
    
    // Read your sensors (simulated here)
    float temperature = readTemperature();
    float humidity = readHumidity();
    float pressure = readPressure();
    
    // Send sensor data to the platform
    device.send_Sensor_Data_logger(
      targetId,
      configId,
      {
        {"temperature", temperature},
        {"humidity", humidity},
        {"pressure", pressure}
      }
    );
    
    Serial.println("Logged sensor data:");
    Serial.println("  Temperature: " + String(temperature) + " °C");
    Serial.println("  Humidity: " + String(humidity) + " %");
    Serial.println("  Pressure: " + String(pressure) + " hPa");
    Serial.println("---");
  }
}

// Simulated sensor functions - replace with your actual sensor code
float readTemperature() {
  return 22.0 + random(-50, 50) / 10.0;  // 17.0 - 27.0
}

float readHumidity() {
  return 55.0 + random(-150, 150) / 10.0;  // 40.0 - 70.0
}

float readPressure() {
  return 1013.25 + random(-100, 100) / 10.0;  // ~1003 - 1023 hPa
}
