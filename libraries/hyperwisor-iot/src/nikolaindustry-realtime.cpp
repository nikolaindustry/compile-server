#include "nikolaindustry-realtime.h"

nikolaindustryrealtime::nikolaindustryrealtime() {}

void nikolaindustryrealtime::begin(const char *_deviceId)
{
  deviceId = _deviceId;

  if (WiFi.status() == WL_CONNECTED)
  {
    connect();
  }
  else
  {
    Serial.println("❌ WiFi not connected. WebSocket not started.");
  }
}

void nikolaindustryrealtime::connect()
{
  Serial.println("🔄 Connecting to nikolaindustry-realtime...");
  
  webSocket.beginSSL("nikolaindustry-realtime.onrender.com", 443, ("/?id=" + deviceId).c_str());

  webSocket.onEvent([this](WStype_t type, uint8_t *payload, size_t length)
                    {
    DynamicJsonDocument doc(2048);
    switch (type) {
      case WStype_CONNECTED:
        Serial.println("🟢 nikolaindustry-realtime connected");
        _isConnected = true;
        if (onConnectionStatusChange) onConnectionStatusChange(true);
        break;
      case WStype_DISCONNECTED:
        Serial.println("🔴 nikolaindustry-realtime disconnected");
        _isConnected = false;
        if (onConnectionStatusChange) onConnectionStatusChange(false);
        break;
      case WStype_TEXT:
        if (!deserializeJson(doc, payload, length) && onMessageCallback) {
          JsonObject obj = doc.as<JsonObject>();
          onMessageCallback(obj);
        }
        break;
      case WStype_PING:
        // Ping received from server, pong will be sent automatically
        Serial.printf("📡 [%lu] PING received from server\n", millis());
        break;
      case WStype_PONG:
        // Pong received - connection is verified alive
        Serial.printf("📡 [%lu] PONG received - connection alive ✓\n", millis());
        break;
      case WStype_ERROR:
        Serial.println("❌ WebSocket error occurred");
        _isConnected = false;
        break;
      default:
        break;
    } });

  // Set reconnect interval for automatic reconnection attempts
  webSocket.setReconnectInterval(5000);
  
  // CRITICAL: Enable heartbeat to detect zombie connections
  // Ping every 15 seconds, timeout after 3 seconds, disconnect after 2 failed pongs
  webSocket.enableHeartbeat(15000, 3000, 2);
}

void nikolaindustryrealtime::loop()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    webSocket.loop();
  }
}

void nikolaindustryrealtime::sendJson(const JsonObject &json)
{
  String output;
  if (serializeJson(json, output))
  {
    webSocket.sendTXT(output);
  }
  else
  {
    Serial.println("❌ Failed to serialize JSON!");
  }
}

void nikolaindustryrealtime::sendTo(const String &targetId, std::function<void(JsonObject &)> payloadBuilder)
{
  DynamicJsonDocument doc(512);
  doc["targetId"] = targetId;
  JsonObject payload = doc.createNestedObject("payload");
  payloadBuilder(payload);
  sendJson(doc.as<JsonObject>());
}

void nikolaindustryrealtime::setOnMessageCallback(std::function<void(JsonObject &)> callback)
{
  onMessageCallback = callback;
}

void nikolaindustryrealtime::setOnConnectionStatusChange(std::function<void(bool)> callback)
{
  onConnectionStatusChange = callback;
}

bool nikolaindustryrealtime::isNikolaindustryRealtimeConnected()
{
  return webSocket.isConnected();
}
