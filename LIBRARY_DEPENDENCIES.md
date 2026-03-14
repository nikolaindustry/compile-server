# Library Dependencies Guide

## Understanding the Error

When you see this compilation error:
```
fatal error: Ethernet.h: No such file or directory
```

It means a library your code depends on is **missing** from the `libraries` array in your compile request.

---

## Common Missing Dependencies

### WebSockets Library
If using **WebSockets**, you may need:
```json
{
  "libraries": [
    "WebSockets",
    "Ethernet"  // ← Required if using Ethernet connection
  ]
}
```

### Display Libraries
If using **Adafruit displays**:
```json
{
  "libraries": [
    "Adafruit SSD1306",
    "Adafruit GFX Library",
    "Adafruit Unified Sensor"  // ← Often needed as dependency
  ]
}
```

### Sensor Libraries
If using **DHT sensors**:
```json
{
  "libraries": [
    "DHT sensor library",
    "Adafruit Unified Sensor"  // ← Required dependency
  ]
}
```

---

## How to Fix Missing Dependencies

### Step 1: Identify the Missing Header
Look at the error message:
```
fatal error: Ethernet.h: No such file or directory
          ^~~~~~~~~~~~
```
The missing header is **Ethernet.h**

### Step 2: Find the Library Name
Common mappings:
- `Ethernet.h` → **"Ethernet"**
- `WiFi.h` → **"WiFi"** (built-in for ESP32)
- `SPI.h` → Built-in (no action needed)
- `Wire.h` → Built-in (no action needed)
- `Adafruit_Sensor.h` → **"Adafruit Unified Sensor"**
- `DHT.h` → **"DHT sensor library"**

### Step 3: Add to Compile Request
```json
{
  "source": "...",
  "productId": "...",
  "libraries": [
    "YourMainLibrary",
    "Ethernet"  // ← Add the missing library
  ]
}
```

---

## Transitive Dependencies

Some libraries depend on other libraries. You must include **ALL** of them:

### Example: Hyperwisor IoT + WebSockets
```json
{
  "libraries": [
    "hyperwisor-iot",      // Your main library
    "WebSockets",          // Dependency of hyperwisor-iot
    "Ethernet"             // Dependency of WebSockets (if used)
  ]
}
```

### Example: OLED Display Project
```json
{
  "libraries": [
    "Adafruit SSD1306",       // Main display library
    "Adafruit GFX Library",   // Required by SSD1306
    "Adafruit Unified Sensor" // Required by GFX
  ]
}
```

---

## Complete Example Request

```json
{
  "source": "#include <WebSocketsClient.h>\nvoid setup() { ... }",
  "productId": "your-product-uuid",
  "version": "1.0.0",
  "board": "esp32:esp32:esp32",
  "libraries": [
    "WebSockets",
    "Ethernet"
  ],
  "partitionScheme": "min_spiffs",
  "flashMode": "qio",
  "flashFreq": "80m",
  "eraseFlash": true
}
```

---

## Troubleshooting Tips

### Tip 1: Check Library Dependencies
Most Arduino library README files list their dependencies. Check the library's documentation.

### Tip 2: Look at examples
Library examples often show all required includes:
```cpp
#include <SPI.h>      // Built-in
#include <Wire.h>     // Built-in
#include <Ethernet.h> // Need to install
#include <WebSocketsClient.h> // Need to install
```

### Tip 3: Install One at a Time
If multiple are missing, add them one by one to identify which causes the issue.

### Tip 4: Use Library Manager
Search for libraries in Arduino IDE or PlatformIO to see their dependencies.

---

## Built-in Libraries (No Installation Needed)

These are included with ESP32 core - **don't** add them to libraries array:
- `SPI.h`
- `Wire.h`
- `WiFi.h`
- `BluetoothSerial.h`
- `ESPAsyncWebServer.h` (if using ESP32 async libraries)
- `Preferences.h`
- `HTTPClient.h`
- `ArduinoJson.h` (if using ArduinoJson library)

---

## Quick Reference Table

| Missing Header | Add This Library |
|----------------|------------------|
| `Ethernet.h` | `"Ethernet"` |
| `WiFi.h` | Built-in (ESP32) |
| `SPI.h` | Built-in |
| `Wire.h` | Built-in |
| `Adafruit_Sensor.h` | `"Adafruit Unified Sensor"` |
| `DHT.h` | `"DHT sensor library"` |
| `Adafruit_GFX.h` | `"Adafruit GFX Library"` |
| `Adafruit_SSD1306.h` | `"Adafruit SSD1306"` |
| `LiquidCrystal_I2C.h` | `"LiquidCrystal I2C"` |
| `Servo.h` | `"ESP32Servo"` (for ESP32) |
| `OneWire.h` | `"OneWire"` |
| `DallasTemperature.h` | `"DallasTemperature"` |

---

## Still Having Issues?

If you're still getting errors after adding libraries:

1. **Check spelling** - Library names are case-sensitive
2. **Check version conflicts** - Some libraries don't work together
3. **Check board compatibility** - Some libraries are board-specific
4. **Clean build** - Set `eraseFlash: true` to force fresh compilation

---

## Example: Complete Working Setup

For a project with:
- Hyperwisor IoT SDK
- WebSockets for real-time communication  
- DHT22 temperature sensor
- SSD1306 OLED display

```json
{
  "source": "...",
  "productId": "uuid",
  "libraries": [
    "hyperwisor-iot",
    "WebSockets",
    "Ethernet",
    "DHT sensor library",
    "Adafruit Unified Sensor",
    "Adafruit SSD1306",
    "Adafruit GFX Library"
  ],
  "partitionScheme": "min_spiffs",
  "flashMode": "qio",
  "flashFreq": "80m"
}
```

This ensures ALL dependencies are satisfied! ✅
