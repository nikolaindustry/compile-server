# Custom Library Upload Guide

## Overview

The compile server now supports **uploading custom libraries** directly in your compile request. This gives you complete control over library versions and allows you to use private or modified libraries.

---

## 🎯 Why Use Custom Libraries?

### Problems Solved:
✅ Use specific library versions not available in Arduino Registry  
✅ Upload private/internal company libraries  
✅ Use modified forks of popular libraries  
✅ Test library changes before publishing  
✅ Include libraries that don't exist in the registry  

---

## 📦 Two Upload Methods

### Method 1: File-by-File Upload (Recommended for Small Libraries)

Upload individual source files with their content:

```json
{
  "customLibraries": [
    {
      "name": "WebSockets",
      "files": [
        {
          "path": "src/WebSockets.h",
          "content": "#ifndef WEBSOCKETS_H\n#define WEBSOCKETS_H\nclass WebSocketsClient {\npublic:\n  void beginSSL(const char* host, uint16_t port, const char* url);\n};\n#endif"
        },
        {
          "path": "src/WebSocketsClient.cpp",
          "content": "#include \"WebSockets.h\"\nvoid WebSocketsClient::beginSSL(const char* host, uint16_t port, const char* url) {\n  // Your custom implementation\n}"
        },
        {
          "path": "library.properties",
          "content": "name=WebSockets\nversion=2.3.7\nauthor=Your Name\nmaintainer=Your Company\nsentence=WebSocket library with SSL support\n"
        }
      ]
    }
  ]
}
```

### Method 2: ZIP Upload (Best for Large Libraries)

Upload a complete library as a base64-encoded ZIP file:

```json
{
  "customLibraries": [
    {
      "name": "WebSockets",
      "zipBase64": "UEsDBBQAAAAIAAAhAAAAAAAAAAAAAAAAAAAAAAA..."
    }
  ]
}
```

---

## 🔧 Complete Example: Fix WebSockets beginSSL Issue

### Step 1: Prepare Your Library

Download WebSockets v2.3.7 (the version with `beginSSL`):
```bash
wget https://github.com/Links2004/ArduinoWebSockets/archive/refs/tags/2.3.7.zip
unzip 2.3.7.zip
```

### Step 2: Convert to Base64
```bash
base64 -w 0 ArduinoWebSockets-2.3.7.zip > websocket-base64.txt
```

### Step 3: Send Compile Request

```javascript
const fs = require('fs');

const zipContent = fs.readFileSync('./ArduinoWebSockets-2.3.7.zip', 'base64');

const response = await fetch('https://hyperwisor-compile-server.onrender.com/compile', {
  method: 'POST',
  headers: {
    'Content-Type': 'application/json',
    'X-Hyperwisor-Token': 'YOUR_SECRET'
  },
  body: JSON.stringify({
    source: `
      #include <WebSocketsClient.h>
      
      void setup() {
        Serial.begin(115200);
        webSocket.beginSSL("nikolaindustry-realtime.onrender.com", 443, "/?id=device123");
      }
      
      void loop() {}
    `,
    productId: "your-product-uuid",
    version: "1.0.0",
    customLibraries: [
      {
        name: "WebSockets",
        zipBase64: zipContent
      }
    ],
    partitionScheme: "min_spiffs",
    flashMode: "qio",
    flashFreq: "80m",
    eraseFlash: true
  })
});

const result = await response.json();
console.log(result);
// Should return: { success: true, binUrl: "...", ... }
```

---

## 📋 API Reference

### Custom Library Object Structure

#### Option A: File-Based
```typescript
{
  name: string;           // Library name (must match #include directives)
  files: Array<{
    path: string;         // Relative path within library
    content: string;      // File content as string
  }>;
}
```

#### Option B: ZIP-Based
```typescript
{
  name: string;           // Library name
  zipBase64: string;      // Base64-encoded ZIP file content
}
```

### Complete Compile Request Schema

```json
{
  "source": "string (required)",
  "productId": "string (required)",
  "version": "string (optional, default: 1.0.0)",
  "board": "string (optional, default: esp32:esp32:esp32)",
  "libraries": ["string array (optional)"],
  "customLibraries": [
    {
      "name": "string",
      "files": [{"path": "string", "content": "string"}],
      // OR
      "zipBase64": "string"
    }
  ],
  "partitionScheme": "string (optional, default: min_spiffs)",
  "partitionsCsv": "string (optional)",
  "flashMode": "string (optional, default: qio)",
  "flashFreq": "string (optional, default: 80m)",
  "flashSize": "string (optional, default: 4MB)",
  "eraseFlash": "boolean (optional, default: true)"
}
```

---

## 🎨 Usage Examples

### Example 1: Private Company Library

```json
{
  "source": "#include <MyCompanyIoT.h>\nvoid setup() { MyCompanyIoT.begin(); }",
  "productId": "product-uuid",
  "customLibraries": [
    {
      "name": "MyCompanyIoT",
      "files": [
        {
          "path": "MyCompanyIoT.h",
          "content": "// Your proprietary header"
        },
        {
          "path": "MyCompanyIoT.cpp",
          "content": "// Your proprietary implementation"
        },
        {
          "path": "library.properties",
          "content": "name=MyCompanyIoT\nversion=1.0.0\n"
        }
      ]
    }
  ]
}
```

### Example 2: Modified Library Fork

```json
{
  "customLibraries": [
    {
      "name": "DHT sensor library",
      "files": [
        {
          "path": "DHT.h",
          "content": "// Your modified version with custom features"
        },
        // ... other files
      ]
    }
  ]
}
```

### Example 3: Multiple Custom Libraries

```json
{
  "customLibraries": [
    {
      "name": "WebSockets",
      "zipBase64: "..."
    },
    {
      "name": "CustomLogger",
      "files": [
        {"path": "CustomLogger.h", "content": "..."},
        {"path": "CustomLogger.cpp", "content": "..."}
      ]
    }
  ]
}
```

---

## 🛠️ Best Practices

### 1. **Library Structure**
Follow Arduino library structure:
```
LibraryName/
├── src/
│   ├── LibraryName.h
│   └── LibraryName.cpp
├── library.properties
└── README.md
```

### 2. **library.properties Format**
Always include a proper `library.properties`:
```properties
name=LibraryName
version=1.0.0
author=Your Name
maintainer=Your Email
sentence=Brief description
paragraph=Longer description
category=Communication
url=https://github.com/you/library
architectures=esp32
```

### 3. **File Paths**
Use relative paths from library root:
```json
{
  "path": "src/LibraryName.h",     // ✅ Correct
  "path": "LibraryName.h",         // ❌ Wrong (should be in src/)
}
```

### 4. **ZIP Encoding**
For base64 encoding:
```bash
# Linux/Mac
base64 -w 0 LibraryName.zip

# Windows PowerShell
[Convert]::ToBase64String([IO.File]::ReadAllBytes("LibraryName.zip"))
```

---

## 🔍 Troubleshooting

### Error: "No such file or directory"
**Cause:** Incorrect file paths in custom library  
**Fix:** Ensure paths are relative and include `src/` directory

### Error: "Library not found"
**Cause:** Library name doesn't match folder name  
**Fix:** Make sure `name` matches the folder structure

### Compilation Still Fails with beginSSL
**Cause:** Custom library not being used  
**Fix:** Check logs to verify custom library was uploaded before auto-installed version

---

## 📊 How It Works

### Server Processing Flow:

1. **Receive Request** → Parse `customLibraries` array
2. **Create Temp Directory** → `/tmp/job_xxx/libraries/`
3. **Extract Custom Libraries**:
   - If ZIP: Decode base64, extract files
   - If files: Create directory structure
4. **Install Auto-Dependencies** → Install from Arduino Registry
5. **Compile** → Arduino CLI finds custom libraries first
6. **Cleanup** → Remove temp directories

### Priority Order:
```
1. Custom libraries (from upload)     ← Highest priority
2. Bundled libraries (/opt/arduino-libraries)
3. Arduino Registry libraries          ← Lowest priority
```

---

## 🚀 Advanced Features

### Coming Soon:
- [ ] ZIP extraction with `adm-zip` library
- [ ] Git repository URL support
- [ ] Library version caching
- [ ] Private library registry integration
- [ ] Library dependency auto-resolution

---

## 💡 Pro Tips

### Tip 1: Test Locally First
Before uploading, test your custom library locally:
```bash
arduino-cli compile --libraries ./MyLibrary sketch.ino
```

### Tip 2: Keep Libraries Small
For file-based uploads, minimize file count:
- Remove examples/
- Remove tests/
- Remove unused source files

### Tip 3: Use Version Control
Tag your library releases:
```json
{
  "name": "WebSockets",
  "files": [...],
  "_metadata": {
    "version": "2.3.7-custom",
    "git_commit": "abc123",
    "modified": "2024-01-15"
  }
}
```

---

## 🎉 Summary

**Custom library upload gives you:**
- ✅ Full control over library versions
- ✅ Ability to use private/modified libraries
- ✅ Solution to compatibility issues
- ✅ No need to publish to Arduino Registry
- ✅ Test libraries before releasing

**Two easy methods:**
1. **File-by-file** - Great for small libraries or quick fixes
2. **ZIP upload** - Perfect for complete library packages

Start using custom libraries today and never worry about version conflicts again! 🚀
