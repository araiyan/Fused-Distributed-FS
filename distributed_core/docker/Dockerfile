# Dockerfile for Distributed Core Metadata Cluster
FROM ubuntu:22.04

# Avoid interactive prompts
ENV DEBIAN_FRONTEND=noninteractive

# Install dependencies
RUN apt-get update && apt-get install -y \
    gcc \
    g++ \
    make \
    gdb \
    net-tools \
    iputils-ping \
    vim \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy organized source structure
COPY include/ ./include/
COPY src/ ./src/
COPY tests/ ./tests/
COPY Makefile ./

# Create necessary directories
RUN mkdir -p ./build ./bin /tmp/metadata_wal

# Build the distributed core
RUN IN_DOCKER=1 make clean && IN_DOCKER=1 make all && IN_DOCKER=1 make test_server

# Verify binary was created
RUN ls -lh ./bin/test_server

# Expose ports for metadata cluster communication
EXPOSE 7001 7002 7003

# Environment variables (can be overridden in docker-compose)
ENV NODE_ID=1
ENV LISTEN_PORT=7001
ENV TOTAL_NODES=3
ENV PEER_NODES=""

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD netstat -an | grep $LISTEN_PORT > /dev/null || exit 1

# Entry point - run the test server
WORKDIR /app
CMD ["/bin/bash", "-c", "/app/bin/test_server ${NODE_ID} ${LISTEN_PORT} ${TOTAL_NODES} ${PEER_NODES}"]
