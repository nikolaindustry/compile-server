FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# ── System dependencies ────────────────────────────────────────
RUN apt-get update && apt-get install -y \
    curl wget git ca-certificates \
    && rm -rf /var/lib/apt/lists/*

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

# ── Copy ALL local Arduino libraries (the entire libraries/ folder)
# This includes hyperwisor-iot and ALL its dependencies.
# Just paste your local Arduino/libraries folder content here:
#   compile-server/libraries/  ← paste everything from your local
#                                  C:/Users/<you>/Documents/Arduino/libraries/
COPY libraries/ /root/Arduino/libraries/

# ── Node.js app ───────────────────────────────────────────────
WORKDIR /app
COPY package.json .
RUN npm install --omit=dev
COPY server.js .

EXPOSE 3000
CMD ["node", "server.js"]
