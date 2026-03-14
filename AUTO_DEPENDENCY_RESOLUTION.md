# Automatic Library Dependency Resolution

## Overview

The compile server **automatically resolves library dependencies and versions** to prevent common compilation errors. You no longer need to manually specify compatible versions or transitive dependencies.

---

## 🎯 How It Works

### Before (Manual Resolution)
```json
{
  "libraries": [
    "hyperwisor-iot",
    "WebSockets",
    "Ethernet"
  ]
}
```
❌ Would fail with `beginSSL not found` error (wrong WebSockets version)

### After (Automatic Resolution)
```json
{
  "libraries": [
    "hyperwisor-iot",
    "WebSockets"
  ]
}
```
✅ Automatically resolves to:
- `WebSockets@2.3.7` (version with `beginSSL` support)
- `Ethernet` (required dependency)

---

## 📚 Auto-Resolved Libraries

### WebSockets
**Problem:** Different versions have different APIs
- v2.3.x has `beginSSL()` method
- v2.4+ removed `beginSSL()` in favor of auto-detection

**Solution:** Server auto-pins to **v2.3.7**
```javascript
// Your request
"libraries": ["WebSockets"]

// Server installs
"libraries": ["WebSockets@2.3.7", "Ethernet"]
```

### DHT Sensor Library
**Problem:** Requires Adafruit Unified Sensor
```javascript
// Your request
"libraries": ["DHT sensor library"]

// Server installs
"libraries": ["DHT sensor library", "Adafruit Unified Sensor"]
```

### Adafruit SSD1306
**Problem:** Requires Adafruit GFX Library
```javascript
// Your request
"libraries": ["Adafruit SSD1306"]

// Server installs
"libraries": ["Adafruit SSD1306", "Adafruit GFX Library"]
```

---

## 🔧 How to Use

### Simple Case - Just List Libraries
```json
{
  "source": "...",
  "productId": "uuid",
  "libraries": [
    "hyperwisor-iot",
    "WebSockets",
    "DHT sensor library",
    "Adafruit SSD1306"
  ]
}
```

Server will automatically install:
- `hyperwisor-iot` (latest)
- `WebSockets@2.3.7` + `Ethernet`
- `DHT sensor library` + `Adafruit Unified Sensor`
- `Adafruit SSD1306` + `Adafruit GFX Library`

### Override Versions (Advanced)
If you need a specific version:
```json
{
  "libraries": [
    "WebSockets@2.4.0"  // Use your specified version, not auto-pinned one
  ]
}
```

---

## 📊 Logging & Debugging

When a compile request comes in, you'll see:
```
📚 Resolved libraries: WebSockets, DHT sensor library → WebSockets@2.3.7, Ethernet, DHT sensor library, Adafruit Unified Sensor
```

This shows exactly what was requested vs. what's actually installed.

---

## 🗺️ Current Dependency Map

The server maintains this internal map:

```javascript
const LIBRARY_DEPENDENCIES = {
  'WebSockets': {
    version: '2.3.7',           // Auto-pin to this version
    dependencies: ['Ethernet']  // Auto-add these
  },
  'DHT sensor library': {
    dependencies: ['Adafruit Unified Sensor']
  },
  'Adafruit SSD1306': {
    dependencies: ['Adafruit GFX Library']
  }
};
```

---

## ➕ Adding New Auto-Resolutions

If you encounter a library that needs auto-resolution, the system is designed to be easily extended. The dependency map can be updated to include:

1. **Version pinning** - For libraries with breaking changes
2. **Optional dependencies** - Commonly needed companion libraries
3. **Transitive dependencies** - Libraries that depend on other libraries

---

## 🎯 Benefits

### For Developers
✅ No need to research compatible versions  
✅ No need to track transitive dependencies  
✅ Cleaner, simpler compile requests  
✅ Fewer compilation errors  

### For the System
✅ Consistent library versions across builds  
✅ Predictable compilation behavior  
✅ Easier troubleshooting  

---

## 📝 Examples

### Example 1: IoT Project with Sensors
**Request:**
```json
{
  "libraries": [
    "hyperwisor-iot",
    "WebSockets",
    "DHT sensor library"
  ]
}
```

**Actually Installed:**
```
✓ hyperwisor-iot (latest)
✓ WebSockets@2.3.7
✓ Ethernet
✓ DHT sensor library
✓ Adafruit Unified Sensor
```

### Example 2: Display Project
**Request:**
```json
{
  "libraries": [
    "Adafruit SSD1306"
  ]
}
```

**Actually Installed:**
```
✓ Adafruit SSD1306 (latest)
✓ Adafruit GFX Library
```

### Example 3: Custom Version Override
**Request:**
```json
{
  "libraries": [
    "WebSockets@2.5.0"  // Force latest version
  ]
}
```

**Actually Installed:**
```
✓ WebSockets@2.5.0 (your specified version, NOT auto-pinned)
✓ Ethernet (dependency still added)
```

---

## 🔍 Troubleshooting

### Check What's Being Installed
Look at the server logs for:
```
📚 Resolved libraries: WebSockets → WebSockets@2.3.7, Ethernet
```

### Force Specific Version
If auto-resolution doesn't work for your use case:
```json
{
  "libraries": [
    "WebSockets@2.4.0"  // Bypass auto-versioning
  ]
}
```

### Report Missing Dependencies
If a library needs auto-resolution but isn't covered, the system can be extended by updating the `LIBRARY_DEPENDENCIES` map in `server.js`.

---

## 🎉 Summary

**You just list the libraries you need - we handle the rest!**

No more:
- ❌ Researching which version has the API you need
- ❌ Tracking down transitive dependencies
- ❌ Debugging "library not found" errors
- ❌ Version compatibility issues

Just simple, clean compile requests! 🚀
