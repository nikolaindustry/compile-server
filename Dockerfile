FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# ── System dependencies ────────────────────────────────────────
RUN apt-get update && apt-get install -y \
    curl wget git ca-certificates python3 python3-pip \
    && rm -rf /var/lib/apt/lists/*

# ── Install Python dependencies for ESP32 tools ────────────────
RUN pip3 install pyserial

# ── Install Node.js 20 (LTS) ───────────────────────────────────
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
    apt-get install -y nodejs

# ── Install arduino-cli ────────────────────────────────────────
RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh -o /tmp/install.sh && \
    chmod +x /tmp/install.sh && \
    BINDIR=/usr/local/bin sh /tmp/install.sh && \
    rm /tmp/install.sh

# ── Configure arduino-cli (add ESP32 board manager URL) ───────
RUN arduino-cli config init && \
    arduino-cli config set board_manager.additional_urls \
    https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

# ── Install ESP32 core ─────────────────────────────────────────
# On Render with persistent disk mounted at /root/Arduino,
# the core survives redeploys. First deploy will download ~300MB.
RUN arduino-cli core update-index && \
    arduino-cli core install esp32:esp32@2.0.17

# ── Copy bundled libraries to a safe path (not overwritten by persistent disk)
# The persistent disk mounts at /root/Arduino at runtime, so we store
# bundled libraries in /opt/arduino-libraries and sync them on startup.
COPY libraries/ /opt/arduino-libraries/

# ── Node.js app ───────────────────────────────────────────────
WORKDIR /app
COPY package.json .
RUN npm install --omit=dev
COPY server.js .
COPY partitions/ ./partitions/

# Expose port for the HTTP server
EXPOSE 3000

# Start the Node.js application
CMD ["node", "server.js"]
