# Hyperwisor Compile Server

Arduino/ESP32 firmware compilation microservice for the Hyperwisor IoT platform.
Compiles `.ino` source code using `arduino-cli`, uploads the resulting `.bin` to Supabase Storage, and queues OTA flash commands for connected devices.

---

## Project Structure

```
compile-server/
├── Dockerfile                   Docker image (ubuntu + arduino-cli + ESP32 core)
├── render.yaml                  Render.com deploy configuration
├── package.json                 Node.js dependencies
├── server.js                    Express server — all API endpoints
├── .env.example                 Environment variable template
├── .gitignore
└── libraries/                   Arduino libraries baked into Docker image
    ├── hyperwisor-iot/          Main Hyperwisor IoT library
    ├── ArduinoJson/             JSON parsing (ArduinoJson v6)
    ├── nikolaindustry-realtime/ Realtime WebSocket library
    └── <any other libs>/        Copy from local Arduino/libraries/
```

---

## How Libraries Are Managed

### Tier 1 — Bundled (always available, zero install time)
All folders inside `libraries/` are baked into the Docker image.
Copy the entire contents of your local Arduino libraries folder here:
- **Windows:** `C:\Users\<YourName>\Documents\Arduino\libraries\`
- **macOS/Linux:** `~/Arduino/libraries/`

These are available to every compile job with no delay.

### Tier 2 — On-demand (Arduino Library Registry)
Users can request extra libraries per compile job via the `libraries` array in the request body.
The server installs them via `arduino-cli lib install` on first use and caches them in-memory.
They also persist on Render's mounted disk (`/root/Arduino`) across requests.

### Tier 3 — Search
Use `GET /libraries/search?q=<keyword>` to search the Arduino Library Registry.
The frontend can use this endpoint to let users discover and add libraries to their project.

---

## API Reference

All endpoints (except `/health`) require the header:
```
X-Hyperwisor-Token: <HYPERWISOR_SECRET>
```

---

### GET /health
Health check — no auth required. Used by Render's health check system.

**Response:**
```json
{ "status": "ok", "uptime": 42.3, "timestamp": "2026-03-13T10:00:00Z" }
```

---

### GET /libraries/search?q=\<query\>
Search the Arduino Library Registry by keyword.

**Query params:**
- `q` — search keyword e.g. `DHT`, `OLED`, `temperature`

**Response:**
```json
{
  "libraries": [
    {
      "name": "DHT sensor library",
      "version": "1.4.6",
      "author": "Adafruit",
      "description": "Arduino library for DHT11, DHT22 sensors"
    }
  ]
}
```

---

### POST /compile
Compile `.ino` source code for ESP32. Uploads the `.bin` to Supabase Storage and returns the public URL.

**Request body:**
```json
{
  "source": "// full .ino source code string",
  "productId": "uuid-of-product",
  "version": "1.0.3",
  "board": "esp32:esp32:esp32",
  "libraries": [
    "DHT sensor library",
    "Adafruit SSD1306@2.5.7"
  ],
  "partitionScheme": "min_spiffs",
  "flashFreq": "80m",
  "eraseFlash": true
}
```

| Field | Type | Required | Default | Notes |
|-------|------|----------|---------|-------|
| `source` | string | yes | — | Full `.ino` source code |
| `productId` | string | yes | — | Supabase product UUID |
| `version` | string | no | `"1.0.0"` | Used in storage filename |
| `board` | string | no | `esp32:esp32:esp32` | arduino-cli FQBN |
| `libraries` | string[] | no | `[]` | Extra libs to install. Format: `"Name"` or `"Name@version"`. **Note:** Include ALL transitive dependencies (e.g., if using WebSockets, also add "Ethernet" if needed) |
| `partitionScheme` | string | no | `min_spiffs` | Partition scheme: `min_spiffs`, `default`, `huge_app`, `no_ota` |
| `partitionsCsv` | string | no | — | Custom partition CSV (overrides `partitionScheme`) |
| `flashMode` | string | no | `qio` | Flash mode: `qio`, `dio`, `dout`, `fastread` |
| `flashFreq` | string | no | `80m` | Flash frequency: `80m`, `40m` |
| `flashSize` | string | no | `4MB` | Flash size: `4MB`, `2MB`, `16MB` |
| `eraseFlash` | boolean | no | `true` | Erase all flash before compilation (recommended for clean updates) |

**Response 200:**
```json
{
  "success": true,
  "jobId": "uuid",
  "binUrl": "https://xxx.supabase.co/storage/v1/object/public/firmware/...",
  "sizeBytes": 421200,
  "compiledAt": "2026-03-13T10:00:00Z"
}
```

**Response 422 (compile error):**
```json
{
  "success": false,
  "error": "Compilation failed",
  "stderr": "sketch.ino:42: error: 'WiFi' was not declared in this scope"
}
```

**Response 422 (library install error):**
```json
{
  "success": false,
  "error": "Library installation failed",
  "details": ["Failed to install \"UnknownLib\": exit code 1"]
}
```

---

### POST /flash
Queue an OTA firmware update command for a specific device.
The device's WebSocket handler picks this up and triggers the ESP32 `Update.h` OTA flow.

**Request body:**
```json
{
  "deviceId": "uuid-of-device",
  "productId": "uuid-of-product",
  "binUrl": "https://xxx.supabase.co/storage/v1/object/public/firmware/..."
}
```

**Response 200:**
```json
{
  "success": true,
  "message": "OTA command queued — device will flash on next check-in"
}
```

---

## Environment Variables

Copy `.env.example` to `.env` and fill in:

| Variable | Description |
|----------|-------------|
| `HYPERWISOR_SECRET` | Shared secret between this server and the Supabase edge functions. Generate a strong random string. |
| `SUPABASE_URL` | Your Supabase project URL from: Dashboard → Settings → API |
| `SUPABASE_SERVICE_KEY` | Supabase `service_role` key (NOT the anon key). Has full DB + Storage access. |
| `PORT` | Port to listen on. Render sets this automatically. Default: `3000` |

---

## Render Deployment

### First-time setup

1. Push this repo to GitHub
2. Go to [render.com](https://render.com) → New → Web Service
3. Connect this repo
4. Render auto-detects Docker from the `Dockerfile`
5. Set plan to **Starter ($7/mo)** — do NOT use Free (it sleeps)
6. Add a **Persistent Disk**: mount path `/root/Arduino`, size `5GB`
7. Add the 4 environment variables listed above
8. Click **Create Web Service**

First build takes ~15–20 minutes (downloads ESP32 core ~300MB).
Subsequent deploys take ~2–3 minutes.

### Render config (`render.yaml`)
The `render.yaml` in this repo auto-configures the service. You can also use it with `render deploy`.

---

## How It Connects to Hyperwisor

The Hyperwisor main app (Hyperwisor-v4) never calls this server directly.
All calls go through a **Supabase Edge Function** that acts as a secure proxy:

```
Browser (Hyperwisor)
  → POST /functions/v1/compile-firmware   (Supabase edge fn)
    → POST https://your-render-url/compile  (this server)
      → arduino-cli compile
        → .bin uploaded to Supabase Storage
          → binUrl returned back to browser
```

The Render URL and `HYPERWISOR_SECRET` are stored as Supabase Edge Function secrets:
- `COMPILE_SERVER_URL` = your Render service URL
- `HYPERWISOR_COMPILE_SECRET` = same value as `HYPERWISOR_SECRET` on Render

---

## Local Development

```bash
# Install deps
npm install

# Copy and fill env
cp .env.example .env

# Run (requires arduino-cli installed locally)
npm run dev
```

To install arduino-cli locally:
```bash
# macOS
brew install arduino-cli

# Windows
winget install Arduino.ArduinoCLI

# Linux
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
```

After installing:
```bash
arduino-cli config init
arduino-cli config set board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@2.0.17
```

---

## Security Notes

- All library names are validated against `/^[a-zA-Z0-9 _\-\.@]+$/` before being passed to `arduino-cli` (prevents shell injection)
- The `HYPERWISOR_SECRET` header is required on all endpoints except `/health`
- Use `service_role` key for `SUPABASE_SERVICE_KEY` — it bypasses RLS and is needed for Storage uploads
- Never commit `.env` to git
