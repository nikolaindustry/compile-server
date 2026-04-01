# Hyperwisor Compile Server

ESP32 firmware compilation microservice for the [Hyperwisor IIoT Platform](https://www.hyperwisor.com). Accepts Arduino sketch source code via API, compiles it using `arduino-cli`, and returns the compiled `.bin` binary.

**Live URL:** https://hyperwisor-compile-server.onrender.com

## Architecture

```
Client (Hyperwisor Platform / Web UI)
  │
  ├── POST /compile        → Submit compilation job
  ├── GET  /jobs/:id       → Poll job status
  └── GET  /health         → Health check
  │
Compile Server (Node.js + Express)
  │
  ├── In-memory job queue (sequential processing)
  ├── arduino-cli compile (ESP32 core 3.3.7)
  ├── 29 pre-installed Arduino libraries
  ├── On-demand library install (Arduino Registry)
  ├── Custom Git library cloning
  └── Binary upload to Supabase Storage
```

## Features

- **Async Job Queue** — Submit a compile job, get a `jobId`, poll for status. No timeouts.
- **29 Pre-installed Libraries** — Baked into the Docker image at build time. Zero install delay.
- **Additional Libraries** — Install any library from the Arduino Library Registry at compile time.
- **Custom Git Libraries** — Clone any public Git repo containing an Arduino library.
- **Binary Upload** — Compiled `.bin` files uploaded to Supabase Storage for OTA delivery.
- **Web UI** — Built-in React interface at the server root URL. No authentication required.
- **CORS Enabled** — Callable from any frontend origin.

## Pre-installed Libraries

| Library | Description |
|---------|-------------|
| hyperwisor-iot | Hyperwisor IoT abstraction layer |
| ArduinoJson | JSON parsing/serialization |
| WebSockets | WebSocket client/server |
| DHT_sensor_library | DHT temperature/humidity sensors |
| Adafruit_SSD1306 | OLED display driver |
| Adafruit_GFX_Library | Graphics primitives |
| Adafruit_MPU6050 | IMU/accelerometer |
| Adafruit_BusIO | I2C/SPI abstraction |
| Adafruit_PCF8574 | I/O expander |
| Adafruit_Unified_Sensor | Sensor abstraction |
| ESP32Servo | Servo motor control |
| ESP32_Display_Panel | Display panel driver |
| ESP32_IO_Expander | I/O expander driver |
| LiquidCrystal_I2C | LCD display driver |
| ModbusMaster | Modbus RTU master |
| NTPClient | NTP time sync |
| PZEM004Tv30 | Power meter |
| PCF8574_library | I/O expander |
| PCF8575_library | 16-bit I/O expander |
| ArduinoHttpClient | HTTP client |
| BlynkNcpDriver | Blynk NCP driver |
| AnalogWrite_ESP32 | PWM output |
| Servo | Standard servo library |
| SparkFun_Spectral_Triad_AS7265X | Spectral sensor |
| TinyGPS | GPS parsing |
| TinyGPSPlus | GPS parsing (extended) |
| esp-lib-utils | ESP utility functions |
| lvgl | LVGL graphics library |
| NI-EVALUATION_BOARD-V1 | NikolAIndustry eval board |

## API Reference

### `POST /compile`

Submit a compilation job.

**Request Body:**

```json
{
  "source": "#include <WiFi.h>\nvoid setup() {}\nvoid loop() {}",
  "productId": "my-product-id",
  "version": "1.0.0",
  "board": "esp32:esp32:esp32",
  "libraries": ["FastLED", "PubSubClient"],
  "customLibs": ["https://github.com/user/my-library.git"]
}
```

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `source` | `string` | Yes | — | Arduino sketch source code |
| `productId` | `string` | No | `"test"` | Product identifier for storage path |
| `version` | `string` | No | `"1.0.0"` | Firmware version |
| `board` | `string` | No | `"esp32:esp32:esp32"` | Arduino FQBN |
| `libraries` | `string[]` | No | `[]` | Arduino Library Registry names to install |
| `customLibs` | `string[]` | No | `[]` | Git repository URLs to clone as libraries |

**Supported Boards:**

| FQBN | Board |
|------|-------|
| `esp32:esp32:esp32` | ESP32 Dev Module |
| `esp32:esp32:esp32s2` | ESP32-S2 |
| `esp32:esp32:esp32s3` | ESP32-S3 |
| `esp32:esp32:esp32c3` | ESP32-C3 |

**Response:**

```json
{
  "success": true,
  "jobId": "a1b2c3d4-...",
  "status": "queued",
  "pollUrl": "/jobs/a1b2c3d4-..."
}
```

### `GET /jobs/:id`

Poll job status.

**Response (compiling):**

```json
{
  "id": "a1b2c3d4-...",
  "status": "compiling",
  "progress": 40,
  "logs": [
    { "time": "2026-04-01T12:00:00Z", "message": "Starting compilation..." },
    { "time": "2026-04-01T12:00:01Z", "message": "Source code written" }
  ]
}
```

**Response (completed):**

```json
{
  "id": "a1b2c3d4-...",
  "status": "completed",
  "progress": 100,
  "result": {
    "binUrl": "https://xxx.supabase.co/storage/v1/object/public/firmware/...",
    "sizeBytes": 245760,
    "compiledAt": "2026-04-01T12:02:00Z"
  }
}
```

**Response (failed):**

```json
{
  "id": "a1b2c3d4-...",
  "status": "failed",
  "error": "sketch.ino:5:10: fatal error: missing-lib.h: No such file or directory"
}
```

**Job Statuses:** `queued` → `compiling` → `completed` | `failed`

### `GET /jobs`

List recent jobs (last 20).

### `GET /health`

Health check endpoint.

```json
{
  "status": "ok",
  "uptime": 3600,
  "jobs": { "total": 5, "processing": 1 }
}
```

## Local Development

### Prerequisites

- Node.js 20+
- Docker (for full compilation testing)

### Setup

```bash
# Clone the repo
git clone https://github.com/nikolaindustry/compile-server.git
cd compile-server

# Install dependencies
npm install

# Copy environment variables
cp .env.example .env
# Edit .env with your Supabase credentials (optional)

# Start the server
npm start
```

The server starts at `http://localhost:3000`. Open it in your browser to access the Web UI.

> **Note:** Compilation requires `arduino-cli` with ESP32 core installed. The server runs fully only inside Docker. Locally, it starts and serves the UI but compilation will fail without arduino-cli.

### Docker Build (Full Environment)

```bash
docker build -t compile-server .
docker run -p 3000:3000 --env-file .env compile-server
```

## Deployment (Render)

The server is deployed on [Render](https://render.com) using Docker.

**Plan:** Pro (4GB RAM — required for ESP32 core 3.3.7 compilation)

### Environment Variables

Set these in Render Dashboard → Environment:

| Variable | Description |
|----------|-------------|
| `SUPABASE_URL` | Supabase project URL |
| `SUPABASE_SERVICE_KEY` | Supabase service role key |
| `HYPERWISOR_SECRET` | Shared auth secret |
| `PORT` | Set automatically by Render |

### Deploy

Push to `main` branch — Render auto-deploys.

```bash
git push origin main
```

## Project Structure

```
compile-server/
├── server.js              # Express server + async job queue + compile logic
├── package.json           # Node.js dependencies
├── Dockerfile             # Ubuntu 22.04 + arduino-cli + ESP32 core 3.3.7
├── render.yaml            # Render deployment config
├── .env.example           # Environment variable template
├── public/
│   └── index.html         # React Web UI (standalone, no auth)
├── partitions/
│   ├── default.csv        # Default partition table
│   ├── min_spiffs.csv     # Minimal SPIFFS (used for compilation)
│   ├── huge_app.csv       # Large app partition
│   └── no_ota.csv         # No OTA partition
└── libraries/             # 29 Arduino libraries (baked into Docker image)
    ├── hyperwisor-iot/
    ├── ArduinoJson/
    ├── WebSockets/
    └── ... (26 more)
```

## Integration with Hyperwisor Platform

The Hyperwisor IIoT Platform calls this server from the **Code Generator → Compile & Flash** page.

### Example Integration (fetch)

```javascript
// 1. Submit compile job
const res = await fetch('https://hyperwisor-compile-server.onrender.com/compile', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({
    source: sketchCode,
    productId: 'my-product',
    version: '1.0.0',
    board: 'esp32:esp32:esp32',
    libraries: ['DHT sensor library'],
    customLibs: ['https://github.com/user/my-lib.git']
  })
});
const { jobId } = await res.json();

// 2. Poll for completion
const poll = setInterval(async () => {
  const status = await fetch(`https://hyperwisor-compile-server.onrender.com/jobs/${jobId}`);
  const job = await status.json();
  
  if (job.status === 'completed') {
    clearInterval(poll);
    console.log('Binary URL:', job.result.binUrl);
  } else if (job.status === 'failed') {
    clearInterval(poll);
    console.error('Error:', job.error);
  }
}, 2000);
```

## License

Proprietary — NIKOLAINDUSTRY
