# Hydrophonics Controller - Offline Scheduling & Timers

This example demonstrates **Hyperwisor's powerful offline automation capabilities**, allowing your ESP32 device to control relays autonomously without requiring constant cloud connectivity. Perfect for hydroponics systems, aquariums, grow rooms, and any time-critical automation that must work even when WiFi is down.

## 🔥 Key Features

### 1. **Offline Time-of-Day Scheduling** (NTP-Based)
- Schedule relays to activate at specific times using NTP network time
- No RTC module required - uses ESP32 internal clock synced via NTP
- Survives WiFi outages - schedules continue running offline
- Daily recurring schedules with start/end times
- Multiple schedule modes:
  - `RELAY_ON` - Force relay ON during scheduled window
  - `RELAY_OFF` - Force relay OFF during scheduled window
  - `THERMOSTAT` - Activate temperature control with custom settings
  - `TIMER` - Activate looping timer with custom intervals

### 2. **Looping Timers**
- Create repeating ON/OFF cycles (e.g., 2 minutes ON, 10 minutes OFF)
- Runs completely offline once configured
- Automatically toggles relays based on elapsed time
- Perfect for water pumps, aerators, misting systems

### 3. **Local Thermostat Control**
- Temperature-based relay control using DS18B20 sensors
- Heating or cooling modes
- Configurable hysteresis (deadband)
- Works offline - no cloud needed for temperature decisions

### 4. **Persistent Memory (NVS)**
- All settings saved to ESP32 flash memory
- Survives power cycles and reboots
- Configuration persists even when WiFi is unavailable

## 🎯 What This Demonstrates

This hydrophonics controller showcases Hyperwisor's ability to run **completely autonomous automation**:

✅ **Offline Operation**: Once configured, all automations run locally on the ESP32  
✅ **No RTC Module Needed**: Uses NTP time sync + ESP32 internal clock  
✅ **Hybrid Control**: Cloud commands + local automation coexist seamlessly  
✅ **Safety Systems**: Temperature monitoring prevents equipment damage  
✅ **Flexible Scheduling**: Multiple schedule types for different needs  

## 📋 Hardware Requirements

- **ESP32 Development Board** (any variant with WiFi)
- **8-Channel Relay Module** (active-low or active-high supported)
- **DS18B20 Temperature Sensors** (optional, up to 8 sensors)
- **4.7kΩ Resistor** (for DS18B20 data line pull-up)

### Example Pin Configuration (Customizable)
```cpp
#define ONE_WIRE_PIN 2           // DS18B20 data pin
const int RELAY_PINS[] = {32, 33, 25, 26, 27, 4, 16, 17};  // 8 relays
const bool ACTIVE_LOW_RELAYS = true;  // Most relay modules are active-low
```

## 🚀 How It Works

### Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                    ESP32 Controller                      │
│                                                          │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │   Schedules  │  │    Timers    │  │  Thermostat  │  │
│  │  (NTP Time)  │  │ (millis())   │  │  (DS18B20)   │  │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  │
│         │                 │                  │          │
│         └────────────┬────┴──────────────────┘          │
│                      │                                   │
│              ┌───────▼────────┐                         │
│              │ Relay Control  │                         │
│              │   (8 Channels) │                         │
│              └────────────────┘                         │
│                                                          │
│  ┌──────────────────────────────────────────────────┐   │
│  │        NVS Persistent Memory Storage             │   │
│  │  • Widget IDs  • Timer Settings  • Schedules     │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
                            ▲
                            │ WiFi (Optional)
                            ▼
                  ┌─────────────────┐
                  │  Hyperwisor     │
                  │     Cloud       │
                  │  (Configuration)│
                  └─────────────────┘
```

### Automation Priority System

When multiple automations are configured, the system uses this priority:

1. **Manual Cloud Commands** (highest priority) - Disables all automations
2. **Active Time Schedule** - Overrides thermostats/timers during window
3. **Thermostat Control** - Continuous temperature monitoring
4. **Looping Timer** - Background timing cycles

## 💡 Usage Examples

### Example 1: Water Pump Cycling (Timer Mode)
Run a water pump for 2 minutes, then rest for 10 minutes, continuously:

**Cloud Command:**
```json
{
  "to": "device_id",
  "payload": {
    "command": "SET_TIMER",
    "actions": [
      {
        "action": "SET",
        "params": {
          "relay": 0,
          "active": true,
          "onMinutes": 2,
          "offMinutes": 10
        }
      }
    ]
  }
}
```

### Example 2: Daily Light Schedule (Time-of-Day)
Turn grow lights ON from 6:00 AM to 8:00 PM daily:

**Cloud Command:**
```json
{
  "to": "device_id",
  "payload": {
    "command": "SET_SCHEDULE",
    "actions": [
      {
        "action": "SET",
        "params": {
          "relay": 1,
          "active": true,
          "startHour": 6,
          "startMin": 0,
          "endHour": 20,
          "endMin": 0,
          "mode": "RELAY_ON"
        }
      }
    ]
  }
}
```

### Example 3: Temperature-Controlled Heater (Thermostat)
Maintain water temperature between 24-26°C using heater on relay 2:

**Cloud Command:**
```json
{
  "to": "device_id",
  "payload": {
    "command": "SET_THERMOSTAT",
    "actions": [
      {
        "action": "SET",
        "params": {
          "relay": 2,
          "active": true,
          "sensorIndex": 0,
          "targetTemp": 25.0,
          "hysteresis": 1.0,
          "mode": "HEATING"
        }
      }
    ]
  }
}
```

### Example 4: Complex Schedule (Thermostat + Timer Hybrid)
During daytime hours (8 AM - 6 PM), run water circulation pump in timed cycles:

**Cloud Command:**
```json
{
  "to": "device_id",
  "payload": {
    "command": "SET_SCHEDULE",
    "actions": [
      {
        "action": "SET",
        "params": {
          "relay": 3,
          "active": true,
          "startHour": 8,
          "startMin": 0,
          "endHour": 18,
          "endMin": 0,
          "mode": "TIMER",
          "onMinutes": 5,
          "offMinutes": 15
        }
      }
    ]
  }
}
```

## 🔧 Configuration

### NVS Storage Keys

All settings are automatically saved to NVS with these keys:

| Setting Type | NVS Key Pattern | Data Type |
|-------------|----------------|-----------|
| Relay Widget ID | `rw_{index}` | String |
| Temp Widget ID | `tw_{index}` | String |
| Thermostat Active | `tcA_{index}` | Bool |
| Thermostat Sensor | `tcS_{index}` | Int |
| Thermostat Target | `tcT_{index}` | Float |
| Thermostat Hysteresis | `tcH_{index}` | Float |
| Thermostat Mode | `tcM_{index}` | Bool |
| Timer Active | `tmA_{index}` | Bool |
| Timer ON Duration | `tmOn_{index}` | UInt (ms) |
| Timer OFF Duration | `tmOff_{index}` | UInt (ms) |
| Schedule Active | `scA_{index}` | Bool |
| Schedule Start Hour | `scSH_{index}` | UChar |
| Schedule Start Min | `scSM_{index}` | UChar |
| Schedule End Hour | `scEH_{index}` | UChar |
| Schedule End Min | `scEM_{index}` | UChar |
| Schedule Mode | `scMd_{index}` | String |
| Schedule Temp Params | `scTT_{index}`, etc. | Various |

*(Replace `{index}` with relay number 0-7)*

## 📊 Monitoring & Debugging

### Get Current Settings Report

Request a full report of all automation settings:

```json
{
  "to": "device_id",
  "payload": {
    "command": "GET_SETTINGS"
  }
}
```

**Response:**
```json
{
  "status": "success",
  "type": "SETTINGS_REPORT",
  "rules": [
    {
      "relay": 0,
      "thermostat": {
        "active": false,
        "sensorIndex": -1,
        "targetTemp": 25.0,
        "hysteresis": 1.0,
        "mode": "HEATING"
      },
      "timer": {
        "active": true,
        "onMinutes": 2.0,
        "offMinutes": 10.0
      },
      "schedule": {
        "active": true,
        "start": "06:00",
        "end": "20:00",
        "mode": "RELAY_ON"
      }
    }
    // ... more relays
  ]
}
```

### Serial Debug Output

The example includes extensive serial debugging:

```
[Timer] Checking Relay 0...
  -> Phase: ON | Elapsed: 45s | Target: 120s | Remaining: 75s
  -> ACTION: Waiting for ON phase to complete.

[Schedule] Checking Clock... Current Time is 14:30
  -> Relay 1 is scheduled from 06:00 to 20:00 (Mode: RELAY_ON)
  -> ACTION: ⏰ Time window active! Injecting Schedule logic.
  -> Result: Forced Relay ON.

[Thermostat] Checking Relay 2...
  -> Temp: 24.5C | Target: 25.0C | Hysteresis: 1.0C
  -> Mode: HEATING | Current State: OFF
  -> ACTION: Temperature is in the safe zone. No changes.
```

## ⚙️ Advanced Features

### Boot-Time Schedule Enforcement

The system includes intelligent boot logic to handle schedules that were active during power loss:

- On boot, checks current time against all schedules
- If within a scheduled window, immediately activates the appropriate automation
- Prevents "ghost states" where relays should have been ON but weren't
- Critical for life-support systems (aquariums, hydroponics)

### Safety Override System

Manual cloud commands instantly disable all local automations:

```cpp
// When user manually controls relay via cloud:
thermostats[channel].active = false;
timers[channel].active = false;
schedules[channel].active = false;
setRelay(channel, newState);
```

This ensures:
- Emergency shutdown capability
- Manual override always works
- User has final control authority

### Dynamic Widget Mapping

Widget IDs are dynamically assigned and persisted:

- No hardcoded widget dependencies
- Cloud UI can be reconfigured without reflashing
- Each relay/sensor can have multiple widget associations
- Mappings survive power cycles via NVS storage

## 🛠️ Troubleshooting

### Issue: Schedules not executing at correct times

**Solution:** Verify NTP time sync is working:
```cpp
struct tm timeinfo;
if (!getLocalTime(&timeinfo)) {
  Serial.println("NTP sync failed!");
  // Check WiFi connection
  // Verify timezone setting: device.setTimezone("IST");
}
```

### Issue: Timer not cycling

**Solution:** Check timer activation state:
- Ensure `timers[relay].active = true`
- Verify `onTimeMs` and `offTimeMs` are set correctly
- Timer requires 1 second loop interval minimum

### Issue: Thermostat not responding

**Solution:** 
- Verify DS18B20 sensor is detected (`sensorCount > 0`)
- Check `sensorIndex` matches actual sensor position
- Ensure hysteresis isn't too wide (prevents switching)
- Verify relay isn't being overridden by schedule

## 🎓 Best Practices

1. **Always test schedules with current time** before deploying
2. **Use appropriate hysteresis** (0.5-2.0°C) to prevent rapid cycling
3. **Set reasonable timer intervals** - don't cycle relays too fast
4. **Monitor NVS wear** - avoid excessive config saves (>10k writes)
5. **Implement watchdog timers** for critical applications
6. **Add manual override buttons** in your cloud UI
7. **Log temperature data** to detect sensor failures early

## 📝 Customization Tips

### Adding More Relays
```cpp
const int NUM_RELAYS = 16;  // Update relay count
const int RELAY_PINS[] = {32, 33, 25, 26, 27, 4, 16, 17, ...};
String RELAY_WIDGET_IDS[NUM_RELAYS] = {...};
```

### Changing Automation Logic
Modify `evaluateSchedules()`, `evaluateTimers()`, or `evaluateThermostats()` functions to implement custom behavior.

### Adding New Schedule Modes
Extend the `DailySchedule` struct and add handling in `evaluateSchedules()`:
```cpp
else if (schedules[i].mode == "CUSTOM_MODE") {
  // Your custom logic here
}
```

## 🔗 Related Examples

- **BasicSetup** - Minimal Hyperwisor configuration
- **GPIOControl** - Direct GPIO pin manipulation
- **IoTRelayController** - Basic relay control without automation
- **Conditional_Provisioning_Example** - Advanced WiFi provisioning

## 📚 Further Reading

- [Hyperwisor Core Concepts](../../README.md#core-concepts)
- [NTP Time Sync Documentation](https://github.com/nikolaindustry/hyperwisor-iot-arduino-library/wiki/Advanced-Features#ntp-time-sync)
- [NVS Storage Best Practices](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html)

---

**Created with Hyperwisor IoT Library**  
*Making offline automation simple, reliable, and cloud-ready*
