# Docker Testing Guide

This guide shows how to build and test the Distributed Core metadata cluster using Docker.

## Docker Environments

There are now **two Docker environments**:

### 1. Production Environment (`docker/Dockerfile`)
- **Purpose:** Run the 3-node metadata cluster
- **Build:** Optimized (-O2)
- **Entry:** Automatic server startup
- **Use case:** Production deployment and integration testing

### 2. Test Environment (`docker/Dockerfile.test`)
- **Purpose:** Interactive development and debugging
- **Build:** Debug mode (-O0 -g)
- **Entry:** Interactive bash shell
- **Includes:** gdb, valgrind, strace, tcpdump, netcat
- **Use case:** Development, debugging, manual testing

## Prerequisites

- Docker installed and running
- Docker Compose installed

## Quick Start

### Option 1: Using the Build Script (Recommended)

**Windows (PowerShell):**
```powershell
cd distributed_core
.\scripts\docker-build.ps1
```

**Linux/Mac:**
```bash
cd distributed_core
chmod +x scripts/docker-build.sh
./scripts/docker-build.sh
```

### Option 2: Manual Docker Commands

```bash
cd distributed_core

# Build the image
docker-compose build

# Start the cluster
docker-compose up -d

# View logs
docker-compose logs -f
```

## What Gets Built

The Docker setup creates:

### Production Cluster
- **3 metadata nodes** running Paxos consensus
- Each node listens on ports 7001, 7002, 7003
- Persistent WAL storage in Docker volumes
- Automatic peer connections between nodes
- Built from organized source structure:
  - `include/` → Header files
  - `src/` → Source files
  - `tests/` → Test server
  - `Makefile` → Build system

### Test Shell (Optional)
- **Interactive development environment**
- Full debugging toolkit
- Source code mounted as read-only volumes
- Network access to all cluster nodes
- Started with: `docker-compose --profile test up -d test-shell`

## Interactive Test Shell

### Start Test Shell

```bash
# Start the test environment
docker-compose --profile test up -d test-shell

# Enter interactive shell
docker exec -it test-shell bash
```

### Inside Test Shell

You'll see a help menu on login. Available commands:

**Build:**
```bash
make clean
make all
make test_server
```

**Debug with GDB:**
```bash
gdb ./bin/test_server
(gdb) break paxos_init
(gdb) run 1 7001 3 metadata-node-2:7002 metadata-node-3:7003
(gdb) backtrace
(gdb) print *paxos
```

**Memory leak detection:**
```bash
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         ./bin/test_server 1 7001 3
```

**System call tracing:**
```bash
strace -f -e trace=network,file ./bin/test_server 1 7001 3
```

**Network testing:**
```bash
# Test connectivity
ping -c 3 metadata-node-1
ping -c 3 metadata-node-2

# Check listening ports
netstat -tlnp

# Capture traffic
tcpdump -i any port 7001 -w capture.pcap

# Test port connections
nc -zv metadata-node-1 7001
```

## Useful Commands

### View Logs
```bash
# All nodes
docker-compose logs -f

# Specific node
docker logs -f metadata-node-1
docker logs -f metadata-node-2
docker logs -f metadata-node-3
```

### Check Status
```bash
docker-compose ps
docker-compose top
```

### Access a Node's Shell
```bash
# Interactive shell
docker exec -it metadata-node-1 /bin/bash

# Once inside, you can:
ls -l /app/bin/
cat /tmp/metadata_wal/*
netstat -tlnp
```

### Stop the Cluster
```bash
# Stop production cluster
docker-compose stop

# Stop and remove containers (keeps volumes)
docker-compose down

# Stop test shell
docker-compose --profile test down

# Stop everything and remove volumes (DELETES DATA!)
docker-compose down -v
```

### Restart a Node
```bash
# Restart specific node
docker restart metadata-node-1

# Restart all nodes
docker-compose restart
```

### View Resource Usage
```bash
docker stats
```

## Testing Scenarios

### 1. Test Basic Cluster
```bash
# Start production cluster
docker-compose up -d

# Watch logs to see Paxos initialization
docker-compose logs -f metadata-node-1

# Should see:
# [Paxos] Initialized (quorum size: 2)
# [Metadata] Initialized (WAL: ...)
# [Network] Started, listening on port ...
```

### 2. Test with Interactive Shell
```bash
# Start test shell
docker-compose --profile test up -d test-shell

# Enter shell
docker exec -it test-shell bash

# Inside shell:
# - Rebuilds: make clean && make all
# - Debug: gdb ./bin/test_server
# - Test connectivity: ping metadata-node-1
```

### 2. Test Node Failure
```bash
# Stop one node (cluster should still work with quorum)
docker stop metadata-node-3

# Check logs
docker logs metadata-node-1

# Restart node
docker start metadata-node-3
```

### 3. Test Network Connectivity
```bash
# From node 1, ping node 2
docker exec metadata-node-1 ping -c 3 metadata-node-2

# Check if service is listening
docker exec metadata-node-1 netstat -tlnp | grep 7001
```

### 4. Inspect WAL Files
```bash
# View WAL files
docker exec metadata-node-1 ls -lh /tmp/metadata_wal/

# View WAL content (binary)
docker exec metadata-node-1 hexdump -C /tmp/metadata_wal/metadata_wal_node1.dat
```

### 5. Manual Testing with Shell
```bash
# Enter node 1
docker exec -it metadata-node-1 /bin/bash

# Inside container:
cd /app
./bin/test_server 1 7001 3
```

## Troubleshooting

### Build Fails
```bash
# Clean build cache
docker-compose build --no-cache

# Remove old images
docker system prune -a
```

### Containers Won't Start
```bash
# Check Docker daemon
docker info

# Check port conflicts
netstat -an | grep 700

# View detailed logs
docker-compose logs --tail=100
```

### Network Issues
```bash
# Inspect network
docker network inspect distributed_core_distributed-fs-net

# Check container IPs
docker inspect metadata-node-1 | grep IPAddress
```

### Reset Everything
```bash
# Nuclear option - removes everything
docker-compose down -v
docker system prune -a -f
docker volume prune -f

# Rebuild from scratch
docker-compose build --no-cache
docker-compose up -d
```

## Architecture in Docker

```
┌─────────────────────────────────────────────┐
│         Host Machine (Windows/Linux)         │
│                                              │
│  ┌────────────────────────────────────────┐ │
│  │    Docker Network: distributed-fs-net  │ │
│  │          Subnet: 172.20.0.0/16         │ │
│  │                                         │ │
│  │   ┌──────────────┐  ┌──────────────┐  │ │
│  │   │   Node 1     │  │   Node 2     │  │ │
│  │   │ (port 7001)  │◄─┤ (port 7002)  │  │ │
│  │   └──────┬───────┘  └──────┬───────┘  │ │
│  │          │                 │           │ │
│  │          └────────┬────────┘           │ │
│  │                   │                    │ │
│  │          ┌────────▼────────┐           │ │
│  │          │     Node 3      │           │ │
│  │          │   (port 7003)   │           │ │
│  │          └─────────────────┘           │ │
│  │                                         │ │
│  │   Volumes:                              │ │
│  │   - metadata-wal-1 → /tmp/metadata_wal │ │
│  │   - metadata-wal-2 → /tmp/metadata_wal │ │
│  │   - metadata-wal-3 → /tmp/metadata_wal │ │
│  └────────────────────────────────────────┘ │
│                                              │
│  Port Mappings:                              │
│  - localhost:7001 → container:7001          │
│  - localhost:7002 → container:7002          │
│  - localhost:7003 → container:7003          │
└─────────────────────────────────────────────┘
```

## Next Steps

1. **Monitor Consensus**: Watch Paxos messages in logs
2. **Simulate Failures**: Stop nodes and observe recovery
3. **Measure Performance**: Use docker stats to monitor resources
4. **Integrate with FUSE**: Connect your FUSE filesystem to the cluster

## Performance Notes

- Each container uses ~50MB RAM (minimal)
- CPU usage depends on message throughput
- Network latency between containers is <1ms
- WAL writes are synced to host disk

## Clean Up

When done testing:
```bash
# Stop and remove everything
docker-compose down -v

# Clean up Docker
docker system prune -a
```

---

Happy testing! 🚀
