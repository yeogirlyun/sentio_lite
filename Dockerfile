# Multi-stage build for Sentio Lite (SIGOR-enabled)

# ============================
# Builder image
# ============================
FROM debian:bookworm AS builder

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    pkg-config \
    libeigen3-dev \
    librdkafka-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . /app

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build -j

# ============================
# Runtime image
# ============================
FROM debian:bookworm-slim AS runtime

ARG DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libstdc++6 \
    libgcc-s1 \
    libeigen3-dev \
    librdkafka1 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy binary
COPY --from=builder /app/build/sentio_lite /usr/local/bin/sentio_lite

# Copy entrypoint helper
COPY docker/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

# Default environment (override at run time)
ENV STRATEGY=sigor \
    TEST_DATE=2025-10-22 \
    SIM_DAYS=0 \
    DATA_DIR=/app/data \
    EXT=.bin \
    DASHBOARD=1 \
    EXTRA_ARGS=""

# Mount points for data and logs
VOLUME ["/app/data", "/app/logs"]

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]


