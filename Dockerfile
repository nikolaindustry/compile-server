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
RUN echo "=== Library directories ===" && \
    ls /root/Arduino/libraries/ && \
    echo "=== hyperwisor-iot contents ===" && \
    ls /root/Arduino/libraries/hyperwisor-iot/src/ && \
    echo "=== arduino-cli config ===" && \
    arduino-cli config dump | grep -A2 directories && \
    echo "=== arduino-cli lib list ===" && \
    arduino-cli lib list

# ── Node.js app ────────────────────────────────────────────────
WORKDIR /app
COPY package.json .
RUN npm install --omit=dev
COPY server.js .
COPY partitions/ ./partitions/
COPY public/ ./public/

EXPOSE 3000

CMD ["node", "server.js"]
