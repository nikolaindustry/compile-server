#ifndef HYPERWISOR_IOT_H
#define HYPERWISOR_IOT_H

#include <Arduino.h>
#include <WiFi.h>
#include "nikolaindustry-realtime.h"
#include <WebServer.h>
#include <Preferences.h>
#include <Update.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <sys/time.h>

// Heat map data point structure
/**
 * @brief Structure representing a single point in a heat map visualization
 * 
 * Used with updateHeatMap() function to create visual representations
 * of sensor data distributions across a 2D area.
 */
struct HeatMapPoint {
  int x;        ///< X coordinate on the heat map (pixels)
  int y;        ///< Y coordinate on the heat map (pixels)
  int value;    ///< Intensity/value at this point (sensor reading)
};

// 3D model update structure
/**
 * @brief Structure for updating 3D model properties in a 3D widget
 * 
 * Contains all transform and material properties for 3D models
 * displayed in Hyperwisor's 3D viewer widgets.
 */
struct ThreeDModelUpdate {
  String modelId;         ///< Unique identifier for the model
  float position[3];      ///< X, Y, Z position coordinates
  float rotation[3];      ///< Rotation around X, Y, Z axes (degrees)
  float scale[3];         ///< Scale factors for X, Y, Z axes
  String color;           ///< Hex color code (e.g., "#FF0000")
  float metalness;        ///< Metallic appearance (0.0 = non-metallic, 1.0 = metallic)
  float roughness;        ///< Surface roughness (0.0 = smooth, 1.0 = rough)
  float opacity;          ///< Transparency (0.0 = invisible, 1.0 = opaque)
  bool wireframe;         ///< Wireframe display mode (true/false)
  bool visible;           ///< Visibility flag (true = show, false = hide)
};

typedef std::function<void(JsonObject &msg)> UserCommandCallback;

class HyperwisorIOT
{
public:

  /**
   * @brief Constructor for HyperwisorIOT class
   */
  HyperwisorIOT();

  /**
   * @brief Initialize the Hyperwisor IoT device
   * 
   * This function initializes WiFi connection, sets up the real-time
   * communication layer, and prepares the device for cloud communication.
   * Should be called once in the setup() function.
   */
  void begin();

  /**
   * @brief Main loop handler for Hyperwisor IoT device
   * 
   * This function must be called repeatedly in the loop() function.
   * It handles WiFi reconnection, processes incoming messages,
   * and maintains the real-time connection with Hyperwisor cloud.
   */
  void loop();

  /**
   * @brief Set API credentials for Hyperwisor cloud authentication
   * 
   * @param apiKey Your Hyperwisor API key from the dashboard
   * @param secretKey Your Hyperwisor secret key from the dashboard
   * 
   * @note Call this before begin() to authenticate your device
   */
  void setApiKeys(const String &apiKey, const String &secretKey);

  /**
   * @brief Set callback handler for custom user commands
   * 
   * @param cb Callback function that receives JsonObject with command data
   * 
   * Use this to handle custom commands sent from the Hyperwisor dashboard.
   * The callback will receive a JSON object containing the command details.
   * 
   * Example:
   * ```cpp
   * device.setUserCommandHandler([](JsonObject &cmd) {
   *   if (cmd.containsKey("action")) {
   *     String action = cmd["action"].as<String>();
   *     // Handle action
   *   }
   * });
   * ```
   */
  void setUserCommandHandler(UserCommandCallback cb);

  /**
   * @brief Send custom message to a target device/dashboard
   * 
   * @param targetId Target device ID or dashboard ID to send message to
   * @param payloadBuilder Function to build the JSON payload
   * 
   * This is a low-level function for sending custom JSON messages.
   * Most widget updates have dedicated helper functions.
   * 
   * Example:
   * ```cpp
   * device.sendTo(targetId, [](JsonObject &payload) {
   *   payload["customField"] = "value";
   * });
   * ```
   */
  void sendTo(const String &targetId, std::function<void(JsonObject &)> payloadBuilder);

  /**
   * @brief Save GPIO pin state to persistent storage
   * 
   * @param pin GPIO pin number
   * @param state Pin state (HIGH/LOW or 1/0)
   * 
   * Saves the state so it can be restored after reboot.
   */
  void saveGPIOState(int pin, int state);

  /**
   * @brief Load previously saved GPIO pin state
   * 
   * @param pin GPIO pin number
   * @return int Saved pin state (HIGH/LOW), or -1 if not found
   */
  int loadGPIOState(int pin);

  /**
   * @brief Restore all saved GPIO states from persistent storage
   * 
   * Useful for maintaining output states after power cycle.
   */
  void restoreAllGPIOStates();

  /**
   * @brief Get the unique device ID
   * 
   * @return String Device ID (ESP32 MAC address or custom ID)
   */
  String getDeviceId();

  /**
   * @brief Get the user ID associated with this device
   * 
   * @return String User ID from provisioning/credentials
   */
  String getUserId();

  /**
   * @brief Set WiFi credentials for automatic connection
   * 
   * @param ssid WiFi network name
   * @param password WiFi password
   * 
   * Credentials are saved in NVS and reused on next boot.
   */
  void setWiFiCredentials(const String &ssid, const String &password);

  /**
   * @brief Set custom device ID (overrides default)
   * 
   * @param deviceId Custom device identifier
   * 
   * Call before begin() if you want to use a custom device ID.
   */
  void setDeviceId(const String &deviceId);

  /**
   * @brief Set custom user ID
   * 
   * @param userId User identifier to associate with device
   */
  void setUserId(const String &userId);

  /**
   * @brief Set all credentials at once
   * 
   * @param ssid WiFi network name
   * @param password WiFi password
   * @param deviceId Device ID
   * @param userId User ID (optional, can be empty)
   * 
   * Convenience function to set all credentials before provisioning.
   */
  void setCredentials(const String &ssid, const String &password, const String &deviceId, const String &userId = "");

  /**
   * @brief Clear all saved credentials from NVS storage
   * 
   * Use this to reset device to factory settings.
   */
  void clearCredentials();

  /**
   * @brief Check if device has valid credentials
   * 
   * @return true If WiFi and device credentials are configured
   * @return false If credentials are missing
   */
  bool hasCredentials();

  /**
   * @brief Send sensor data to Hyperwisor data logger
   * 
   * @param targetId Target dashboard/device ID
   * @param configId Data logger configuration ID
   * @param dataList List of sensor name-value pairs
   * 
   * Example:
   * ```cpp
   * device.send_Sensor_Data_logger(targetId, configId, {
   *   {"Temperature", 25.5},
   *   {"Humidity", 60.2},
   *   {"Pressure", 1013.25}
   * });
   * ```
   */
  void send_Sensor_Data_logger(const String &targetId, const String &configId, std::initializer_list<std::pair<const char *, float>> dataList);

  /**
   * @brief Update widget with string value
   * 
   * @param targetId Target dashboard/device ID
   * @param widgetId Widget ID from Hyperwisor dashboard
   * @param value String value to display
   * 
   * Works with text widgets, number displays, and status indicators.
   */
  void updateWidget(const String &targetId, const String &widgetId, const String &value);

  /**
   * @brief Update widget with numeric value
   * 
   * @param targetId Target dashboard/device ID
   * @param widgetId Widget ID from Hyperwisor dashboard
   * @param value Float value to display
   * 
   * Works with gauge widgets, progress bars, and numeric displays.
   */
  void updateWidget(const String &targetId, const String &widgetId, float value);

  /**
   * @brief Update widget with array of float values
   * 
   * @param targetId Target dashboard/device ID
   * @param widgetId Widget ID from Hyperwisor dashboard
   * @param values Vector of float values
   * 
   * Used for multi-point widgets like graphs and charts.
   */
  void updateWidget(const String &targetId, const String &widgetId, const std::vector<float> &values);

  /**
   * @brief Update widget with array of integer values
   * 
   * @param targetId Target dashboard/device ID
   * @param widgetId Widget ID from Hyperwisor dashboard
   * @param values Vector of integer values
   * 
   * Used for multi-point widgets like graphs and charts.
   */
  void updateWidget(const String &targetId, const String &widgetId, const std::vector<int> &values);

  /**
   * @brief Update widget with array of string values
   * 
   * @param targetId Target dashboard/device ID
   * @param widgetId Widget ID from Hyperwisor dashboard
   * @param values Vector of string values
   * 
   * Used for list widgets or multi-label displays.
   */
  void updateWidget(const String &targetId, const String &widgetId, const std::vector<String> &values);

  /**
   * @brief Show dialog popup on dashboard
   * 
   * @param targetId Target dashboard/device ID
   * @param title Dialog title
   * @param description Dialog message content
   * @param icon Icon type: "info" (default), "warning", "success", "error", "risk"
   * 
   * Displays a modal dialog to users on the Hyperwisor dashboard.
   */
  void showDialog(const String &targetId, const String &title, const String &description, const String &icon = "info");

  /**
   * @brief Show info dialog
   * 
   * @param targetId Target dashboard/device ID
   * @param title Dialog title
   * @param description Dialog message content
   * 
   * Convenience wrapper for showDialog() with info icon.
   */
  void info(const String &targetId, const String &title, const String &description);

  /**
   * @brief Show warning dialog
   * 
   * @param targetId Target dashboard/device ID
   * @param title Dialog title
   * @param description Dialog message content
   * 
   * Convenience wrapper for showDialog() with warning icon.
   */
  void warning(const String &targetId, const String &title, const String &description);

  /**
   * @brief Show success dialog
   * 
   * @param targetId Target dashboard/device ID
   * @param title Dialog title
   * @param description Dialog message content
   * 
   * Convenience wrapper for showDialog() with success icon.
   */
  void success(const String &targetId, const String &title, const String &description);

  /**
   * @brief Show error dialog
   * 
   * @param targetId Target dashboard/device ID
   * @param title Dialog title
   * @param description Dialog message content
   * 
   * Convenience wrapper for showDialog() with error icon.
   */
  void error(const String &targetId, const String &title, const String &description);

  /**
   * @brief Show risk/critical alert dialog
   * 
   * @param targetId Target dashboard/device ID
   * @param title Dialog title
   * @param description Dialog message content
   * 
   * Convenience wrapper for showDialog() with risk icon for critical alerts.
   */
  void risk(const String &targetId, const String &title, const String &description);

  /**
   * @brief Update flight attitude indicator widget
   * 
   * @param targetId Target dashboard/device ID
   * @param widgetId Widget ID from Hyperwisor dashboard
   * @param roll Roll angle in degrees (-180 to 180)
   * @param pitch Pitch angle in degrees (-90 to 90)
   * 
   * Updates artificial horizon/attitude indicator for drones or aircraft.
   */
  void updateFlightAttitude(const String &targetId, const String &widgetId, float roll, float pitch);

  /**
   * @brief Update widget position and size on dashboard
   * 
   * @param targetId Target dashboard/device ID
   * @param widgetId Widget ID from Hyperwisor dashboard
   * @param x X coordinate in pixels
   * @param y Y coordinate in pixels
   * @param w Width in pixels
   * @param h Height in pixels
   * @param r Rotation angle in degrees (0, 90, 180, 270), default 0
   * 
   * Dynamically reposition and resize widgets on the dashboard.
   */
  void updateWidgetPosition(const String &targetId, const String &widgetId, int x, int y, int w, int h, int r = 0);

  /**
   * @brief Update countdown timer widget
   * 
   * @param targetId Target dashboard/device ID
   * @param widgetId Widget ID from Hyperwisor dashboard
   * @param hours Hours remaining
   * @param minutes Minutes remaining
   * @param seconds Seconds remaining
   * 
   * Updates a countdown timer display on the dashboard.
   */
  void updateCountdown(const String &targetId, const String &widgetId, const String &hours, const String &minutes, const String &seconds);

  /**
   * @brief Update heat map visualization widget
   * 
   * @param targetId Target dashboard/device ID
   * @param widgetId Widget ID from Hyperwisor dashboard
   * @param dataPoints Vector of HeatMapPoint structures with x, y, value
   * 
   * Creates or updates a heat map visualization from sensor data points.
   * Each point contains coordinates (x, y) and an intensity value.
   * 
   * Example:
   * ```cpp
   * std::vector<HeatMapPoint> points;
   * HeatMapPoint point;
   * point.x = 100; point.y = 100; point.value = sensorReading;
   * points.push_back(point);
   * device.updateHeatMap(targetId, widgetId, points);
   * ```
   */
  void updateHeatMap(const String &targetId, const String &widgetId, const std::vector<HeatMapPoint> &dataPoints);

  /**
   * @brief Update 3D model widget with new model URL
   * 
   * @param targetId Target dashboard/device ID
   * @param widgetId Widget ID from Hyperwisor dashboard
   * @param modelUrl URL to 3D model file (.glb, .gltf)
   * 
   * Loads a new 3D model into a 3D viewer widget.
   */
  void update3DModel(const String &targetId, const String &widgetId, const String &modelUrl);

  /**
   * @brief Update 3D widget with multiple model transformations
   * 
   * @param targetId Target dashboard/device ID
   * @param widgetId Widget ID from Hyperwisor dashboard
   * @param models Vector of ThreeDModelUpdate structures
   * 
   * Updates position, rotation, scale, color, and material properties
   * of 3D models in a 3D scene widget.
   * 
   * ThreeDModelUpdate structure includes:
   * - modelId: Identifier for the model
   * - position[3]: X, Y, Z coordinates
   * - rotation[3]: Rotation around X, Y, Z axes
   * - scale[3]: Scale factors for each axis
   * - color: Hex color string (e.g., "#FF0000")
   * - metalness: 0.0-1.0 metallic appearance
   * - roughness: 0.0-1.0 surface roughness
   * - opacity: 0.0-1.0 transparency
   * - wireframe: true/false wireframe mode
   * - visible: true/false visibility
   */
  void update3DWidget(const String &targetId, const String &widgetId, const std::vector<ThreeDModelUpdate> &models);

  /**
   * @brief Send device status report to dashboard
   * 
   * @param targetId Target dashboard/device ID
   * 
   * Sends current device status including WiFi signal, uptime,
   * memory usage, and other system metrics to the dashboard.
   */
  void sendDeviceStatus(const String &targetId);
  
  /**
   * @brief Initialize NTP time synchronization
   * 
   * Sets up Network Time Protocol to get accurate time from internet.
   * Called automatically by begin(), but can be called manually if needed.
   */
  void initNTP();

  /**
   * @brief Set timezone for NTP time
   * 
   * @param timezone Timezone string (e.g., "UTC0", "EST5EDT", "CET-1")
   * 
   * Must be called after initNTP() to set correct local time.
   * Format: Standard timezone abbreviation + offset from UTC.
   */
  void setTimezone(const char* timezone);

  /**
   * @brief Get current network time as formatted string
   * 
   * @return String Current time in HH:MM:SS format
   * 
   * Requires NTP to be initialized.
   */
  String getNetworkTime();

  /**
   * @brief Get current network date as formatted string
   * 
   * @return String Current date in YYYY-MM-DD format
   * 
   * Requires NTP to be initialized.
   */
  String getNetworkDate();

  /**
   * @brief Get current network date and time as formatted string
   * 
   * @return String Current date and time in YYYY-MM-DD HH:MM:SS format
   * 
   * Requires NTP to be initialized.
   */
  String getNetworkDateTime();

  /**
   * @brief Insert data into Hyperwisor database
   * 
   * @param productId Product ID from Hyperwisor dashboard
   * @param deviceId Device ID to associate data with
   * @param tableName Database table name
   * @param dataBuilder Function to build JSON data record
   * 
   * Example:
   * ```cpp
   * device.insertDatainDatabase(productId, deviceId, "sensor_readings", 
   *   [](JsonObject &data) {
   *     data["temperature"] = 25.5;
   *     data["humidity"] = 60.2;
   *   });
   * ```
   */
  void insertDatainDatabase(const String &productId, const String &deviceId, const String &tableName, std::function<void(JsonObject &)> dataBuilder);

  /**
   * @brief Insert data into database and get response
   * 
   * @param productId Product ID from Hyperwisor dashboard
   * @param deviceId Device ID to associate data with
   * @param tableName Database table name
   * @param dataBuilder Function to build JSON data record
   * @return DynamicJsonDocument Server response with inserted data ID
   * 
   * Returns server response including the auto-generated data ID.
   */
  DynamicJsonDocument insertDatainDatabaseWithResponse(const String &productId, const String &deviceId, const String &tableName, std::function<void(JsonObject &)> dataBuilder);

  /**
   * @brief Retrieve data from database
   * 
   * @param productId Product ID from Hyperwisor dashboard
   * @param tableName Database table name
   * @param limit Maximum number of records to retrieve (default 50)
   * 
   * Fetches recent records from the specified database table.
   */
  void getDatabaseData(const String &productId, const String &tableName, int limit = 50);

  /**
   * @brief Retrieve data from database with response
   * 
   * @param productId Product ID from Hyperwisor dashboard
   * @param tableName Database table name
   * @param limit Maximum number of records to retrieve (default 50)
   * @return DynamicJsonDocument Query results
   * 
   * Returns retrieved records as JSON document.
   */
  DynamicJsonDocument getDatabaseDataWithResponse(const String &productId, const String &tableName, int limit = 50);

  /**
   * @brief Update existing database record
   * 
   * @param dataId ID of the record to update
   * @param dataBuilder Function to build updated JSON data
   * 
   * Example:
   * ```cpp
   * device.updateDatabaseData(dataId, [](JsonObject &data) {
   *   data["status"] = "completed";
   * });
   * ```
   */
  void updateDatabaseData(const String &dataId, std::function<void(JsonObject &)> dataBuilder);

  /**
   * @brief Update database record and get response
   * 
   * @param dataId ID of the record to update
   * @param dataBuilder Function to build updated JSON data
   * @return DynamicJsonDocument Server response
   */
  DynamicJsonDocument updateDatabaseDataWithResponse(const String &dataId, std::function<void(JsonObject &)> dataBuilder);

  /**
   * @brief Delete database record
   * 
   * @param dataId ID of the record to delete
   */
  void deleteDatabaseData(const String &dataId);

  /**
   * @brief Delete database record and get response
   * 
   * @param dataId ID of the record to delete
   * @return DynamicJsonDocument Server response
   */
  DynamicJsonDocument deleteDatabaseDataWithResponse(const String &dataId);

  /**
   * @brief Register/onboard new device with Hyperwisor
   * 
   * @param productId Product ID from Hyperwisor dashboard
   * @param userId User ID to associate device with
   * @param deviceName Friendly name for the device
   * @param deviceIdentifier Unique hardware identifier (MAC, serial number)
   * 
   * Registers a new device in the Hyperwisor system.
   */
  void onboardDevice(const String &productId, const String &userId, const String &deviceName, const String &deviceIdentifier);

  /**
   * @brief Register device and get response with device details
   * 
   * @param productId Product ID from Hyperwisor dashboard
   * @param userId User ID to associate device with
   * @param deviceName Friendly name for the device
   * @param deviceIdentifier Unique hardware identifier
   * @return DynamicJsonDocument Server response with device credentials
   * 
   * Returns device ID and other credentials assigned by Hyperwisor.
   */
  DynamicJsonDocument onboardDeviceWithResponse(const String &productId, const String &userId, const String &deviceName, const String &deviceIdentifier);

  /**
   * @brief Send SMS message via Hyperwisor service
   * 
   * @param productId Product ID from Hyperwisor dashboard
   * @param to Recipient phone number (with country code)
   * @param message SMS message content
   * 
   * Sends SMS notification through Hyperwisor's SMS gateway.
   */
  void sendSMS(const String &productId, const String &to, const String &message);

  /**
   * @brief Send SMS and get delivery confirmation
   * 
   * @param productId Product ID from Hyperwisor dashboard
   * @param to Recipient phone number (with country code)
   * @param message SMS message content
   * @return DynamicJsonDocument Server response with delivery status
   */
  DynamicJsonDocument sendSMSWithResponse(const String &productId, const String &to, const String &message);

  /**
   * @brief Authenticate user with email and password
   * 
   * @param email User email address
   * @param password User password
   * 
   * Authenticates against Hyperwisor user database.
   */
  void authenticateUser(const String &email, const String &password);

  /**
   * @brief Authenticate user and get session token
   * 
   * @param email User email address
   * @param password User password
   * @return DynamicJsonDocument Server response with auth token
   * 
   * Returns authentication token for subsequent API calls.
   */
  DynamicJsonDocument authenticateUserWithResponse(const String &email, const String &password);


  // JSON utility functions
  JsonObject findCommand(JsonObject& payload, const char* commandName);
  JsonObject findAction(JsonObject& payload, const char* commandName, const char* actionName);
  JsonObject findParams(JsonObject& payload, const char* commandName, const char* actionName);

private:
  // WiFi & Real-time Communication
  nikolaindustryrealtime realtime;
  WebServer server;
  DNSServer dnsServer;
  HTTPClient http;

  // Core functions
  void setupMessageHandler();
  void performOTA(const char *otaUrl);
  void getcredentials();
  void startAPMode();
  void handle_provision();
  void connectToWiFi();
  String getSuccessHtml();
  String getErrorHtml(String errorMessage);



  // User-defined command callback
  UserCommandCallback userCommandCallback = nullptr;

  // Credentials and config
  String ssid, password, userid, deviceid, productid, email, loaclip, macid, newtarget, versionid;
  String apiKey, secretKey;

  // AP Mode
  const char *apSSID = "NIKOLAINDUSTRY_Setup";
  const char *apPassword = "0123456789";
  String fversion = "0.0.1";

  // Retry Logic
  unsigned long lastReconnectAttempt = 0;
  const unsigned long reconnectInterval = 10000;
  int retryCount = 0;
  const int maxRetries = 6;
  
  // NTP
  bool ntpInitialized = false;
  String timezone = "UTC0";  // Default to UTC
};

#endif
