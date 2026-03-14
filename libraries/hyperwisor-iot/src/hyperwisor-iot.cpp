#include "hyperwisor-iot.h"

Preferences preferences;
Preferences gpioPreferences;
bool provision_request = false;

// Constructor
HyperwisorIOT::HyperwisorIOT()
{
}

// Public methods
void HyperwisorIOT::begin()
{
  getcredentials();

  if (!ssid.isEmpty() && !password.isEmpty())
  {
    connectToWiFi();
  }
  else
  {
    startAPMode();
  }
  
  // Initialize NTP for time functions
  initNTP();
}

// void HyperwisorIOT::begin()
// void HyperwisorIOT::loop()
// {
//   realtime.loop();
//   if (WiFi.getMode() == WIFI_AP)
//   {
//     dnsServer.processNextRequest();
//     server.handleClient();
//   }
// }

unsigned long apStartTime = 0;
bool apStarted = false;
bool wasConnected = false;

// Private methods
void HyperwisorIOT::loop()
{
  // Handle WiFi reconnection and WebSocket management
  if (WiFi.getMode() == WIFI_STA)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      // WiFi is connected
      if (!wasConnected)
      {
        // WiFi just reconnected after being disconnected
        Serial.println("WiFi reconnected. Re-establishing realtime connection...");
        realtime.begin(deviceid.c_str());
        setupMessageHandler();
        wasConnected = true;
        retryCount = 0;
      }
      
      // Check WebSocket connection status and attempt reconnection if needed
      if (!realtime.isNikolaindustryRealtimeConnected())
      {
        unsigned long currentMillis = millis();
        if (currentMillis - lastReconnectAttempt >= reconnectInterval)
        {
          lastReconnectAttempt = currentMillis;
          retryCount++;
          
          if (retryCount <= maxRetries)
          {
            Serial.printf("WebSocket disconnected. Reconnection attempt %d/%d...\n", retryCount, maxRetries);
            realtime.begin(deviceid.c_str());
            setupMessageHandler();
          }
          else
          {
            // Max retries exceeded, reboot the device
            Serial.println("Max reconnection attempts exceeded. Rebooting...");
            delay(1000);
            ESP.restart();
          }
        }
      }
      else
      {
        // WebSocket is connected, reset retry count
        retryCount = 0;
      }
      
      realtime.loop();
    }
    else
    {
      // WiFi disconnected
      if (wasConnected)
      {
        Serial.println("WiFi disconnected. Waiting for reconnection...");
        wasConnected = false;
      }
      
      // Attempt to reconnect WiFi
      unsigned long currentMillis = millis();
      if (currentMillis - lastReconnectAttempt >= reconnectInterval)
      {
        lastReconnectAttempt = currentMillis;
        Serial.println("Attempting WiFi reconnection...");
        WiFi.reconnect();
      }
    }
  }

  if (WiFi.getMode() == WIFI_AP)
  {
    if (!apStarted)
    {
      apStartTime = millis();
      apStarted = true;
    }

    dnsServer.processNextRequest();
    server.handleClient();

    if (millis() - apStartTime > 240000)
    { // 4 min
      Serial.println("Stuck in AP mode too long. Rebooting...");
      ESP.restart();
    }
  }
  else
  {
    apStarted = false;
  }
}


// AP mode
void HyperwisorIOT::startAPMode()
{
  Serial.println("Starting in AP mode...");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPassword);
  Serial.print("AP Started. IP: ");
  Serial.println(WiFi.softAPIP());

  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/api/provision", [this]()
            { handle_provision(); });
  server.begin();
  Serial.println("HTTP server started.");
}

// Provisioning
void HyperwisorIOT::handle_provision()
{
  if (server.hasArg("ssid") && server.arg("ssid").length() > 0)
  {
    ssid = server.arg("ssid");
    password = server.arg("password");
    deviceid = server.arg("target_id");
    userid = server.arg("user_id");
    

    preferences.begin("wifi-creds", false);
    preferences.putString("ssid", ssid);
    preferences.putString("password", password);
    preferences.putString("deviceid", deviceid);
    preferences.putString("userid", userid);
    preferences.end();

    server.send(200, "text/html", getSuccessHtml());
    provision_request = true;
    delay(2000);
    ESP.restart();
  }
  else
  {
    server.send(400, "text/html", getErrorHtml("Missing_SSID"));
  }
}

// Provisioning success
String HyperwisorIOT::getSuccessHtml()
{
  String message = "Device_is_connecting_to_the_new_network.";
  String redirectUrl = "hypervisorv4://provisioning?status=success&message=" + message;

  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Provisioning Success</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; display: flex; flex-direction: column; justify-content: center; align-items: center; height: 100vh; margin: 0; background-color: #f0f2f5; text-align: center; padding: 20px; }
    .card { background: white; padding: 40px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); max-width: 400px; }
    h1 { color: #28a745; font-size: 24px; }
    p { color: #666; font-size: 16px; margin-bottom: 30px; }
    a { display: inline-block; background-color: #007aff; color: white; padding: 15px 30px; text-decoration: none; border-radius: 8px; font-weight: 500; font-size: 18px; transition: background-color 0.2s; }
    a:hover { background-color: #0056b3; }
  </style>
</head>
<body>
  <div class="card">
    <h1>Configuration Sent!</h1>
    <p>Your device will now attempt to connect to the new Wi-Fi network.</p>
    <a href=")rawliteral" +
         redirectUrl + R"rawliteral(">Return to App</a>
  </div>
</body>
</html>
)rawliteral";
}

// Provisioning error
String HyperwisorIOT::getErrorHtml(String errorMessage)
{
  String redirectUrl = "hypervisorv4://provisioning?status=error&message=" + errorMessage;
  String decodedMessage = errorMessage;
  decodedMessage.replace("_", " ");

  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Provisioning Error</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; display: flex; flex-direction: column; justify-content: center; align-items: center; height: 100vh; margin: 0; background-color: #f0f2f5; text-align: center; padding: 20px; }
    .card { background: white; padding: 40px; border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.1); max-width: 400px; }
    h1 { color: #dc3545; font-size: 24px; }
    p { color: #666; font-size: 16px; margin-bottom: 30px; }
    a { display: inline-block; background-color: #6c757d; color: white; padding: 15px 30px; text-decoration: none; border-radius: 8px; font-weight: 500; font-size: 18px; transition: background-color 0.2s; }
    a:hover { background-color: #5a6268; }
  </style>
</head>
<body>
  <div class="card">
    <h1>Provisioning Failed</h1>
    <p>Error: )rawliteral" +
         decodedMessage + R"rawliteral(</p>
    <a href=")rawliteral" +
         redirectUrl + R"rawliteral(">Return to App</a>
  </div>
</body>
</html>
)rawliteral";
}

// Load credentials
void HyperwisorIOT::getcredentials()
{

  delay(1000);
  preferences.begin("wifi-creds", false);
  ssid = preferences.getString("ssid", "");
  password = preferences.getString("password", "");
  deviceid = preferences.getString("deviceid", "");
  userid = preferences.getString("userid", "");
  email = preferences.getString("email", "");
  productid = preferences.getString("productid", "");
  versionid = preferences.getString("firmware", "");
  macid = WiFi.macAddress();
  preferences.end();

  Serial.println("Loaded credentials:");
  Serial.println(ssid);
  Serial.println(password);
  Serial.println(deviceid);
}

// Connect to WiFi
void HyperwisorIOT::connectToWiFi()
{
  extern bool wasConnected;
  const char *hostname = "NIKOLAINDUSTRY_Device";
  WiFi.setHostname(hostname);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid.c_str(), password.c_str());

  Serial.println("Connecting to WiFi...");
  unsigned long start = millis();
  const unsigned long timeout = 30000;

  while (WiFi.status() != WL_CONNECTED && millis() - start < timeout)
  {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("\nWiFi connected: " + WiFi.localIP().toString());
    wasConnected = true;  // Set initial connection state
    realtime.begin(deviceid.c_str());
    setupMessageHandler();
  }
  else
  {
    Serial.println("\nFailed to connect. Switching to AP Mode.");
    wasConnected = false;
    startAPMode();
  }
}


void HyperwisorIOT::setupMessageHandler()
{
  realtime.setOnMessageCallback([this](JsonObject &msg)
                                {
                                  if (!msg.containsKey("payload"))
                                    return;

                                  JsonObject payload = msg["payload"];
                                  JsonArray commands = payload["commands"];
                                  this->newtarget = msg["from"] | "";

                                  for (JsonObject commandObj : commands)
                                  {
                                    const char *command = commandObj["command"];

                                    if (strcmp(command, "GPIO_MANAGEMENT") == 0)
                                    {
                                      JsonArray actions = commandObj["actions"];

                                      for (JsonObject actionObj : actions)
                                      {
                                        const char *action = actionObj["action"];
                                        JsonObject params = actionObj["params"];

                                        // Extract parameters
                                        int gpio = atoi(params["gpio"] | "0");
                                        String pinmodeStr = params["pinmode"] | "OUTPUT";
                                        String statusStr = params["status"] | "LOW";

                                        // Resolve pin mode
                                        int mode = OUTPUT;
                                        if (pinmodeStr == "INPUT")
                                          mode = INPUT;
                                        else if (pinmodeStr == "INPUT_PULLUP")
                                          mode = INPUT_PULLUP;

                                        // Resolve output state
                                        int value = LOW;
                                        if (statusStr == "HIGH")
                                          value = HIGH;

                                        // Apply pin configuration
                                        pinMode(gpio, mode);

                                        if (strcmp(action, "ON") == 0 || strcmp(action, "OFF") == 0)
                                        {
                                          digitalWrite(gpio, value);
                                          Serial.printf("Pin %d set to %s with mode %s\n", gpio, statusStr.c_str(), pinmodeStr.c_str());
                                        } // next action inside command
                                      }
                                    }
                                    else if (strcmp(command, "OTA") == 0)
                                    {
                                      JsonArray actions = commandObj["actions"];

                                      for (JsonObject actionObj : actions)
                                      {

                                        const char *action = actionObj["action"];
                                        JsonObject params = actionObj["params"];

                                        if (strcmp(action, "ota_update") == 0)
                                        {

                                          const char *otaUrl = params["url"];
                                          const char *version = params["version"];
                                          this->versionid = String(version);
                                          if (otaUrl != nullptr)
                                          {
                                            performOTA(otaUrl); // Trigger OTA update
                                          }
                                          else
                                          {
                                            Serial.println("Invalid OTA URL received.");
                                          }
                                        }
                                      }
                                    }
                                    else if (strcmp(command, "DEVICE_STATUS") == 0)
                                    {
                                      // Respond with device status (online)
                                      realtime.sendTo(this->newtarget, [](JsonObject &payload) {
                                        payload["status"] = "online";
                                        payload["response"] = "device_status";
                                      });
                                    } // next command
                                  }

                                  if (userCommandCallback)
                                  {
                                    userCommandCallback(msg); // Pass entire message to the user's custom logic
                                  } });
}

// Set the user's custom command handler
void HyperwisorIOT::setUserCommandHandler(UserCommandCallback cb)
{
  userCommandCallback = cb;
}

// Get device ID
String HyperwisorIOT::getDeviceId()
{
  preferences.begin("wifi-creds", true);
  String id = preferences.getString("deviceid", "unknown");
  preferences.end();
  return id;
}

// Get user ID
String HyperwisorIOT::getUserId()
{
  preferences.begin("wifi-creds", true);
  String id = preferences.getString("userid", "unknown");
  preferences.end();
  return id;
}

// Manual provisioning: Set WiFi credentials
void HyperwisorIOT::setWiFiCredentials(const String &ssid, const String &password)
{
  preferences.begin("wifi-creds", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.end();
  
  this->ssid = ssid;
  this->password = password;
  
  Serial.println("WiFi credentials saved manually.");
}

// Manual provisioning: Set device ID
void HyperwisorIOT::setDeviceId(const String &deviceId)
{
  preferences.begin("wifi-creds", false);
  preferences.putString("deviceid", deviceId);
  preferences.end();
  
  this->deviceid = deviceId;
  
  Serial.println("Device ID saved manually.");
}

// Manual provisioning: Set user ID
void HyperwisorIOT::setUserId(const String &userId)
{
  preferences.begin("wifi-creds", false);
  preferences.putString("userid", userId);
  preferences.end();
  
  this->userid = userId;
  
  Serial.println("User ID saved manually.");
}

// Manual provisioning: Set all credentials at once
void HyperwisorIOT::setCredentials(const String &ssid, const String &password, const String &deviceId, const String &userId)
{
  preferences.begin("wifi-creds", false);
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  preferences.putString("deviceid", deviceId);
  if (!userId.isEmpty()) {
    preferences.putString("userid", userId);
  }
  preferences.end();
  
  this->ssid = ssid;
  this->password = password;
  this->deviceid = deviceId;
  if (!userId.isEmpty()) {
    this->userid = userId;
  }
  
  Serial.println("All credentials saved manually.");
}

// Manual provisioning: Clear all credentials
void HyperwisorIOT::clearCredentials()
{
  preferences.begin("wifi-creds", false);
  preferences.clear();
  preferences.end();
  
  ssid = "";
  password = "";
  deviceid = "";
  userid = "";
  email = "";
  productid = "";
  
  Serial.println("All credentials cleared.");
}

// Manual provisioning: Check if credentials exist
bool HyperwisorIOT::hasCredentials()
{
  preferences.begin("wifi-creds", true);
  bool hasSsid = preferences.isKey("ssid");
  bool hasPassword = preferences.isKey("password");
  bool hasDeviceId = preferences.isKey("deviceid");
  preferences.end();
  
  return hasSsid && hasPassword && hasDeviceId;
}

// Send a message to a target
void HyperwisorIOT::sendTo(const String &targetId, std::function<void(JsonObject &)> payloadBuilder)
{
  DynamicJsonDocument doc(512); // Adjust size as needed
  JsonObject root = doc.to<JsonObject>();

  root["targetId"] = targetId;

  JsonObject payload = root.createNestedObject("payload");
  payloadBuilder(payload); // Let user fill the payload

  realtime.sendJson(root); // ✅ Pass JsonObject directly
}

// Send sensor data to a target
void HyperwisorIOT::send_Sensor_Data_logger(const String &targetId, const String &configId, std::initializer_list<std::pair<const char *, float>> dataList)
{
  String deviceId = getDeviceId();

  sendTo(targetId, [&](JsonObject &payload)
         {
    payload["type"] = "sensorDataResponse";
    payload["configId"] = configId;
    payload["deviceId"] = deviceId;

    JsonObject data = payload.createNestedObject("data");
    for (auto& kv : dataList) {
      data[kv.first] = kv.second;
    } });
}

// Update a widget with a string value
void HyperwisorIOT::updateWidget(const String &targetId, const String &widgetId, const String &value) {
  sendTo(targetId, [&](JsonObject &payload) {
    payload["widgetId"] = widgetId;
    payload["value"] = value;
  });
}

// Update a widget with a float value
void HyperwisorIOT::updateWidget(const String &targetId, const String &widgetId, float value) {
  sendTo(targetId, [&](JsonObject &payload) {
    payload["widgetId"] = widgetId;
    payload["value"] = value;
  });
}

// Update a widget with an array of float values
void HyperwisorIOT::updateWidget(const String &targetId, const String &widgetId, const std::vector<float> &values) {
  sendTo(targetId, [&](JsonObject &payload) {
    payload["widgetId"] = widgetId;
    JsonArray valueArray = payload.createNestedArray("value");
    for (const auto &val : values) {
      valueArray.add(val);
    }
  });
}

// Update a widget with an array of int values
void HyperwisorIOT::updateWidget(const String &targetId, const String &widgetId, const std::vector<int> &values) {
  sendTo(targetId, [&](JsonObject &payload) {
    payload["widgetId"] = widgetId;
    JsonArray valueArray = payload.createNestedArray("value");
    for (const auto &val : values) {
      valueArray.add(val);
    }
  });
}

// Update a widget with an array of String values
void HyperwisorIOT::updateWidget(const String &targetId, const String &widgetId, const std::vector<String> &values) {
  sendTo(targetId, [&](JsonObject &payload) {
    payload["widgetId"] = widgetId;
    JsonArray valueArray = payload.createNestedArray("value");
    for (const auto &val : values) {
      valueArray.add(val);
    }
  });
}

// Show a dialog to a target
void HyperwisorIOT::showDialog(const String &targetId, const String &title, const String &description, const String &icon) {
  sendTo(targetId, [&](JsonObject &payload) {
    payload["type"] = "dialog";
    payload["title"] = title;
    payload["description"] = description;
    payload["icon"] = icon;
  });
}

void HyperwisorIOT::info(const String &targetId, const String &title, const String &description) {
  showDialog(targetId, title, description, "info");
}

void HyperwisorIOT::warning(const String &targetId, const String &title, const String &description) {
  showDialog(targetId, title, description, "warning");
}

void HyperwisorIOT::success(const String &targetId, const String &title, const String &description) {
  showDialog(targetId, title, description, "success");
}

void HyperwisorIOT::error(const String &targetId, const String &title, const String &description) {
  showDialog(targetId, title, description, "error");
}

void HyperwisorIOT::risk(const String &targetId, const String &title, const String &description) {
  showDialog(targetId, title, description, "risk");
}

// Update flight attitude meter widget
void HyperwisorIOT::updateFlightAttitude(const String &targetId, const String &widgetId, float roll, float pitch) {
  sendTo(targetId, [&](JsonObject &payload) {
    payload["widgetId"] = widgetId;
    JsonObject value = payload.createNestedObject("value");
    value["roll"] = roll;
    value["pitch"] = pitch;
  });
}

// Update widget position, size, and rotation
void HyperwisorIOT::updateWidgetPosition(const String &targetId, const String &widgetId, int x, int y, int w, int h, int r) {
  sendTo(targetId, [&](JsonObject &payload) {
    payload["widgetId"] = widgetId;
    payload["x"] = x;
    payload["y"] = y;
    payload["w"] = w;
    payload["h"] = h;
    payload["r"] = r;
  });
}

// Update countdown widget
void HyperwisorIOT::updateCountdown(const String &targetId, const String &widgetId, const String &hours, const String &minutes, const String &seconds) {
  sendTo(targetId, [&](JsonObject &payload) {
    payload["widgetId"] = widgetId;
    payload["hr"] = hours;
    payload["min"] = minutes;
    payload["sec"] = seconds;
  });
}

// Update heat map widget
void HyperwisorIOT::updateHeatMap(const String &targetId, const String &widgetId, const std::vector<HeatMapPoint> &dataPoints) {
  sendTo(targetId, [&](JsonObject &payload) {
    payload["widgetId"] = widgetId;
    JsonArray value = payload.createNestedArray("value");
    
    for (const auto &point : dataPoints) {
      JsonObject pointObj = value.createNestedObject();
      pointObj["x"] = point.x;
      pointObj["y"] = point.y;
      pointObj["value"] = point.value;
    }
  });
}

// Update 3D model widget
void HyperwisorIOT::update3DModel(const String &targetId, const String &widgetId, const String &modelUrl) {
  sendTo(targetId, [&](JsonObject &payload) {
    payload["widgetId"] = widgetId;
    payload["value"] = modelUrl;
  });
}

// Update 3D widget with multiple model transformations
void HyperwisorIOT::update3DWidget(const String &targetId, const String &widgetId, const std::vector<ThreeDModelUpdate> &models) {
  sendTo(targetId, [&](JsonObject &payload) {
    payload["widgetId"] = widgetId;
    JsonArray valueArray = payload.createNestedArray("value");
    
    for (const auto &m : models) {
      JsonObject modelObj = valueArray.createNestedObject();
      modelObj["modelId"] = m.modelId;
      
      JsonObject updates = modelObj.createNestedObject("updates");
      
      JsonArray pos = updates.createNestedArray("position");
      pos.add(m.position[0]); pos.add(m.position[1]); pos.add(m.position[2]);
      
      JsonArray rot = updates.createNestedArray("rotation");
      rot.add(m.rotation[0]); rot.add(m.rotation[1]); rot.add(m.rotation[2]);
      
      JsonArray sca = updates.createNestedArray("scale");
      sca.add(m.scale[0]); sca.add(m.scale[1]); sca.add(m.scale[2]);
      
      updates["color"] = m.color;
      updates["metalness"] = m.metalness;
      updates["roughness"] = m.roughness;
      updates["opacity"] = m.opacity;
      updates["wireframe"] = m.wireframe;
      updates["visible"] = m.visible;
    }
  });
}

// Send device status to a target
void HyperwisorIOT::sendDeviceStatus(const String &targetId) {
  realtime.sendTo(targetId, [](JsonObject &payload) {
    payload["status"] = "online";
    payload["response"] = "device_status";
  });
}

// Set API keys for database operations
void HyperwisorIOT::setApiKeys(const String &apiKey, const String &secretKey) {
  this->apiKey = apiKey;
  this->secretKey = secretKey;
}

// Insert data into a database
void HyperwisorIOT::insertDatainDatabase(const String &productId, const String &deviceId, const String &tableName, std::function<void(JsonObject &)> dataBuilder) {
  // Check if API keys are set
  if (apiKey.isEmpty() || secretKey.isEmpty()) {
    Serial.println("Error: API keys not set. Please call setApiKeys() first.");
    return;
  }

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Error: No WiFi connection.");
    return;
  }

  // Create JSON payload
  DynamicJsonDocument doc(1024);
  JsonObject root = doc.to<JsonObject>();
  
  root["product_id"] = productId;
  root["device_id"] = deviceId;
  root["table_name"] = tableName;
  
  JsonObject dataPayload = root.createNestedObject("data_payload");
  dataBuilder(dataPayload); // Let user fill the data payload

  // Send HTTP POST request
  WiFiClientSecure client;
  client.setInsecure(); // Ignore SSL certificate verification (not recommended for production)
  HTTPClient http;
  
  http.begin(client, "https://cgsuxlbravclbbpnvfky.supabase.co/functions/v1/manufacturer-api/database/runtime-data");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", this->apiKey);
  http.addHeader("x-secret-key", this->secretKey);
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Database data sent successfully. Response: " + response);
  } else {
    Serial.println("Error sending database data. HTTP Response code: " + String(httpResponseCode));
  }
  
  http.end();
}

// Insert data into a database and get a response
DynamicJsonDocument HyperwisorIOT::insertDatainDatabaseWithResponse(const String &productId, const String &deviceId, const String &tableName, std::function<void(JsonObject &)> dataBuilder) {
  // Create a JSON document to hold the response
  DynamicJsonDocument responseDoc(1024);
  JsonObject response = responseDoc.to<JsonObject>();
  
  // Check if API keys are set
  if (apiKey.isEmpty() || secretKey.isEmpty()) {
    response["success"] = false;
    response["error"] = "API keys not set. Please call setApiKeys() first.";
    return responseDoc;
  }

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    response["success"] = false;
    response["error"] = "No WiFi connection.";
    return responseDoc;
  }

  // Create JSON payload
  DynamicJsonDocument doc(1024);
  JsonObject root = doc.to<JsonObject>();
  
  root["product_id"] = productId;
  root["device_id"] = deviceId;
  root["table_name"] = tableName;
  
  JsonObject dataPayload = root.createNestedObject("data_payload");
  dataBuilder(dataPayload); // Let user fill the data payload

  // Send HTTP POST request
  WiFiClientSecure client;
  client.setInsecure(); // Ignore SSL certificate verification (not recommended for production)
  HTTPClient http;
  
  http.begin(client, "https://cgsuxlbravclbbpnvfky.supabase.co/functions/v1/manufacturer-api/database/runtime-data");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", this->apiKey);
  http.addHeader("x-secret-key", this->secretKey);
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    String httpResponse = http.getString();
    response["success"] = true;
    response["http_response_code"] = httpResponseCode;
    
    // Try to parse the response as JSON
    DynamicJsonDocument responsePayload(1024);
    DeserializationError error = deserializeJson(responsePayload, httpResponse);
    if (!error) {
      response["data"] = responsePayload.as<JsonObject>();
    } else {
      response["raw_response"] = httpResponse;
    }
  } else {
    response["success"] = false;
    response["http_response_code"] = httpResponseCode;
    response["error"] = "HTTP request failed";
  }
  
  http.end();
  return responseDoc;
}

// Get database data
void HyperwisorIOT::getDatabaseData(const String &productId, const String &tableName, int limit) {
  // Check if API keys are set
  if (apiKey.isEmpty() || secretKey.isEmpty()) {
    Serial.println("Error: API keys not set. Please call setApiKeys() first.");
    return;
  }

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Error: No WiFi connection.");
    return;
  }

  // Build the URL with query parameters
  String url = "https://cgsuxlbravclbbpnvfky.supabase.co/functions/v1/manufacturer-api/database/runtime-data";
  url += "?product_id=" + productId;
  url += "&table_name=" + tableName;
  url += "&limit=" + String(limit);

  // Send HTTP GET request
  WiFiClientSecure client;
  client.setInsecure(); // Ignore SSL certificate verification (not recommended for production)
  HTTPClient http;
  
  http.begin(client, url);
  http.addHeader("x-api-key", this->apiKey);
  http.addHeader("x-secret-key", this->secretKey);
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Database data retrieved successfully. Response: " + response);
  } else {
    Serial.println("Error retrieving database data. HTTP Response code: " + String(httpResponseCode));
  }
  
  http.end();
}

// Get database data and get a response
DynamicJsonDocument HyperwisorIOT::getDatabaseDataWithResponse(const String &productId, const String &tableName, int limit) {
  // Create a JSON document to hold the response
  DynamicJsonDocument responseDoc(2048);
  JsonObject response = responseDoc.to<JsonObject>();
  
  // Check if API keys are set
  if (apiKey.isEmpty() || secretKey.isEmpty()) {
    response["success"] = false;
    response["error"] = "API keys not set. Please call setApiKeys() first.";
    return responseDoc;
  }

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    response["success"] = false;
    response["error"] = "No WiFi connection.";
    return responseDoc;
  }

  // Build the URL with query parameters
  String url = "https://cgsuxlbravclbbpnvfky.supabase.co/functions/v1/manufacturer-api/database/runtime-data";
  url += "?product_id=" + productId;
  url += "&table_name=" + tableName;
  url += "&limit=" + String(limit);

  // Send HTTP GET request
  WiFiClientSecure client;
  client.setInsecure(); // Ignore SSL certificate verification (not recommended for production)
  HTTPClient http;
  
  http.begin(client, url);
  http.addHeader("x-api-key", this->apiKey);
  http.addHeader("x-secret-key", this->secretKey);
  
  int httpResponseCode = http.GET();
  
  if (httpResponseCode > 0) {
    String httpResponse = http.getString();
    response["success"] = true;
    response["http_response_code"] = httpResponseCode;
    
    // Try to parse the response as JSON
    DynamicJsonDocument responsePayload(2048);
    DeserializationError error = deserializeJson(responsePayload, httpResponse);
    if (!error) {
      response["data"] = responsePayload.as<JsonObject>();
    } else {
      response["raw_response"] = httpResponse;
    }
  } else {
    response["success"] = false;
    response["http_response_code"] = httpResponseCode;
    response["error"] = "HTTP request failed";
  }
  
  http.end();
  return responseDoc;
}

// Update database data
void HyperwisorIOT::updateDatabaseData(const String &dataId, std::function<void(JsonObject &)> dataBuilder) {
  // Check if API keys are set
  if (apiKey.isEmpty() || secretKey.isEmpty()) {
    Serial.println("Error: API keys not set. Please call setApiKeys() first.");
    return;
  }

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Error: No WiFi connection.");
    return;
  }

  // Create JSON payload
  DynamicJsonDocument doc(1024);
  JsonObject root = doc.to<JsonObject>();
  
  JsonObject dataPayload = root.createNestedObject("data_payload");
  dataBuilder(dataPayload); // Let user fill the data payload

  // Send HTTP PUT request
  WiFiClientSecure client;
  client.setInsecure(); // Ignore SSL certificate verification (not recommended for production)
  HTTPClient http;
  
  String url = "https://cgsuxlbravclbbpnvfky.supabase.co/functions/v1/manufacturer-api/database/runtime-data/" + dataId;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", this->apiKey);
  http.addHeader("x-secret-key", this->secretKey);
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  int httpResponseCode = http.PUT(jsonString);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Database data updated successfully. Response: " + response);
  } else {
    Serial.println("Error updating database data. HTTP Response code: " + String(httpResponseCode));
  }
  
  http.end();
}

// Update database data and get a response
DynamicJsonDocument HyperwisorIOT::updateDatabaseDataWithResponse(const String &dataId, std::function<void(JsonObject &)> dataBuilder) {
  // Create a JSON document to hold the response
  DynamicJsonDocument responseDoc(2048);
  JsonObject response = responseDoc.to<JsonObject>();
  
  // Check if API keys are set
  if (apiKey.isEmpty() || secretKey.isEmpty()) {
    response["success"] = false;
    response["error"] = "API keys not set. Please call setApiKeys() first.";
    return responseDoc;
  }

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    response["success"] = false;
    response["error"] = "No WiFi connection.";
    return responseDoc;
  }

  // Create JSON payload
  DynamicJsonDocument doc(1024);
  JsonObject root = doc.to<JsonObject>();
  
  JsonObject dataPayload = root.createNestedObject("data_payload");
  dataBuilder(dataPayload); // Let user fill the data payload

  // Send HTTP PUT request
  WiFiClientSecure client;
  client.setInsecure(); // Ignore SSL certificate verification (not recommended for production)
  HTTPClient http;
  
  String url = "https://cgsuxlbravclbbpnvfky.supabase.co/functions/v1/manufacturer-api/database/runtime-data/" + dataId;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", this->apiKey);
  http.addHeader("x-secret-key", this->secretKey);
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  int httpResponseCode = http.PUT(jsonString);
  
  if (httpResponseCode > 0) {
    String httpResponse = http.getString();
    response["success"] = true;
    response["http_response_code"] = httpResponseCode;
    
    // Try to parse the response as JSON
    DynamicJsonDocument responsePayload(2048);
    DeserializationError error = deserializeJson(responsePayload, httpResponse);
    if (!error) {
      response["data"] = responsePayload.as<JsonObject>();
    } else {
      response["raw_response"] = httpResponse;
    }
  } else {
    response["success"] = false;
    response["http_response_code"] = httpResponseCode;
    response["error"] = "HTTP request failed";
  }
  
  http.end();
  return responseDoc;
}

// Delete database data
void HyperwisorIOT::deleteDatabaseData(const String &dataId) {
  // Check if API keys are set
  if (apiKey.isEmpty() || secretKey.isEmpty()) {
    Serial.println("Error: API keys not set. Please call setApiKeys() first.");
    return;
  }

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Error: No WiFi connection.");
    return;
  }

  // Send HTTP DELETE request
  WiFiClientSecure client;
  client.setInsecure(); // Ignore SSL certificate verification (not recommended for production)
  HTTPClient http;
  
  String url = "https://cgsuxlbravclbbpnvfky.supabase.co/functions/v1/manufacturer-api/database/runtime-data/" + dataId;
  http.begin(client, url);
  http.addHeader("x-api-key", this->apiKey);
  http.addHeader("x-secret-key", this->secretKey);
  
  int httpResponseCode = http.sendRequest("DELETE", "");
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Database data deleted successfully. Response: " + response);
  } else {
    Serial.println("Error deleting database data. HTTP Response code: " + String(httpResponseCode));
  }
  
  http.end();
}

// Delete database data and get a response
DynamicJsonDocument HyperwisorIOT::deleteDatabaseDataWithResponse(const String &dataId) {
  // Create a JSON document to hold the response
  DynamicJsonDocument responseDoc(1024);
  JsonObject response = responseDoc.to<JsonObject>();
  
  // Check if API keys are set
  if (apiKey.isEmpty() || secretKey.isEmpty()) {
    response["success"] = false;
    response["error"] = "API keys not set. Please call setApiKeys() first.";
    return responseDoc;
  }

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    response["success"] = false;
    response["error"] = "No WiFi connection.";
    return responseDoc;
  }

  // Send HTTP DELETE request
  WiFiClientSecure client;
  client.setInsecure(); // Ignore SSL certificate verification (not recommended for production)
  HTTPClient http;
  
  String url = "https://cgsuxlbravclbbpnvfky.supabase.co/functions/v1/manufacturer-api/database/runtime-data/" + dataId;
  http.begin(client, url);
  http.addHeader("x-api-key", this->apiKey);
  http.addHeader("x-secret-key", this->secretKey);
  
  int httpResponseCode = http.sendRequest("DELETE", "");
  
  if (httpResponseCode > 0) {
    String httpResponse = http.getString();
    response["success"] = true;
    response["http_response_code"] = httpResponseCode;
    
    // Try to parse the response as JSON
    DynamicJsonDocument responsePayload(1024);
    DeserializationError error = deserializeJson(responsePayload, httpResponse);
    if (!error) {
      response["data"] = responsePayload.as<JsonObject>();
    } else {
      response["raw_response"] = httpResponse;
    }
  } else {
    response["success"] = false;
    response["http_response_code"] = httpResponseCode;
    response["error"] = "HTTP request failed";
  }
  
  http.end();
  return responseDoc;
}

// Onboard a device
void HyperwisorIOT::onboardDevice(const String &productId, const String &userId, const String &deviceName, const String &deviceIdentifier) {
  // Check if API keys are set
  if (apiKey.isEmpty() || secretKey.isEmpty()) {
    Serial.println("Error: API keys not set. Please call setApiKeys() first.");
    return;
  }

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Error: No WiFi connection.");
    return;
  }

  // Create JSON payload
  DynamicJsonDocument doc(1024);
  JsonObject root = doc.to<JsonObject>();
  
  root["product_id"] = productId;
  root["user_id"] = userId;
  root["device_name"] = deviceName;
  root["device_identifier"] = deviceIdentifier;

  // Send HTTP POST request
  WiFiClientSecure client;
  client.setInsecure(); // Ignore SSL certificate verification (not recommended for production)
  HTTPClient http;
  
  http.begin(client, "https://cgsuxlbravclbbpnvfky.supabase.co/functions/v1/manufacturer-api/onboarding/device");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", this->apiKey);
  http.addHeader("x-secret-key", this->secretKey);
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Device onboarded successfully. Response: " + response);
  } else {
    Serial.println("Error onboarding device. HTTP Response code: " + String(httpResponseCode));
  }
  
  http.end();
}

// Onboard a device and get a response
DynamicJsonDocument HyperwisorIOT::onboardDeviceWithResponse(const String &productId, const String &userId, const String &deviceName, const String &deviceIdentifier) {
  // Create a JSON document to hold the response
  DynamicJsonDocument responseDoc(2048);
  JsonObject response = responseDoc.to<JsonObject>();
  
  // Check if API keys are set
  if (apiKey.isEmpty() || secretKey.isEmpty()) {
    response["success"] = false;
    response["error"] = "API keys not set. Please call setApiKeys() first.";
    return responseDoc;
  }

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    response["success"] = false;
    response["error"] = "No WiFi connection.";
    return responseDoc;
  }

  // Create JSON payload
  DynamicJsonDocument doc(1024);
  JsonObject root = doc.to<JsonObject>();
  
  root["product_id"] = productId;
  root["user_id"] = userId;
  root["device_name"] = deviceName;
  root["device_identifier"] = deviceIdentifier;

  // Send HTTP POST request
  WiFiClientSecure client;
  client.setInsecure(); // Ignore SSL certificate verification (not recommended for production)
  HTTPClient http;
  
  http.begin(client, "https://cgsuxlbravclbbpnvfky.supabase.co/functions/v1/manufacturer-api/onboarding/device");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", this->apiKey);
  http.addHeader("x-secret-key", this->secretKey);
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    String httpResponse = http.getString();
    response["success"] = true;
    response["http_response_code"] = httpResponseCode;
    
    // Try to parse the response as JSON
    DynamicJsonDocument responsePayload(2048);
    DeserializationError error = deserializeJson(responsePayload, httpResponse);
    if (!error) {
      response["data"] = responsePayload.as<JsonObject>();
    } else {
      response["raw_response"] = httpResponse;
    }
  } else {
    response["success"] = false;
    response["http_response_code"] = httpResponseCode;
    response["error"] = "HTTP request failed";
  }
  
  http.end();
  return responseDoc;
}

// Send SMS
void HyperwisorIOT::sendSMS(const String &productId, const String &to, const String &message) {
  // Check if API keys are set
  if (apiKey.isEmpty() || secretKey.isEmpty()) {
    Serial.println("Error: API keys not set. Please call setApiKeys() first.");
    return;
  }

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Error: No WiFi connection.");
    return;
  }

  // Create JSON payload
  DynamicJsonDocument doc(512);
  JsonObject root = doc.to<JsonObject>();
  
  root["productId"] = productId;
  root["to"] = to;
  root["message"] = message;

  // Send HTTP POST request
  WiFiClientSecure client;
  client.setInsecure(); // Ignore SSL certificate verification (not recommended for production)
  HTTPClient http;
  
  http.begin(client, "https://cgsuxlbravclbbpnvfky.supabase.co/functions/v1/sms-service");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", this->apiKey);
  http.addHeader("x-secret-key", this->secretKey);
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("SMS sent successfully. Response: " + response);
  } else {
    Serial.println("Error sending SMS. HTTP Response code: " + String(httpResponseCode));
  }
  
  http.end();
}

// Send SMS and get a response
DynamicJsonDocument HyperwisorIOT::sendSMSWithResponse(const String &productId, const String &to, const String &message) {
  // Create a JSON document to hold the response
  DynamicJsonDocument responseDoc(1024);
  JsonObject response = responseDoc.to<JsonObject>();
  
  // Check if API keys are set
  if (apiKey.isEmpty() || secretKey.isEmpty()) {
    response["success"] = false;
    response["error"] = "API keys not set. Please call setApiKeys() first.";
    return responseDoc;
  }

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    response["success"] = false;
    response["error"] = "No WiFi connection.";
    return responseDoc;
  }

  // Create JSON payload
  DynamicJsonDocument doc(512);
  JsonObject root = doc.to<JsonObject>();
  
  root["productId"] = productId;
  root["to"] = to;
  root["message"] = message;

  // Send HTTP POST request
  WiFiClientSecure client;
  client.setInsecure(); // Ignore SSL certificate verification (not recommended for production)
  HTTPClient http;
  
  http.begin(client, "https://cgsuxlbravclbbpnvfky.supabase.co/functions/v1/sms-service");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", this->apiKey);
  http.addHeader("x-secret-key", this->secretKey);
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    String httpResponse = http.getString();
    response["success"] = true;
    response["http_response_code"] = httpResponseCode;
    
    // Try to parse the response as JSON
    DynamicJsonDocument responsePayload(1024);
    DeserializationError error = deserializeJson(responsePayload, httpResponse);
    if (!error) {
      response["data"] = responsePayload.as<JsonObject>();
    } else {
      response["raw_response"] = httpResponse;
    }
  } else {
    response["success"] = false;
    response["http_response_code"] = httpResponseCode;
    response["error"] = "HTTP request failed";
  }
  
  http.end();
  return responseDoc;
}

// Save GPIO state
void HyperwisorIOT::saveGPIOState(int pin, int state)
{
  gpioPreferences.begin("gpio-states", false);
  gpioPreferences.putInt(("pin_" + String(pin)).c_str(), state);
  gpioPreferences.end();
}

// Load GPIO state
int HyperwisorIOT::loadGPIOState(int pin)
{
  gpioPreferences.begin("gpio-states", false);
  return gpioPreferences.getInt(("pin_" + String(pin)).c_str(), LOW);
  gpioPreferences.end();
}

// Restore all GPIO states
void HyperwisorIOT::restoreAllGPIOStates()
{
  gpioPreferences.begin("gpio-states", false);
  for (int pin = 0; pin < 40; pin++)
  {
    String key = "pin_" + String(pin);
    if (gpioPreferences.isKey(key.c_str()))
    {
      int state = gpioPreferences.getInt(key.c_str(), LOW);
      pinMode(pin, OUTPUT);
      digitalWrite(pin, state);
      Serial.printf("Restored pin %d to state %d\n", pin, state);
    }
  }
  gpioPreferences.end();
}

// Perform OTA
void HyperwisorIOT::performOTA(const char *otaUrl)
{
  String firmwarefeedback;
  StaticJsonDocument<512> firmwarefeedbackDoc; // Increased buffer size

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;

  Serial.printf("Attempting to download OTA file from %s\n", otaUrl);

  http.begin(client, otaUrl);
  int httpCode = http.GET();

  if (httpCode != HTTP_CODE_OK)
  {
    Serial.printf("HTTP GET failed, error: %s\n", http.errorToString(httpCode).c_str());
    realtime.sendTo(newtarget, [&](JsonObject &payload)
                    {
      payload["status"] = "OTA_Download_Failed";
      payload["value"] = http.errorToString(httpCode).c_str(); });
    http.end();
    return;
  }

  int contentLength = http.getSize();
  Serial.printf("Content length: %d\n", contentLength);

  if (contentLength <= 0)
  {
    Serial.println("No content in the OTA update file.");
    realtime.sendTo(newtarget, [&](JsonObject &payload)
                    {
      payload["status"] = "OTA_Download_Failed";
      payload["value"] = "No content in OTA file."; });
    http.end();
    return;
  }

  Serial.println("Starting OTA update...");

  realtime.sendTo(newtarget, [&](JsonObject &payload)
                  { payload["status"] = "OTA_Update_Started"; });

  if (!Update.begin(contentLength))
  {
    Serial.println("Not enough space for OTA update!");
    realtime.sendTo(newtarget, [&](JsonObject &payload)
                    {
      payload["status"] = "OTA_Download_Failed";
      payload["value"] = "Not enough space!"; });
    http.end();
    return;
  }

  WiFiClient &stream = http.getStream();
  size_t written = Update.writeStream(stream);

  Serial.printf("Written %d bytes\n", written);

  if (written == contentLength && Update.end() && Update.isFinished())
  {
    Serial.println("OTA update successfully completed.");

    preferences.begin("wifi-creds", false);
    preferences.putString("firmware", versionid);
    preferences.end();

    realtime.sendTo(newtarget, [&](JsonObject &payload)
                    {
      payload["status"] = "OTA_Update_Completed";
      payload["value"] = "Rebooting"; });

    delay(2000);
    ESP.restart();
  }
  else
  {
    Serial.println("OTA update failed!");
    realtime.sendTo(newtarget, [&](JsonObject &payload)
                    {
      payload["status"] = "OTA_Update_Failed";
      payload["value"] = Update.errorString(); });
  }

  http.end();
}

// Authenticate user
void HyperwisorIOT::authenticateUser(const String &email, const String &password) {
  // Check if API keys are set
  if (apiKey.isEmpty() || secretKey.isEmpty()) {
    Serial.println("Error: API keys not set. Please call setApiKeys() first.");
    return;
  }

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Error: No WiFi connection.");
    return;
  }

  // Create JSON payload
  DynamicJsonDocument doc(512);
  JsonObject root = doc.to<JsonObject>();
  
  root["email"] = email;
  root["password"] = password;

  // Send HTTP POST request
  WiFiClientSecure client;
  client.setInsecure(); // Ignore SSL certificate verification (not recommended for production)
  HTTPClient http;
  
  http.begin(client, "https://cgsuxlbravclbbpnvfky.supabase.co/functions/v1/manufacturer-api/auth/signin");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", this->apiKey);
  http.addHeader("x-secret-key", this->secretKey);
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.println("Authentication successful. Response: " + response);
  } else {
    Serial.println("Error during authentication. HTTP Response code: " + String(httpResponseCode));
  }
  
  http.end();
}

// Authenticate user with response
DynamicJsonDocument HyperwisorIOT::authenticateUserWithResponse(const String &email, const String &password) {
  // Create a JSON document to hold the response
  DynamicJsonDocument responseDoc(1024);
  JsonObject response = responseDoc.to<JsonObject>();
  
  // Check if API keys are set
  if (apiKey.isEmpty() || secretKey.isEmpty()) {
    response["success"] = false;
    response["error"] = "API keys not set. Please call setApiKeys() first.";
    return responseDoc;
  }

  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    response["success"] = false;
    response["error"] = "No WiFi connection.";
    return responseDoc;
  }

  // Create JSON payload
  DynamicJsonDocument doc(512);
  JsonObject root = doc.to<JsonObject>();
  
  root["email"] = email;
  root["password"] = password;

  // Send HTTP POST request
  WiFiClientSecure client;
  client.setInsecure(); // Ignore SSL certificate verification (not recommended for production)
  HTTPClient http;
  
  http.begin(client, "https://cgsuxlbravclbbpnvfky.supabase.co/functions/v1/manufacturer-api/auth/signin");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("x-api-key", this->apiKey);
  http.addHeader("x-secret-key", this->secretKey);
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    String httpResponse = http.getString();
    response["success"] = true;
    response["http_response_code"] = httpResponseCode;
    
    // Try to parse the response as JSON
    DynamicJsonDocument responsePayload(1024);
    DeserializationError error = deserializeJson(responsePayload, httpResponse);
    if (!error) {
      response["data"] = responsePayload.as<JsonObject>();
    } else {
      response["raw_response"] = httpResponse;
    }
  } else {
    response["success"] = false;
    response["http_response_code"] = httpResponseCode;
    response["error"] = "HTTP request failed";
  }
  
  http.end();
  return responseDoc;
}

// Time and date functions using NTP
void HyperwisorIOT::initNTP() {
  if (!ntpInitialized) {
    // Configure NTP only once with timezone support
    // Parse timezone to get offset values
    long gmtOffset = 0;
    int daylightOffset = 0;
    
    // Simple parsing for common timezone formats
    if (timezone.startsWith("IST")) {
      gmtOffset = 5 * 3600 + 30 * 60;  // UTC+5:30
    } else if (timezone.startsWith("EST")) {
      gmtOffset = -5 * 3600;  // UTC-5
    } else if (timezone.startsWith("PST")) {
      gmtOffset = -8 * 3600;  // UTC-8
    } else if (timezone.startsWith("UTC")) {
      gmtOffset = 0;  // UTC
    }
    
    // Configure NTP servers with timezone offsets
    configTime(gmtOffset, daylightOffset, "pool.ntp.org", "time.nist.gov");
    ntpInitialized = true;
    
    // Wait for time to be synchronized (but don't block forever)
    time_t now = time(nullptr);
    int retry = 0;
    while (now < 1000000000L && retry < 5) {
      delay(500);
      now = time(nullptr);
      retry++;
    }
    
    if (now < 1000000000L) {
      Serial.println("Warning: Failed to synchronize with NTP server immediately, will try again when needed.");
    } else {
      Serial.println("NTP time synchronized successfully.");
    }
  }
}

// Set timezone
void HyperwisorIOT::setTimezone(const char* timezone) {
  this->timezone = String(timezone);
  
  // If NTP is already initialized, we need to reconfigure with new timezone
  if (ntpInitialized) {
    ntpInitialized = false;  // Reset flag to force reinitialization
    initNTP();  // Reinitialize with new timezone
  }
}

// Get network time
String HyperwisorIOT::getNetworkTime() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Error: No WiFi connection.");
    return "";
  }

  // Initialize NTP if not already done
  initNTP();
  
  // Get current time
  time_t now = time(nullptr);
  
  // If time is not synchronized, try again
  if (now < 1000000000L) {
    int retry = 0;
    while (now < 1000000000L && retry < 5) {
      delay(500);
      now = time(nullptr);
      retry++;
    }
    
    if (now < 1000000000L) {
      Serial.println("Error: Failed to get time from NTP server.");
      return "";
    }
  }
  
  // Format time as HH:MM:SS with local timezone adjustment
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);  // Use localtime_r instead of gmtime_r for timezone support
  char timeBuffer[9];
  strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);
  
  return String(timeBuffer);
}

// Get network date
String HyperwisorIOT::getNetworkDate() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Error: No WiFi connection.");
    return "";
  }

  // Initialize NTP if not already done
  initNTP();
  
  // Get current time
  time_t now = time(nullptr);
  
  // If time is not synchronized, try again
  if (now < 1000000000L) {
    int retry = 0;
    while (now < 1000000000L && retry < 5) {
      delay(500);
      now = time(nullptr);
      retry++;
    }
    
    if (now < 1000000000L) {
      Serial.println("Error: Failed to get date from NTP server.");
      return "";
    }
  }
  
  // Format date as YYYY-MM-DD with local timezone adjustment
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);  // Use localtime_r instead of gmtime_r for timezone support
  char dateBuffer[11];
  strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d", &timeinfo);
  
  return String(dateBuffer);
}

// Get network date and time
String HyperwisorIOT::getNetworkDateTime() {
  // Check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Error: No WiFi connection.");
    return "";
  }

  // Initialize NTP if not already done
  initNTP();
  
  // Get current time
  time_t now = time(nullptr);
  
  // If time is not synchronized, try again
  if (now < 1000000000L) {
    int retry = 0;
    while (now < 1000000000L && retry < 5) {
      delay(500);
      now = time(nullptr);
      retry++;
    }
    
    if (now < 1000000000L) {
      Serial.println("Error: Failed to get date and time from NTP server.");
      return "";
    }
  }
  
  // Format date and time as YYYY-MM-DD HH:MM:SS with local timezone adjustment
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);  // Use localtime_r instead of gmtime_r for timezone support
  char dateTimeBuffer[20];
  strftime(dateTimeBuffer, sizeof(dateTimeBuffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  
  return String(dateTimeBuffer);
}

// ✅ Find command by name
JsonObject HyperwisorIOT::findCommand(JsonObject& payload, const char* commandName) {
  if (!payload.containsKey("commands")) return JsonObject();
  for (JsonObject commandObj : payload["commands"].as<JsonArray>()) {
    if (strcmp(commandObj["command"], commandName) == 0) {
      return commandObj;
    }
  }
  return JsonObject(); // Not found
}

// ✅ Find action inside a command
JsonObject HyperwisorIOT::findAction(JsonObject& payload, const char* commandName, const char* actionName) {
  JsonObject commandObj = findCommand(payload, commandName);
  if (commandObj.isNull() || !commandObj.containsKey("actions")) return JsonObject();

  for (JsonObject actionObj : commandObj["actions"].as<JsonArray>()) {
    if (strcmp(actionObj["action"], actionName) == 0) {
      return actionObj;
    }
  }
  return JsonObject();
}

// ✅ Get params of a specific action
JsonObject HyperwisorIOT::findParams(JsonObject& payload, const char* commandName, const char* actionName) {
  JsonObject actionObj = findAction(payload, commandName, actionName);
  if (actionObj.isNull() || !actionObj.containsKey("params")) return JsonObject();
  return actionObj["params"].as<JsonObject>();
}
