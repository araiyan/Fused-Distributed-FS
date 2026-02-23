# 🚀 Build and Test Guide

## Directory Structure

The distributed_core is now organized as follows:
```
distributed_core/
├── src/              # Source files (.c)
├── include/          # Header files (.h)
├── tests/            # Test files
├── docker/           # Docker configurations
│   ├── Dockerfile        # Production environment
│   ├── Dockerfile.test   # Test environment
│   └── README.md         # Docker guide
├── scripts/          # Build scripts
├── docs/             # Documentation (you are here)
├── build/            # Build artifacts (generated)
├── bin/              # Binaries (generated)
├── Makefile          # Build system
└── docker-compose.yml # Container orchestration
```

## Step 1: Start Docker Desktop

**Before building, make sure Docker Desktop is running:**

1. Open **Docker Desktop** application
2. Wait for it to start (you'll see the whale icon in system tray)
3. Verify it's running:
   ```bash
   docker --version
   docker ps
   ```

## Step 2: Build the Docker Image

### Using PowerShell (Windows):

```powershell
cd distributed_core
.\scripts\docker-build.ps1
```

### Using Git Bash (Windows) or Linux/Mac:

```bash
cd distributed_core
chmod +x scripts/docker-build.sh
./scripts/docker-build.sh
```

### Manual Build:

```bash
cd distributed_core

# Build image
docker-compose build

# Start cluster
docker-compose up -d

# View logs
docker-compose logs -f
```

## Step 3: Verify the Cluster is Running

```bash
# Check container status
docker-compose ps

# Should show 3 containers running:
# - metadata-node-1
# - metadata-node-2  
# - metadata-node-3

# View logs
docker-compose logs --tail=50

# You should see:
# - "[Paxos] Initialized"
# - "[Metadata] Initialized"
# - "[Network] Started, listening on port..."
```

## Step 4: Test the Cluster

### Method 1: Using Test Shell (Recommended)

The test shell provides an interactive environment with full debugging tools:

```bash
# Start test shell
docker-compose --profile test up -d test-shell

# Enter the shell
docker exec -it test-shell bash

# Inside test shell - you'll see a help menu showing:
# - Build commands
# - Run commands
# - Debugging tools (gdb, valgrind, strace)
# - Network utilities
```

**Inside the test shell:**
```bash
# Rebuild code
make clean && make all && make test_server

# Run with debugger
gdb ./bin/test_server
(gdb) break paxos_init
(gdb) run 1 7001 3 metadata-node-2:7002 metadata-node-3:7003

# Memory leak detection
valgrind --leak-check=full ./bin/test_server 1 7001 3

# Trace system calls
strace -f ./bin/test_server 1 7001 3

# Network diagnostics
ping metadata-node-1
netstat -tlnp
tcpdump -i any port 7001
```

### Method 2: Direct Container Access

### Test 1: Check Network Connectivity
```bash
# Access node 1
docker exec -it metadata-node-1 /bin/bash

# Inside container, ping other nodes:
ping metadata-node-2
ping metadata-node-3

# Check listening ports:
netstat -tlnp | grep 7001
```

### Test 2: View WAL Files
```bash
docker exec metadata-node-1 ls -lh /tmp/metadata_wal/
```

### Test 3: Monitor Logs in Real-Time
```bash
# All nodes
docker-compose logs -f

# Single node
docker logs -f metadata-node-1
```

### Test 4: Simulate Node Failure
```bash
# Stop node 3 (cluster should still work - quorum is 2/3)
docker stop metadata-node-3

# Check remaining nodes
docker logs metadata-node-1

# Restart node 3
docker start metadata-node-3
```

## Step 5: Local Build (Without Docker)

If you want to build and run locally without Docker:

```bash
cd distributed_core

# Build everything
make clean
make all          # Builds libraries in build/
make test_server  # Builds binary in bin/

# Verify build
ls -lh bin/test_server
ls -lh build/
```

**Directory paths:**
- Source files: `src/*.c`
- Headers: `include/*.h`
- Test files: `tests/*.c`
- Object files: `build/obj/*.o`
- Libraries: `build/libdistributed_core.{a,so}`
- Binaries: `bin/test_server`

**Run locally (3 terminals):**
```bash
# Terminal 1
./bin/test_server 1 7001 3 127.0.0.1:7002 127.0.0.1:7003

# Terminal 2
./bin/test_server 2 7002 3 127.0.0.1:7001 127.0.0.1:7003

# Terminal 3
./bin/test_server 3 7003 3 127.0.0.1:7001 127.0.0.1:7002
```

## Step 6: Stop the Cluster

**Windows:**
```powershell
.\scripts\stop_cluster.ps1
```

**Linux/Mac:**
```bash
./scripts/stop_cluster.sh
```

**Or manually:**
```bash
# Stop production cluster
docker-compose down

# Stop test shell
docker-compose --profile test down

# Stop everything and remove volumes
docker-compose down -v
```

```bash
# Stop containers (keeps data)
docker-compose stop

# Stop and remove containers (keeps volumes)
docker-compose down

# Remove everything including data
docker-compose down -v
```

## Expected Output

When the cluster starts successfully, you should see:

```
=== Distributed Core Test Server ===
Node ID: 1
Listen Port: 7001
Total Nodes: 3
=====================================

[Paxos] Initialized (quorum size: 2)
[Metadata] Initialized (WAL: /tmp/metadata_wal_node1.dat)
[Network] Initialized
[Network] Started, listening on port 7001
[Network] Added peer 2 at metadata-node-2:7002
[Network] Added peer 3 at metadata-node-3:7003
[Storage] Initialized

=== Server Running ===
Press Ctrl+C to stop
```

## Troubleshooting

### Problem: Docker not found
**Solution:** Install Docker Desktop from https://www.docker.com/products/docker-desktop/

### Problem: Port already in use
**Solution:** 
```bash
# Windows
netstat -ano | findstr :7001
taskkill /PID <PID> /F

# Linux/Mac
lsof -i :7001
kill -9 <PID>

# Or change ports in docker-compose.yml
```

### Problem: Build fails with compilation errors
**Solution:**
```bash
# Rebuild from scratch
docker-compose down -v
docker-compose build --no-cache
```

### Problem: Containers exit immediately
**Solution:**
```bash
# Check logs for errors
docker-compose logs

# Verify binary exists
docker-compose run metadata-node-1 ls -l /app/bin/test_server

# Check file structure
docker-compose run metadata-node-1 ls -la /app/
```

### Problem: Network connection refused
**Solution:**
```bash
# Inspect network
docker network inspect distributed_core_distributed-fs-net

# Restart Docker Desktop and try again
docker-compose down
docker-compose up -d
```

### Problem: Source code changes not reflected
**Solution:**
```bash
# Rebuild images without cache
docker-compose build --no-cache
docker-compose up -d

# For test shell (source is mounted, just rebuild inside)
docker exec test-shell bash -c "make clean && make all"
```

## Quick Reference

| Command | Description |
|---------|-------------|
| `docker-compose build` | Build the images |
| `docker-compose up -d` | Start cluster in background |
| `docker-compose ps` | Show container status |
| `docker-compose logs -f` | Follow logs |
| `docker-compose stop` | Stop cluster |
| `docker-compose down` | Stop and remove containers |
| `docker-compose down -v` | Remove everything (including data) |
| `docker exec -it metadata-node-1 bash` | Shell into node 1 |
| `docker logs -f metadata-node-1` | View node 1 logs |
| `docker restart metadata-node-1` | Restart node 1 |
| `docker stats` | View resource usage |

## Architecture

```
Your Computer
    │
    ├─ Docker Desktop (Must be Running)
    │
    └─ Docker Compose
        │
        ├─ metadata-node-1 (port 7001)
        │   ├─ Paxos consensus
        │   ├─ Metadata manager
        │   └─ Network engine
        │
        ├─ metadata-node-2 (port 7002)
        │   └─ (same components)
        │
        └─ metadata-node-3 (port 7003)
            └─ (same components)
```

## Next Steps

Once the cluster is running:

1. **Monitor Paxos**: Watch consensus messages in logs
2. **Test Failures**: Stop/start nodes to test fault tolerance
3. **Integrate with FUSE**: Connect your filesystem to the cluster
4. **Add Storage Nodes**: Implement actual data storage

---

**Need help?** Check [DOCKER.md](DOCKER.md) for detailed troubleshooting.
