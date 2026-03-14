/**
 * AttitudeWidget_MPU6050 Example
 * 
 * This example demonstrates how to use the MPU6050 Accelerometer/Gyroscope
 * to drive a Flight Attitude Widget on the Hyperwisor dashboard.
 * 
 * Dependencies:
 * - Adafruit MPU6050 Library
 * - Adafruit Sensor Library
 * - Hyperwisor-IOT Library
 */

#include <hyperwisor-iot.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>

HyperwisorIOT device;
Adafruit_MPU6050 mpu;

// Define your custom pins for I2C
#define I2C_SDA 4
#define I2C_SCL 5

// Replace with your actual Widget ID and Target ID from Hyperwisor Dashboard
const String targetId = "YOUR_TARGET_ID"; 
const String widgetId = "YOUR_ATTITUDE_WIDGET_ID";

unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 100; // Update frequency in milliseconds (10Hz)

void setup(void) {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n=== Hyperwisor-IOT MPU6050 Attitude Widget ===\n");

  // Initialize Hyperwisor-IOT
  device.begin();

  // Initialize I2C with your custom pins (SDA, SCL)
  Wire.begin(I2C_SDA, I2C_SCL);

  // Initialize MPU6050
  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }
  Serial.println("MPU6050 Found!");

  // Set sensor ranges
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  delay(100);
}

void loop() {
  // Keep the Hyperwisor connection alive and handle incoming commands
  device.loop();

  // Send updates periodically
  unsigned long currentMillis = millis();
  if (currentMillis - lastUpdateTime >= updateInterval) {
    lastUpdateTime = currentMillis;

    // Only update if WiFi is connected to avoid blocking
    if (WiFi.status() == WL_CONNECTED) {
      /* Get new sensor events with the readings */
      sensors_event_t a, g, temp;
      mpu.getEvent(&a, &g, &temp);

      /**
       * Calculate Roll and Pitch from Accelerometer readings
       * 
       * Roll (rotation around X-axis)
       * Pitch (rotation around Y-axis)
       */
      float roll = atan2(a.acceleration.y, a.acceleration.z) * 180.0 / M_PI;
      float pitch = atan2(-a.acceleration.x, sqrt(a.acceleration.y * a.acceleration.y + a.acceleration.z * a.acceleration.z)) * 180.0 / M_PI;

      // Update the Attitude Widget on the Hyperwisor dashboard
      device.updateFlightAttitude(targetId, widgetId, roll, pitch);

      /* Debug output */
      Serial.print("Roll: "); Serial.print(roll);
      Serial.print(" | Pitch: "); Serial.println(pitch);
    }
  }
}
