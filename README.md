# FUSED - File System in Userspace

A FUSE-based filesystem implementation written in C with Docker support for easy testing and deployment.

## Project Structure

```
Fused-FS/
├── include/
│   └── fused_fs.h             # Main header file
├── proto/
│   └── filesystem.proto       # gRPC service definitions
├── src/
│   ├── fused_main.c           # Entry point and initialization
│   └── fused_ops.c            # FUSE operations implementation
│   └── fused_rpc_server.cpp   # Network RPC server for distributed filesystem     
├── scripts/
│   └── build_docker.sh        # Docker build helper
├── tests/
│   ├── unit_tests.c           # CUnit test suite
│   ├── simple_bash_test.c     # 
│   └── functional_test.sh     # Functional test script
└── Makefile                   # Build configuration
```
## Requirements

### Local Build
- GCC compiler
- libfuse-dev (FUSE development files)
- Linux kernel with FUSE support
- libcunit1 (for unit tests)

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
docker exec -it fused_fs ls -la /mnt/fused
docker exec -it fused_fs sh -c 'echo "Hello" > /mnt/fused/test.txt'
docker exec -it fused_fs cat /mnt/fused/test.txt

# Get an interactive shell
docker run -it --rm --privileged --entrypoint="" fused-fs:latest bash
# Then inside container:
mkdir -p /mnt/test && /usr/local/bin/fused_fs /mnt/test -o allow_other && cd /mnt/test

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
## Testing

### Unit Tests
Unit tests validate individual FUSE callbacks (getattr, readdir, etc.) using CUnit.
```bash
# Inside Docker container
docker exec fused_fs bash -c "cd /app && make test-unit"

# Local build (requires libcunit1-dev)
make test-unit
```

### Functional Tests
Functional tests verify end-to-end filesystem operations by mounting the filesystem and performing real file operations.
```bash
# Inside Docker container
docker exec fused_fs bash -c "cd /app && make test-functional"

# Run all tests (unit + functional)
docker exec fused_fs bash -c "cd /app && make test"
```

## Acknowledgments

Built using:
- libfuse library (https://github.com/libfuse/libfuse)
