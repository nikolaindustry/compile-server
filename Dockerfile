FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# ── System dependencies ────────────────────────────────────────
RUN apt-get update && apt-get install -y \
    curl ca-certificates python3 python3-pip \
    && rm -rf /var/lib/apt/lists/*

# ── Python dependencies for ESP32 tools ────────────────────────
RUN pip3 install pyserial

# ── Node.js 20 ─────────────────────────────────────────────────
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - && \
    apt-get install -y nodejs

# ── arduino-cli ────────────────────────────────────────────────
RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh

# ── Configure arduino-cli ──────────────────────────────────────
RUN arduino-cli config init && \
    arduino-cli config set board_manager.additional_urls \
    https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json && \
    arduino-cli config set directories.user /root/Arduino

# ── Install ESP32 core 3.3.7 ───────────────────────────────────
RUN arduino-cli core update-index && \
    arduino-cli core install esp32:esp32@3.3.7

# ── Copy ALL libraries (baked in at build time) ────────────────
# This matches the exact local Arduino IDE environment
COPY libraries/ /root/Arduino/libraries/

# ── Verify libraries are in place ─────────────────────────────
RUN echo "========== DEBUG: Full arduino-cli config ==========" && \
    arduino-cli config dump && \
    echo "" && \
    echo "========== DEBUG: /root/Arduino/ tree ==========" && \
    find /root/Arduino/libraries -maxdepth 2 -type f -name '*.h' | head -50 && \
    echo "" && \
    echo "========== DEBUG: hyperwisor-iot full tree ==========" && \
    find /root/Arduino/libraries/hyperwisor-iot -type f && \
    echo "" && \
    echo "========== DEBUG: library.properties ==========" && \
    cat /root/Arduino/libraries/hyperwisor-iot/library.properties && \
    echo "" && \
    echo "========== DEBUG: hyperwisor-iot.h first 5 lines ==========" && \
    head -5 /root/Arduino/libraries/hyperwisor-iot/src/hyperwisor-iot.h && \
    echo "" && \
    echo "========== DEBUG: arduino-cli lib list ==========" && \
    arduino-cli lib list && \
    echo "" && \
    echo "========== DEBUG: all library.properties files ==========" && \
    find /root/Arduino/libraries -maxdepth 2 -name 'library.properties' -exec echo '--- {} ---' \; -exec head -2 {} \;

# ── Node.js app ────────────────────────────────────────────────
WORKDIR /app
COPY package.json .
RUN npm install --omit=dev
COPY server.js .
COPY partitions/ ./partitions/
COPY public/ ./public/

EXPOSE 3000

CMD ["node", "server.js"]
