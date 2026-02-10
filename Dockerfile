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
COPY scripts/ /app/scripts/

RUN make clean && make all && make install

# Create mount point
RUN mkdir -p /mnt/fused

# Add entrypoint script
COPY docker-entrypoint.sh /usr/local/bin/
RUN chmod +x /usr/local/bin/docker-entrypoint.sh

# Expose volume for mount point
VOLUME ["/mnt/fused"]

# Set entrypoint
ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]

# Default command
CMD ["/mnt/fused"]
