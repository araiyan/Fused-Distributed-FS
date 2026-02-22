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
