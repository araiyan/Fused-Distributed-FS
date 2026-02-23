# Docker Environment for Distributed Core

This directory contains Docker configurations for building and testing the distributed metadata cluster.

## Dockerfiles

### `Dockerfile` - Production Environment
The main production Dockerfile that:
- Builds the distributed core libraries
- Compiles the test server
- Runs the metadata cluster nodes
- Includes health checks

**Used by:** `metadata-node-1`, `metadata-node-2`, `metadata-node-3` services

### `Dockerfile.test` - Interactive Test Environment
A development-focused Dockerfile that:
- Includes debugging tools (gdb, valgrind, strace)
- Includes network analysis tools (netcat, tcpdump)
- Provides an interactive shell
- Mounts source code for live editing
- Builds in debug mode with symbols

**Used by:** `test-shell` service

## Usage

### Start Production Cluster
```bash
# Start the 3-node metadata cluster
docker-compose up -d

# View logs
docker-compose logs -f metadata-node-1

# Check status
docker-compose ps
```

### Start Test Shell
```bash
# Start interactive test environment
docker-compose --profile test up -d test-shell

# Enter the shell
docker exec -it test-shell bash

# Inside the shell, you'll see a help menu with available commands
```

### Inside the Test Shell

Once inside the test shell, you can:

**Build and compile:**
```bash
make clean
make all
make test_server
```

**Run tests manually:**
```bash
./bin/test_server 1 7001 3 metadata-node-2:7002 metadata-node-3:7003
```

**Debug with GDB:**
```bash
gdb ./bin/test_server
# Inside GDB:
(gdb) run 1 7001 3 metadata-node-2:7002 metadata-node-3:7003
(gdb) break paxos_init
(gdb) continue
```

**Memory leak detection:**
```bash
valgrind --leak-check=full --show-leak-kinds=all ./bin/test_server 1 7001 3
```

**System call tracing:**
```bash
strace -f ./bin/test_server 1 7001 3
```

**Network inspection:**
```bash
# Show listening ports
netstat -tlnp

# Test connectivity to other nodes
ping metadata-node-1
nc -zv metadata-node-1 7001

# Capture network traffic
tcpdump -i any port 7001
```

## Networking

All containers share the `distributed-fs-net` network:
- Subnet: 172.20.0.0/16
- Metadata nodes can communicate directly using hostnames
- Test shell can connect to any metadata node

## Volumes

### Persistent Volumes
- `metadata-wal-1`, `metadata-wal-2`, `metadata-wal-3`: Store WAL files for each node
- `storage-data-1/2/3`: Storage node data (placeholders)
- `test-workspace`: Persistent workspace in test shell

### Bind Mounts (Test Shell)
- `./src:/app/src:ro`: Source files (read-only)
- `./include:/app/include:ro`: Header files (read-only)
- `./tests:/app/tests:ro`: Test files (read-only)

## Tips

### Rebuild After Changes
```bash
# Rebuild production images
docker-compose build --no-cache
docker-compose up -d

# Rebuild test image
docker-compose --profile test build --no-cache test-shell
docker-compose --profile test up -d test-shell
```

### Clean Up
```bash
# Stop all containers
docker-compose down

# Stop and remove volumes
docker-compose down -v

# Stop test shell
docker-compose --profile test down
```

### View Container Resource Usage
```bash
docker stats metadata-node-1 metadata-node-2 metadata-node-3
```

### Copy Files from Container
```bash
# Copy WAL file from container
docker cp metadata-node-1:/tmp/metadata_wal/metadata_wal_node1.dat ./

# Copy binary
docker cp test-shell:/app/bin/test_server ./
```
