#include <hyperwisor-iot.h>
#include <PCF8574.h>
#include <Wire.h>

HyperwisorIOT hyper;
PCF8574 pcf8574_relays(0x3C);

String deviceId = "";
String userId = "";
String msgfrom = "";

void setup() {
  Serial.begin(115200);
  Wire.begin(4, 5);
  pcf8574_relays.begin();
  pcf8574_relays.pinMode(P3, OUTPUT);
  pcf8574_relays.pinMode(P2, OUTPUT);

  // API key and secret key
  hyper.setApiKeys("mk_4a9c4f312...", "msk_b78602...");

  // Time and date functions
  hyper.setTimezone("IST");

  // Core functions
  hyper.begin();

  // Device info
  deviceId = hyper.getDeviceId();
  userId = hyper.getUserId();
  
  // User-defined command callback
  hyper.setUserCommandHandler([](JsonObject& msg) {
    if (!msg.containsKey("payload")) return;

    msgfrom = msg["from"].as<String>();
    Serial.print("From: ");
    Serial.println(msgfrom);

    JsonObject payload = msg["payload"];
    Serial.println("Full payload:");
    serializeJson(payload, Serial);
    Serial.println();

    // Handle "get_ui" if exists
    if (payload.containsKey("command")) {
      String command = payload["command"].as<String>();
      Serial.print("Command: ");
      Serial.println(command);
      if (command == "get_ui") {
        // Handle dashboard update logic
        return;
      }
    }

    // Handle relay control commands
    JsonObject controlRelay = hyper.findCommand(payload, "Control_Relay");
    if (controlRelay.isNull()) {
      Serial.println("⚠️ Control_Relay command not found");
      return;
    }

    Serial.println("✅ Control_Relay command found");

    // Check Relay_1_ON
    JsonObject relay1On = hyper.findAction(payload, "Control_Relay", "Relay_1_ON");
    if (!relay1On.isNull()) {
      String state = relay1On["params"]["state"] | "N/A";
      Serial.print("Relay_1_ON state: ");
      Serial.println(state);
      if (state == "high") {
        pcf8574_relays.digitalWrite(P3, LOW);
      }
    }

    // Check Relay_1_OFF
    JsonObject relay1Off = hyper.findAction(payload, "Control_Relay", "Relay_1_OFF");
    if (!relay1Off.isNull()) {
      String state = relay1Off["params"]["state"] | "N/A";
      Serial.print("Relay_1_OFF state: ");
      Serial.println(state);
      if (state == "low") {
        pcf8574_relays.digitalWrite(P3, HIGH);
      }
    }

    // Check Relay_2_ON
    JsonObject relay2On = hyper.findAction(payload, "Control_Relay", "Relay_2_ON");
    if (!relay2On.isNull()) {
      String state = relay2On["params"]["state"] | "N/A";
      Serial.print("Relay_2_ON state: ");
      Serial.println(state);
      if (state == "high") {
        pcf8574_relays.digitalWrite(P2, LOW);
      }
    }

    // Check Relay_2_OFF
    JsonObject relay2Off = hyper.findAction(payload, "Control_Relay", "Relay_2_OFF");
    if (!relay2Off.isNull()) {
      String state = relay2Off["params"]["state"] | "N/A";
      Serial.print("Relay_2_OFF state: ");
      Serial.println(state);
      if (state == "low") {
        pcf8574_relays.digitalWrite(P2, HIGH);
      }
    }
  });
}

void loop() {
  // Core functions
  hyper.loop();
  delay(50);
}
