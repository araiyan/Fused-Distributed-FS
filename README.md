# FUSED - File System in Userspace

A FUSE-based filesystem implementation written in C with Docker support for easy testing and deployment.

## Project Structure

```
Fused-FS/
├── include/
│   └── fused_fs.h          # Main header file
├── src/
│   ├── fused_main.c        # Entry point and initialization
│   ├── fused_ops.c         # FUSE operations implementation
├── scripts/
│   └── build_docker.sh     # Docker build helper
├── Makefile                # Build configuration
```
## Requirements

### Local Build
- GCC compiler
- libfuse-dev (FUSE development files)
- Linux kernel with FUSE support

### Docker Build
- Docker Engine 20.10+
- Docker Compose (optional, for multi-container setup)

## Building

### Local Build

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install -y build-essential libfuse-dev fuse

# Build the filesystem
make

# The binary will be at: bin/fused_fs
```

### Docker Build

```bash
# Build using helper script
chmod +x scripts/build_docker.sh
./scripts/build_docker.sh

# Or build directly
docker build -t fused-fs:latest .
```

### Docker Usage

#### Option 1: Docker Compose (Recommended)

```bash
# Start the filesystem
docker-compose up -d

# Check if running
docker-compose ps

# Test the filesystem
docker exec -it fused_filesystem ls -la /mnt/fused
docker exec -it fused_filesystem sh -c 'echo "Hello" > /mnt/fused/test.txt'
docker exec -it fused_filesystem cat /mnt/fused/test.txt

# Get an interactive shell
docker exec -it fused_filesystem /bin/bash

# View logs
docker-compose logs -f

# Stop the filesystem
docker-compose down
```

#### Option 2: Docker Run

```bash
# Run the container
docker run -d \
  --name fused_fs \
  --privileged \
  --device /dev/fuse \
  --cap-add SYS_ADMIN \
  fused-fs:latest

# Test the filesystem
docker exec -it fused_fs ls -la /mnt/fused
docker exec -it fused_fs sh -c 'echo "test" > /mnt/fused/hello.txt'
docker exec -it fused_fs cat /mnt/fused/hello.txt

# Stop the container
docker stop fused_fs
docker rm fused_fs
```

## Acknowledgments

Built using:
- libfuse library (https://github.com/libfuse/libfuse)
