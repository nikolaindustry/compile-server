FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# ── System dependencies ────────────────────────────────────────
RUN apt-get update && apt-get install -y \
    curl ca-certificates python3 python3-pip \
    && rm -rf /var/lib/apt/lists/*

# ── Install Python dependencies for ESP32 tools ────────────────
RUN pip3 install pyserial

# ── Install Node.js 20 ─────────────────────────────────────────
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
    apt-get install -y nodejs

# ── Install arduino-cli ────────────────────────────────────────
RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh

# ── Configure arduino-cli ─────────────────────────────────────
RUN arduino-cli config init && \
    arduino-cli config set board_manager.additional_urls \
    https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

# ── Install ESP32 core 3.3.7 (matches local Arduino IDE) ───────
# Note: Requires 4GB RAM (Render Pro plan)
RUN arduino-cli core update-index && \
    arduino-cli core install esp32:esp32@3.3.7

# ── Node.js app ───────────────────────────────────────────────
WORKDIR /app
COPY package.json .
RUN npm install --omit=dev

# Copy app files
COPY server.js .
COPY partitions/ ./partitions/
COPY public/ ./public/

EXPOSE 3000

CMD ["node", "server.js"]
