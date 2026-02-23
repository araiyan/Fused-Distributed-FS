# Quick Start Guide - Distributed Core

This guide will help you get the Metadata Cluster up and running in minutes.

## Prerequisites

- **Docker** and **Docker Compose** installed (required)
- **gcc/g++** (for local builds - optional)
- **make** (for local builds - optional)
- **pthread** library (for local builds - optional)

**Works on:** Windows, Linux, macOS (via Docker)

For Windows users: Docker Desktop is recommended. WSL2 is optional.

## Quick Start with Docker

### 1. Build and Start the Cluster

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

**Or manually:**
```bash
cd distributed_core
docker-compose up -d
```

This will:
- Build Docker images from the organized source structure (src/, include/, tests/)
- Start a 3-node metadata cluster
- Create persistent volumes for WAL files
- Expose ports 7001, 7002, 7003

### 2. Verify the Cluster is Running

```bash
docker-compose ps
```

You should see 3 metadata nodes and 3 storage nodes running:
```
NAME                COMMAND             STATUS              PORTS
metadata-node-1     ...                 Up                  0.0.0.0:7001->7001/tcp
metadata-node-2     ...                 Up                  0.0.0.0:7002->7002/tcp
metadata-node-3     ...                 Up                  0.0.0.0:7003->7003/tcp
```

### 3. View Logs

```bash
# All nodes
docker-compose logs -f

# Specific node
docker logs -f metadata-node-1
```

### 4. Stop the Cluster

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
docker-compose down
```

## Interactive Test Shell (Recommended for Development)

### 1. Start Test Shell

```bash
# Start the test environment
docker-compose --profile test up -d test-shell

# Enter interactive shell
docker exec -it test-shell bash
```

The test shell includes:
- ✅ Full debugging tools (gdb, valgrind, strace)
- ✅ Network analysis (tcpdump, netcat, netstat)
- ✅ Source code mounted for live editing
- ✅ All cluster nodes accessible

### 2. Inside Test Shell

```bash
# Rebuild if needed
make clean && make all && make test_server

# Run with GDB
gdb ./bin/test_server
(gdb) run 1 7001 3 metadata-node-2:7002 metadata-node-3:7003

# Memory leak detection
valgrind --leak-check=full ./bin/test_server 1 7001 3

# Network testing
ping metadata-node-1
netstat -tlnp
```

## Quick Start without Docker (Local Build)

### 1. Build Locally

```bash
cd distributed_core
make clean
make all
make test_server
```

**Note:** The Makefile uses:
- Source files from `src/`
- Headers from `include/`
- Test files from `tests/`
- Builds to `build/obj/` and `bin/`

### 2. Run 3-Node Cluster

Open 3 separate terminals:

**Terminal 1 (Node 1):**
```bash
./bin/test_server 1 7001 3 127.0.0.1:7002 127.0.0.1:7003
```

**Terminal 2 (Node 2):**
```bash
./bin/test_server 2 7002 3 127.0.0.1:7001 127.0.0.1:7003
```

**Terminal 3 (Node 3):**
```bash
./bin/test_server 3 7003 3 127.0.0.1:7001 127.0.0.1:7002
```

You should see output like:
```
=== Distributed Core Test Server ===
Node ID: 1
Listen Port: 7001
Total Nodes: 3
[Paxos] Initialized (quorum size: 2)
[Metadata] Initialized (WAL: /tmp/metadata_wal_node1.dat)
[Network] Started, listening on port 7001
=== Server Running ===
```

## Testing the Cluster

### 1. Check Network Connectivity

```bash
# From host
telnet localhost 7001
telnet localhost 7002
telnet localhost 7003

# From Docker
docker exec metadata-node-1 netstat -tlnp
```

### 2. Monitor WAL Files

```bash
# Local
ls -lh /tmp/metadata_wal_*.dat

# Docker
docker exec metadata-node-1 ls -lh /tmp/metadata_wal/
```

### 3. Simulate Node Failure

```bash
# Stop one node
docker stop metadata-node-3

# The cluster should still work (quorum = 2 out of 3)
docker logs metadata-node-1

# Restart node
docker start metadata-node-3
```

## Integration with FUSE

### 1. Install Headers

```bash
cd distributed_core
make install  # Copies headers to ../include/distributed_core/
```

### 2. Update Main Makefile

Edit `../Makefile` to link the distributed_core library:

```makefile
CFLAGS += -I./include
LDFLAGS += -L./build -ldistributed_core -pthread
```

### 3. Update FUSE Code

Edit `../src/fused_main.c`:

```c
#include "distributed_core/metadata_manager.h"
#include "distributed_core/paxos.h"

// Global state
static metadata_manager_t *g_metadata = NULL;
static paxos_node_t *g_paxos = NULL;

static void *fused_init(struct fuse_conn_info *conn) {
    // Initialize metadata cluster
    g_metadata = metadata_manager_init("/data/wal.dat");
    g_paxos = paxos_init(1, 3, persist_callback, apply_callback);
    
    // ... rest of init
    return NULL;
}
```

### 4. Rebuild FUSE

```bash
cd ..
make clean
make
```

## Troubleshooting

### Problem: Port Already in Use

**Solution:**
```bash
# Find process using port
lsof -i :7001
kill -9 <PID>

# Or change port in docker-compose.yml
```

### Problem: Docker Build Fails

**Solution:**
```bash
# Clean Docker cache
docker system prune -a
docker-compose build --no-cache
```

### Problem: Nodes Can't Connect

**Solution:**
```bash
# Check Docker network
docker network inspect distributed_core_distributed-fs-net

# Restart networking
docker-compose down
docker network prune
docker-compose up -d
```

### Problem: WAL Corruption

**Solution:**
```bash
# Delete WAL (WARNING: LOSES DATA)
rm /tmp/metadata_wal_*.dat

# Or in Docker
docker-compose down -v  # Removes volumes
docker-compose up -d
```

## Next Steps

1. **Read the [README.md](README.md)** for architecture details
2. **Review the code** in `paxos.c`, `metadata_manager.c`, etc.
3. **Implement FUSE integration** to use the metadata cluster
4. **Add storage nodes** that actually store data
5. **Run functional tests** in `../tests/`

## Configuration Options

### Scaling the Cluster

To run a 5-node cluster, edit `docker-compose.yml`:

```yaml
services:
  metadata-node-4:
    # ... (copy from node-1, change NODE_ID to 4, port to 7004)
  
  metadata-node-5:
    # ... (copy from node-1, change NODE_ID to 5, port to 7005)
```

Then update each node's `TOTAL_NODES=5` and `PEER_NODES`.

### Custom WAL Path

Edit `test_server.c`:

```c
snprintf(wal_path, sizeof(wal_path), "/custom/path/wal_node%u.dat", node_id);
```

### Network Tuning

Edit `network_engine.c`:

```c
#define MAX_PENDING_CONNECTIONS 256  // Increase backlog
#define RECV_BUFFER_SIZE 131072      // 128KB buffer
```

## Performance Tuning

### For Low Latency
- Use SSD for WAL storage
- Enable TCP_NODELAY (already done)
- Reduce fsync frequency (trade-off: durability)

### For High Throughput
- Increase RECV_BUFFER_SIZE
- Use multiple network threads
- Batch Paxos proposals

### For Large Clusters
- Implement stable leader optimization
- Use read-only replicas
- Add log compaction

## Resources

- **API Documentation**: See header files (`.h`)
- **Architecture Diagram**: See README.md
- **Paxos Paper**: [Paxos Made Simple](https://lamport.azurewebsites.net/pubs/paxos-simple.pdf)
- **Issues**: Report bugs in project repository

## Support

For questions or issues:
1. Check the [README.md](README.md) troubleshooting section
2. Review Docker logs: `docker-compose logs`
3. Run with debug symbols: `make clean && CFLAGS="-g -O0" make all`
4. Use gdb: `gdb ../bin/test_server`

---

**Happy distributed system building!** 🚀
