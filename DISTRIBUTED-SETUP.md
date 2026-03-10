# Distributed Filesystem - Complete Setup Guide

## Overview

This distributed filesystem consists of three main layers:

1. **Metadata Cluster** (3 nodes) - Paxos-based consensus for metadata management
2. **Storage Nodes** (3 nodes) - gRPC servers for actual file storage
3. **Frontend Coordinators** (3 nodes) - Client-facing API that orchestrates metadata and storage

## Architecture

```
┌─────────────────────────────────────────┐
│        Client Applications              │
└──────────────┬──────────────────────────┘
               │ gRPC (ports 60051-60053)
               ▼
┌──────────────────────────────────────────┐
│  Frontend Coordinators (3 nodes)        │
│  - Coordinate metadata operations       │
│  - Orchestrate storage operations       │
│  - Client-facing gRPC API                │
└─────┬────────────────────────┬───────────┘
      │                        │
      ▼                        ▼
┌─────────────────┐    ┌──────────────────┐
│ Metadata Cluster│    │  Storage Nodes   │
│   (3 nodes)     │    │   (3 nodes)      │
│  Paxos + WAL    │    │  gRPC + FUSE     │
│  Ports 7001-3   │    │  Ports 50051-3   │
└─────────────────┘    └──────────────────┘
```

## Prerequisites

- **Docker Desktop** (must be running)
- **Docker Compose**
- **Windows, Linux, or macOS**

## Quick Start

### Option 1: PowerShell (Windows)

```powershell
# Start Docker Desktop first!

# Build and start the entire system
.\scripts\quick_start.ps1

# This will:
# - Build all Docker images
# - Start 9 containers (3 metadata + 3 storage + 3 frontend)
# - Wait for initialization
# - Show status
```

### Option 2: Bash (Linux/Mac/WSL)

```bash
# Make scripts executable
chmod +x scripts/*.sh

# Build and start
./scripts/start_distributed_system.sh
```

### Option 3: Manual Docker Compose

```bash
# Build images
docker-compose -f docker-compose-full.yml build

# Start services
docker-compose -f docker-compose-full.yml up -d

# Wait 20 seconds for initialization
sleep 20

# Check status
docker-compose -f docker-compose-full.yml ps
```

## Running Tests

### Automated Test Suite

```bash
# Start test client container
docker-compose -f docker-compose-full.yml --profile test up -d test-client

# Run comprehensive tests
docker exec -it test-client bash
cd /app/tests
./distributed_test.sh frontend-1:60051
```

### PowerShell Automated Tests

```powershell
# Start test client
docker-compose -f docker-compose-full.yml --profile test up -d test-client

# Run tests from host
.\scripts\distributed_test.ps1 localhost:60051
```

### Manual Testing

```bash
# Create directory
docker exec test-client /app/bin/distributed_client frontend-1:60051 mkdir / videos

# Create file
docker exec test-client /app/bin/distributed_client frontend-1:60051 create /videos test.mp4

# Write data
docker exec test-client /app/bin/distributed_client frontend-1:60051 write /videos/test.mp4 "Hello from distributed FS!"

# Read data
docker exec test-client /app/bin/distributed_client frontend-1:60051 read /videos/test.mp4

# Remove file
docker exec test-client /app/bin/distributed_client frontend-1:60051 rm /videos/test.mp4

# Remove empty directory
docker exec test-client /app/bin/distributed_client frontend-1:60051 rmdir /videos

# List directory (if implemented)
docker exec test-client /app/bin/distributed_client frontend-1:60051 ls /videos
```

## Container Ports

### Frontend Coordinators (Client Access)
- `localhost:60051` - Frontend 1 (recommended for testing)
- `localhost:60052` - Frontend 2
- `localhost:60053` - Frontend 3

### Storage Nodes (Internal)
- `localhost:50051` - Storage Node 1
- `localhost:50052` - Storage Node 2
- `localhost:50053` - Storage Node 3

### Metadata Cluster (Internal)
- `localhost:7001` - Metadata Node 1
- `localhost:7002` - Metadata Node 2
- `localhost:7003` - Metadata Node 3

## Viewing Logs

```bash
# All services
docker-compose -f docker-compose-full.yml logs -f

# Specific service
docker logs -f frontend-1
docker logs -f storage-node-1
docker logs -f metadata-node-1

# Last 50 lines
docker logs frontend-1 --tail 50
```

## Stopping the System

```bash
# Stop all containers
docker-compose -f docker-compose-full.yml down

# Stop and remove volumes (clean slate)
docker-compose -f docker-compose-full.yml down -v
```

## Component Details

### Frontend Coordinators
- **Language**: C++ with gRPC
- **Purpose**: Client-facing API, coordinates metadata and storage operations
- **Dependencies**: Distributed core library (Paxos, metadata manager, network engine, storage interface)

### Metadata Cluster
- **Language**: C
- **Purpose**: Distributed consensus for file metadata using Paxos
- **Features**: WAL persistence, quorum-based agreement, fault tolerance

### Storage Nodes
- **Language**: C++ (gRPC) + C (FUSE operations)
- **Purpose**: Actual file storage with gRPC API
- **Features**: Append-only writes, FUSE backend, TCP adapter for Paxos protocol

## Troubleshooting

### Build Errors

If you encounter build errors:
```bash
# Clean everything
docker-compose -f docker-compose-full.yml down -v
docker system prune -f

# Rebuild
docker-compose -f docker-compose-full.yml build --no-cache
```

### Connection Errors

If containers can't communicate:
```bash
# Check network
docker network inspect fused-distributed-fs_distributed-fs-net

# Check if all containers are running
docker ps

# Restart services
docker-compose -f docker-compose-full.yml restart
```

### Logs Show Errors

```bash
# Check specific service logs
docker logs frontend-1
docker logs storage-node-1
docker logs metadata-node-1

# Interactive debugging
docker exec -it frontend-1 bash
docker exec -it storage-node-1 bash
```

## Development

### Building Locally (Linux/Mac/WSL only)

```bash
# Build distributed core
cd distributed_core
make clean && make all
cd ..

# Generate protobuf
make proto

# Build frontend and client
make distributed
```

### Adding New Features

1. Modify source files in `src/`
2. Rebuild specific component:
   ```bash
   docker-compose -f docker-compose-full.yml build frontend-1
   docker-compose -f docker-compose-full.yml up -d frontend-1
   ```

## Testing Strategy

The test suite performs:

1. **Directory Creation** - Creates `/videos`, `/documents`, `/videos/shorts`
2. **File Creation** - Creates multiple test files
3. **Write Operations** - Writes data to files
4. **Read Operations** - Reads and verifies data
5. **Delete Operations** - Removes files (`rm`) and empty directories (`rmdir`)
6. **Append Operations** - Appends additional data to files
7. **Stress Test** - Creates and writes to 10 files simultaneously

## Performance Notes

- **Latency**: ~50-200ms per operation (depends on host machine)
- **Throughput**: Limited by Docker networking overhead
- **Fault Tolerance**: System remains available with 2/3 metadata nodes and 1/3 storage nodes

## Security Note

This is a development/testing setup using insecure gRPC credentials. For production:
- Add TLS/SSL encryption
- Implement authentication
- Use secure credentials
- Set up firewall rules

## License

See main repository LICENSE file.
