/**
 * ThreeDWidgetControl Example
 * 
 * This example demonstrates how to control multiple 3D models within a 3D Widget
 * on the Hyperwisor dashboard. It shows how to update position, rotation, 
 * scale, and material properties like color and metalness.
 */

#include <hyperwisor-iot.h>
#include <vector>

HyperwisorIOT device;

// Replace with your actual IDs from the Hyperwisor Dashboard
const String targetId = "YOUR_TARGET_ID";
const String widgetId = "YOUR_3D_WIDGET_ID";
const String model1Id = "model-1763012135979";
const String model2Id = "model-1763012225109";

unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 2000; // Update every 2 seconds
float rotationAngle = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n=== Hyperwisor-IOT 3D Widget Control ===\n");

  // Initialize the device
  device.begin();
}

void loop() {
  // Keep the connection alive
  device.loop();

  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdateTime >= updateInterval) {
    lastUpdateTime = currentMillis;

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Updating 3D models...");

      // Create a list of model updates
      std::vector<ThreeDModelUpdate> updates;

      // Configuration for Model 1
      ThreeDModelUpdate m1;
      m1.modelId = model1Id;
      m1.position[0] = 0;    m1.position[1] = 10;   m1.position[2] = 50;
      m1.rotation[0] = 0;    m1.rotation[1] = rotationAngle; m1.rotation[2] = 0;
      m1.scale[0] = 1.5;     m1.scale[1] = 1.0;     m1.scale[2] = 1.0;
      m1.color = "#00ff00";
      m1.metalness = 0.8;
      m1.roughness = 0.2;
      m1.opacity = 1.0;
      m1.wireframe = false;
      m1.visible = true;
      updates.push_back(m1);

      // Configuration for Model 2
      ThreeDModelUpdate m2;
      m2.modelId = model2Id;
      m2.position[0] = 0;    m2.position[1] = 1;    m2.position[2] = 0;
      m2.rotation[0] = 0;    m2.rotation[1] = rotationAngle * -1.0; m2.rotation[2] = 0;
      m2.scale[0] = 1.5;     m2.scale[1] = 1.5;     m2.scale[2] = 1.5;
      m2.color = "#ff0000"; // Red for contrast
      m2.metalness = 0.5;
      m2.roughness = 0.5;
      m2.opacity = 0.8;
      m2.wireframe = true;  // Show model 2 as wireframe
      m2.visible = true;
      updates.push_back(m2);

      // Send the combined update to the dashboard
      device.update3DWidget(targetId, widgetId, updates);

      // Increment rotation for next update
      rotationAngle += 0.5;
      if (rotationAngle > 6.28) rotationAngle = 0; // Reset after full circle (approx 2*PI)
    }
  }
}
