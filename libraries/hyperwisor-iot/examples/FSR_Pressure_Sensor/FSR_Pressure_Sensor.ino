/**
 * FSR Pressure Sensor Example
 * 
 * This example demonstrates how to send 7-point FSR matrix sensor data
 * to Hyperwisor IoT dashboard widgets.
 * 
 * Features:
 * - Heat map widget visualization
 * - Individual widget updates for each sensor (optional)
 * - Optional: Data logging for historical visualization
 */

#include <hyperwisor-iot.h>

HyperwisorIOT device;

// Define the pins for the 7 sensors
const int fsrPins[] = {32, 33, 34, 35, 36, 39, 4}; 
const int numSensors = 7;

int sensorReadings[numSensors];

// Replace with your actual target ID
String targetId = "your-dashboard-id";

// Heat map widget ID
String heatMapWidgetId = "widget_1760420016591";

// Grid configuration for 7 sensors arranged in a pattern
// Adjust these coordinates based on your physical sensor layout
struct SensorPosition {
  int x;
  int y;
};

// Example positions for 7 sensors (adjust based on your layout)
SensorPosition sensorPositions[] = {
  {100, 100},  // Sensor 1
  {150, 100},  // Sensor 2
  {200, 100},  // Sensor 3
  {100, 150},  // Sensor 4
  {150, 150},  // Sensor 5
  {200, 150},  // Sensor 6
  {150, 200}   // Sensor 7
};

unsigned long lastUpdate = 0;
const unsigned long updateInterval = 500; // Update every 500ms

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== FSR Pressure Sensor Example ===\n");
  
  // Initialize FSR pins
  for (int i = 0; i < numSensors; i++) {
    pinMode(fsrPins[i], INPUT);
  }
  Serial.println("FSR sensors initialized on pins: 32, 33, 34, 35, 36, 39, 4");
  
  // Initialize Hyperwisor IoT
  device.begin();
  
  Serial.println("Device initialized!");
  Serial.println("Sending pressure data to dashboard...");
}

void loop() {
  device.loop();
  
  // Read all sensors
  readAllSensors();
  
  // Send updates at regular intervals
  if (millis() - lastUpdate >= updateInterval) {
    lastUpdate = millis();
    
    // Method 1: Send as heat map (primary method)
    sendHeatMap();
    
    // Method 2: Update individual widgets (optional)
    // updateIndividualWidgets();
    
    // Method 3: Log data for historical visualization (optional)
    // logSensorData();
    
    // Print to Serial for debugging
    printSensorData();
  }
}

void readAllSensors() {
  for (int i = 0; i < numSensors; i++) {
    sensorReadings[i] = analogRead(fsrPins[i]);
  }
}

void sendHeatMap() {
  // Create heat map data points from sensor readings
  std::vector<HeatMapPoint> heatMapData;
  
  for (int i = 0; i < numSensors; i++) {
    HeatMapPoint point;
    point.x = sensorPositions[i].x;
    point.y = sensorPositions[i].y;
    point.value = sensorReadings[i];
    heatMapData.push_back(point);
  }
  
  // Send heat map to widget
  device.updateHeatMap(targetId, heatMapWidgetId, heatMapData);
  Serial.println("Sent heat map data");
}

void updateIndividualWidgets() {
  // Individual widget IDs (if you want to use this method)
  String sensorWidgetIds[] = {
    "pressure-sensor-1",
    "pressure-sensor-2", 
    "pressure-sensor-3",
    "pressure-sensor-4",
    "pressure-sensor-5",
    "pressure-sensor-6",
    "pressure-sensor-7"
  };
  
  for (int i = 0; i < numSensors; i++) {
    device.updateWidget(targetId, sensorWidgetIds[i], sensorReadings[i]);
  }
  Serial.println("Updated individual widgets");
}

// Optional: Add more sensors to create a denser heat map
void sendEnhancedHeatMap() {
  std::vector<HeatMapPoint> heatMapData;
  
  // Add the 7 actual sensor readings
  for (int i = 0; i < numSensors; i++) {
    HeatMapPoint point;
    point.x = sensorPositions[i].x;
    point.y = sensorPositions[i].y;
    point.value = sensorReadings[i];
    heatMapData.push_back(point);
  }
  
  // Optional: Add interpolated points between sensors for smoother visualization
  // Example: Add midpoint between sensor 1 and 2
  if (numSensors >= 2) {
    HeatMapPoint midPoint;
    midPoint.x = (sensorPositions[0].x + sensorPositions[1].x) / 2;
    midPoint.y = (sensorPositions[0].y + sensorPositions[1].y) / 2;
    midPoint.value = (sensorReadings[0] + sensorReadings[1]) / 2;
    heatMapData.push_back(midPoint);
  }
  
  device.updateHeatMap(targetId, heatMapWidgetId, heatMapData);
  Serial.println("Sent enhanced heat map data");
}

void printSensorData() {
  Serial.print("Pressure Data: ");
  for (int i = 0; i < numSensors; i++) {
    Serial.print("S");
    Serial.print(i + 1);
    Serial.print("=");
    Serial.print(sensorReadings[i]);
    if (i < numSensors - 1) {
      Serial.print(" | ");
    }
  }
  Serial.println();
}
