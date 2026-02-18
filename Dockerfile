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
    libcunit1 \
    libcunit1-dev \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler \
    protobuf-compiler-grpc \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY include/ /app/include/
COPY src/ /app/src/
COPY proto/ /app/proto/
COPY tests/ /app/tests/
COPY Makefile /app/
COPY scripts/ /app/scripts/

RUN make clean && make all && make install

# Create mount point
RUN mkdir -p /mnt/fused

# Add entrypoint script
COPY docker-entrypoint.sh /usr/local/bin/docker-entrypoint.sh
RUN sed -i 's/\r$//' /usr/local/bin/docker-entrypoint.sh && chmod +x /usr/local/bin/docker-entrypoint.sh

# Expose volume for mount point
VOLUME ["/mnt/fused"]

# Set entrypoint
ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]

# Default command
CMD ["/mnt/fused"]
