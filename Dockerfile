FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    libfuse-dev \
    fuse \
    pkg-config \
    git \
    vim \
    curl \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY include/ /app/include/
COPY src/ /app/src/
COPY Makefile /app/

RUN make clean && make all

# Create mount point
RUN mkdir -p /mnt/fused

# Expose volume for mount point
VOLUME ["/mnt/fused"]
