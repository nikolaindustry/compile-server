#ifndef NIKOLAINDUSTRY_REALTIME_H
#define NIKOLAINDUSTRY_REALTIME_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <functional>

class nikolaindustryrealtime
{
public:
  nikolaindustryrealtime();
  void begin(const char *deviceId);
  void loop();
  void sendJson(const JsonObject &json);
  void sendTo(const String &targetId, std::function<void(JsonObject &)> payloadBuilder);

  void setOnMessageCallback(std::function<void(JsonObject &)> callback);
  void setOnConnectionStatusChange(std::function<void(bool)> callback);
  bool isNikolaindustryRealtimeConnected();

private:
  WebSocketsClient webSocket;
  String deviceId;
  bool _isConnected = false;

  std::function<void(JsonObject &)> onMessageCallback;
  std::function<void(bool)> onConnectionStatusChange;

  void connect();
};

#endif
