# Distributed Core - Refactored Directory Structure

This is the refactored and organized directory structure for the distributed metadata cluster.

## 📁 Directory Structure

```
distributed_core/
├── src/                    # Source files (.c)
│   ├── paxos.c
│   ├── metadata_manager.c
│   ├── network_engine.c
│   └── storage_interface.c
│
├── include/                # Header files (.h)
│   ├── paxos.h
│   ├── metadata_manager.h
│   ├── network_engine.h
│   └── storage_interface.h
│
├── tests/                  # Test files
│   └── test_server.c
│
├── docker/                 # Docker configurations
│   ├── Dockerfile          # Production environment
│   ├── Dockerfile.test     # Interactive test environment
│   └── README.md          # Docker usage guide
│
├── scripts/                # Build and utility scripts
│   ├── build_and_run.ps1
│   ├── build_and_run.sh
│   ├── docker-build.ps1
│   ├── docker-build.sh
│   ├── stop_cluster.ps1
│   └── stop_cluster.sh
│
├── docs/                   # Documentation
│   ├── README.md
│   ├── BUILD.md
│   ├── DOCKER.md
│   ├── IMPLEMENTATION.md
│   └── QUICKSTART.md
│
├── build/                  # Build artifacts (generated)
│   └── obj/               # Object files
│
├── bin/                    # Compiled binaries (generated)
│   └── test_server
│
├── Makefile               # Build system
└── docker-compose.yml     # Container orchestration

```

## 🚀 Quick Start

### Option 1: Production Cluster (Docker)
```bash
# Start 3-node metadata cluster
docker-compose up -d

# View logs
docker-compose logs -f metadata-node-1

# Check status
docker-compose ps

# Stop cluster
docker-compose down
```

### Option 2: Interactive Test Shell (Docker)
```bash
# Start test environment
docker-compose --profile test up -d test-shell

# Enter interactive shell
docker exec -it test-shell bash

# Inside the shell, you'll see a help menu with:
# - Build commands
# - Run commands
# - Debugging tools
# - Network utilities
```

### Option 3: Local Build
```bash
# Build libraries
make all

# Build test server
make test_server

# Run test server
./bin/test_server 1 7001 3

# Clean build
make clean
```

## 🔧 Building

The Makefile automatically detects the build environment:
- **Local builds**: Uses `../build` and `../bin` directories
- **Docker builds**: Uses `./build` and `./bin` directories (set `IN_DOCKER=1`)

**Build targets:**
- `make all` - Build static and shared libraries
- `make test_server` - Build test server binary
- `make clean` - Remove build artifacts
- `make install` - Install headers to parent include directory
- `make check` - Run static analysis (requires `cppcheck`)
- `make format` - Format code (requires `clang-format`)

## 🐳 Docker Environments

### Production (Dockerfile)
**Purpose:** Run the distributed metadata cluster
- 3 metadata nodes (ports 7001, 7002, 7003)
- Persistent WAL storage
- Health checks enabled
- Optimized build (-O2)

**Commands:**
```bash
docker-compose up -d                  # Start cluster
docker-compose logs -f metadata-node-1  # View logs
docker-compose ps                     # Check status
docker-compose down                   # Stop cluster
```

### Test Shell (Dockerfile.test)
**Purpose:** Interactive development and debugging
- Full debugging tools (gdb, valgrind, strace)
- Network analysis tools (netcat, tcpdump)
- Source mounted as read-only volumes
- Debug build (-O0 -g)

**Commands:**
```bash
# Start test shell
docker-compose --profile test up -d test-shell

# Enter shell
docker exec -it test-shell bash

# Inside the shell - rebuild code
make clean && make all && make test_server

# Run with GDB
gdb ./bin/test_server
(gdb) run 1 7001 3 metadata-node-2:7002 metadata-node-3:7003

# Memory leak detection
valgrind --leak-check=full ./bin/test_server 1 7001 3

# Network inspection
netstat -tlnp
ping metadata-node-1
tcpdump -i any port 7001
```

## 📚 Documentation

Detailed documentation is in the `docs/` directory:
- **README.md** - Overview and architecture
- **BUILD.md** - Build instructions
- **DOCKER.md** - Docker usage guide
- **IMPLEMENTATION.md** - Implementation details
- **QUICKSTART.md** - Quick start guide

## 🧪 Testing

### Manual Testing (Test Shell)
```bash
# Start test shell
docker-compose --profile test up -d test-shell
docker exec -it test-shell bash

# Run test server connected to cluster
./bin/test_server 1 7001 3 metadata-node-2:7002 metadata-node-3:7003
```

### Network Connectivity Test
```bash
# From test shell, verify connectivity
ping metadata-node-1
ping metadata-node-2
ping metadata-node-3

# Check listening ports
netstat -tlnp | grep test_server
```

## 🔍 Debugging

### Using GDB (in test shell)
```bash
gdb ./bin/test_server
(gdb) break paxos_init
(gdb) run 1 7001 3 metadata-node-2:7002 metadata-node-3:7003
(gdb) continue
(gdb) backtrace
```

### Memory Leak Detection
```bash
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         ./bin/test_server 1 7001 3
```

### System Call Tracing
```bash
strace -f -e trace=network,file ./bin/test_server 1 7001 3
```

### Network Traffic Capture
```bash
# Capture traffic on port 7001
tcpdump -i any port 7001 -w capture.pcap

# View captured traffic
tcpdump -r capture.pcap -A
```

## 🧹 Cleanup

```bash
# Stop all containers
docker-compose down

# Stop and remove volumes
docker-compose down -v

# Stop test shell
docker-compose --profile test down

# Remove build artifacts
make clean
```

## 📝 Development Workflow

### 1. Make Code Changes
Edit files in `src/` or `include/` directories

### 2. Test in Docker
```bash
# Rebuild and test
docker-compose down
docker-compose build --no-cache
docker-compose up -d
docker-compose logs -f
```

### 3. Interactive Debugging
```bash
# Use test shell for detailed debugging
docker-compose --profile test up -d test-shell
docker exec -it test-shell bash
# ... use gdb, valgrind, etc.
```

### 4. Update Documentation
Update relevant files in `docs/` directory

## 🔗 Integration

To integrate with the parent FUSE filesystem:

```bash
# Install headers to parent include directory
make install

# Headers will be copied to:
# ../include/distributed_core/
```

Then from the parent project:
```c
#include "distributed_core/paxos.h"
#include "distributed_core/metadata_manager.h"
// etc.
```

## 🎯 Next Steps

1. **Implement Paxos consensus tests** in test-shell
2. **Add unit tests** for each component
3. **Implement storage node daemon** (replace placeholders)
4. **Integrate with FUSE layer** in parent project
5. **Add monitoring/metrics** collection

## 📞 Need Help?

- Check `docker/README.md` for Docker-specific help
- Review `docs/` for detailed documentation
- Run `show_help` inside test-shell for command reference
