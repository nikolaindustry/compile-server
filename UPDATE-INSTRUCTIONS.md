# Compile & Flash UI Update — Custom Git Library Support

## Overview

Add a "Custom Git Libraries" input field to the **Code Generator → Compile & Flash** page so users can provide Git repository URLs containing custom Arduino libraries for compilation.

---

## Current State

The Compile & Flash page currently sends this payload to the compile server:

```json
{
  "source": "<generated sketch code>",
  "productId": "<product-id>",
  "version": "1.0.0",
  "board": "esp32:esp32:esp32",
  "libraries": ["DHT sensor library", "Adafruit SSD1306"]
}
```

## Required Change

Add a new `customLibs` field to the payload:

```json
{
  "source": "<generated sketch code>",
  "productId": "<product-id>",
  "version": "1.0.0",
  "board": "esp32:esp32:esp32",
  "libraries": ["DHT sensor library", "Adafruit SSD1306"],
  "customLibs": ["https://github.com/user/my-library.git"]
}
```

---

## UI Change

Add a new text input field in the **BUILD SETTINGS** section, positioned below the existing "Extra Libraries" field and above the "Compile Firmware" button.

```
┌─────────────────────────────────────────────────────┐
│ BUILD SETTINGS                                      │
│                                                     │
│ Version          Board FQBN                         │
│ [1.0.0]          [esp32:esp32:esp32]                │
│                                                     │
│ Extra Libraries (comma separated)                   │
│ [DHT sensor library, Adafruit SSD1306           ]   │
│                                                     │
│ Custom Git Libraries (comma separated URLs)  ← NEW  │
│ [https://github.com/user/my-library.git      ]   │  │
│                                                     │
│ [         ⊕  Compile Firmware                     ] │
└─────────────────────────────────────────────────────┘
```

**Field Details:**

| Property    | Value |
|-------------|-------|
| Label       | `Custom Git Libraries (comma separated URLs, optional)` |
| Placeholder | `https://github.com/user/my-library.git` |
| Type        | Text input |
| Required    | No |

---

## Code Changes

### 1. Add State Variable

```javascript
const [customLibs, setCustomLibs] = useState('');
```

### 2. Add Input Field (JSX)

Place this after the existing "Extra Libraries" input:

```jsx
<div className="form-group">
  <label>Custom Git Libraries (comma separated URLs, optional)</label>
  <input
    type="text"
    value={customLibs}
    onChange={(e) => setCustomLibs(e.target.value)}
    placeholder="https://github.com/user/my-library.git"
  />
</div>
```

### 3. Update API Call

Find the function that calls the compile server API and add `customLibs` to the request body:

```javascript
// In the compile/submit function, update the fetch body:
const body = {
  source: generatedCode,
  productId: productId,
  version: version,
  board: boardFqbn,
  libraries: extraLibraries.split(',').map(l => l.trim()).filter(Boolean),
  // ADD THIS LINE:
  customLibs: customLibs.split(',').map(l => l.trim()).filter(Boolean)
};
```

---

## Validation (Optional)

```javascript
function isValidGitUrl(url) {
  return /^https?:\/\/.+\/.+/.test(url);
}

// Before submitting:
const gitLibs = customLibs.split(',').map(l => l.trim()).filter(Boolean);
const invalid = gitLibs.filter(url => !isValidGitUrl(url));
if (invalid.length > 0) {
  showError(`Invalid Git URL(s): ${invalid.join(', ')}`);
  return;
}
```

---

## How It Works on the Server

1. Server receives `customLibs` array of Git URLs
2. Each URL is cloned: `git clone --depth 1 <url>`
3. Cloned repos are passed to `arduino-cli compile` via `--libraries` flag
4. The sketch can `#include` any header from the cloned repos
5. Temp files are cleaned up after compilation

---

## Testing

1. Open Compile & Flash page
2. Leave "Custom Git Libraries" empty → should compile as before (no regression)
3. Enter `https://github.com/nikolaindustry/hyperwisor-iot-arduino-library.git` → sketch using `#include <hyperwisor-iot.h>` should compile
4. Enter an invalid URL → should show warning in logs, compilation continues without that library
5. Enter multiple URLs separated by commas → all should be cloned
