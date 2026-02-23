# Commands Reference - Distributed Core

Quick reference for all build, test, and Docker commands.

## 📦 Building

### Local Build
```bash
# Change to distributed_core directory
cd distributed_core

# Clean previous builds
make clean

# Build libraries (static and shared)
make all

# Build test server
make test_server

# Install headers to parent project
make install

# Run static analysis
make check

# Format code
make format
```

**Output locations:**
- Libraries: `build/libdistributed_core.{a,so}`
- Binary: `bin/test_server`
- Objects: `build/obj/*.o`

### Docker Build

**Production cluster:**
```bash
docker-compose build
docker-compose up -d
```

**Test shell:**
```bash
docker-compose --profile test build test-shell
docker-compose --profile test up -d test-shell
```

**Rebuild without cache:**
```bash
docker-compose build --no-cache
docker-compose --profile test build --no-cache test-shell
```

## 🚀 Running

### Local Execution

**Single node (for testing):**
```bash
./bin/test_server 1 7001 3
```

**3-node cluster (3 terminals needed):**
```bash
# Terminal 1
./bin/test_server 1 7001 3 127.0.0.1:7002 127.0.0.1:7003

# Terminal 2
./bin/test_server 2 7002 3 127.0.0.1:7001 127.0.0.1:7003

# Terminal 3
./bin/test_server 3 7003 3 127.0.0.1:7001 127.0.0.1:7002
```

### Docker Execution

**Start production cluster:**
```bash
docker-compose up -d
```

**Start with logs:**
```bash
docker-compose up
```

**Start test shell:**
```bash
docker-compose --profile test up -d test-shell
docker exec -it test-shell bash
```

## 🔍 Monitoring

### View Logs

**Docker Compose:**
```bash
# All nodes
docker-compose logs -f

# Specific node
docker-compose logs -f metadata-node-1
docker-compose logs -f metadata-node-2
docker-compose logs -f metadata-node-3

# Last N lines
docker-compose logs --tail=50 metadata-node-1
```

**Docker native:**
```bash
docker logs -f metadata-node-1
docker logs --tail=100 metadata-node-1
```

### Check Status

```bash
# Container status
docker-compose ps

# Resource usage
docker stats

# Detailed container info
docker inspect metadata-node-1
```

### Network Inspection

```bash
# Inspect network
docker network inspect distributed_core_distributed-fs-net

# Check listening ports (from host)
netstat -an | grep 700

# Check from container
docker exec metadata-node-1 netstat -tlnp
```

## 🧪 Testing

### Interactive Test Shell

**Start and enter:**
```bash
docker-compose --profile test up -d test-shell
docker exec -it test-shell bash
```

**Inside test shell:**
```bash
# Show help menu
show_help

# Rebuild
make clean && make all && make test_server

# Test connectivity
ping metadata-node-1
ping metadata-node-2
ping metadata-node-3

# Check ports
netstat -tlnp

# View processes
ps aux
top
htop
```

### Debugging

**With GDB:**
```bash
# In test shell
gdb ./bin/test_server

# GDB commands
(gdb) break paxos_init
(gdb) run 1 7001 3 metadata-node-2:7002 metadata-node-3:7003
(gdb) continue
(gdb) backtrace
(gdb) print *paxos
(gdb) next
(gdb) step
(gdb) quit
```

**Memory leak detection:**
```bash
# In test shell
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         ./bin/test_server 1 7001 3
```

**System call tracing:**
```bash
# In test shell
strace -f -e trace=network,file ./bin/test_server 1 7001 3
```

**Network traffic capture:**
```bash
# In test shell
tcpdump -i any port 7001 -w /app/workspace/capture.pcap

# View capture
tcpdump -r /app/workspace/capture.pcap -A
```

### Node Failure Testing

```bash
# Stop a node (cluster should still work with 2/3)
docker stop metadata-node-3

# Check remaining nodes
docker logs metadata-node-1

# Restart node
docker start metadata-node-3
```

## 🛑 Stopping

### Production Cluster

```bash
# Stop containers (keeps volumes)
docker-compose stop

# Stop and remove containers (keeps volumes)
docker-compose down

# Remove everything including volumes (DELETES DATA!)
docker-compose down -v
```

### Test Shell

```bash
# Stop test shell
docker-compose --profile test down

# Stop everything (production + test)
docker-compose down
docker-compose --profile test down
```

### Scripts

**Windows PowerShell:**
```powershell
.\scripts\stop_cluster.ps1
```

**Linux/Mac:**
```bash
./scripts/stop_cluster.sh
```

## 🧹 Cleanup

### Build Artifacts

```bash
# Clean local build
make clean

# Remove all build artifacts
rm -rf build/ bin/
```

### Docker Cleanup

```bash
# Remove stopped containers
docker container prune

# Remove unused images
docker image prune

# Remove unused volumes
docker volume prune

# Remove everything unused
docker system prune -a

# Nuclear option (removes EVERYTHING)
docker system prune -a --volumes
```

## 🔧 Maintenance

### Rebuild Everything

**Local:**
```bash
make clean
make all
make test_server
```

**Docker:**
```bash
docker-compose down -v
docker-compose build --no-cache
docker-compose up -d
```

### Update Configuration

**Edit docker-compose.yml:**
```bash
vim docker-compose.yml
# or
nano docker-compose.yml
```

**Rebuild after changes:**
```bash
docker-compose down
docker-compose up -d
```

### View Configuration

```bash
# View docker-compose config
docker-compose config

# View Dockerfile
cat docker/Dockerfile
cat docker/Dockerfile.test
```

## 📊 Resource Management

### View Resource Usage

```bash
# All containers
docker stats

# Specific containers
docker stats metadata-node-1 metadata-node-2 metadata-node-3

# Container processes
docker top metadata-node-1
```

### Limit Resources

Edit `docker-compose.yml`:
```yaml
services:
  metadata-node-1:
    # ...
    deploy:
      resources:
        limits:
          cpus: '0.5'
          memory: 256M
        reservations:
          memory: 128M
```

## 🔄 Common Workflows

### Development Cycle

```bash
# 1. Start cluster
docker-compose up -d

# 2. Start test shell
docker-compose --profile test up -d test-shell
docker exec -it test-shell bash

# 3. Edit code (on host, in src/ or include/)
# Files are mounted, changes visible immediately

# 4. Rebuild in container
make clean && make all

# 5. Test
gdb ./bin/test_server
# or
valgrind ./bin/test_server 1 7001 3

# 6. Exit shell
exit

# 7. Restart production if needed
docker-compose restart metadata-node-1
```

### Troubleshooting Flow

```bash
# 1. Check container status
docker-compose ps

# 2. View logs
docker-compose logs metadata-node-1

# 3. Enter container
docker exec -it metadata-node-1 bash

# 4. Check processes
ps aux

# 5. Check network
netstat -tlnp
ping metadata-node-2

# 6. Check files
ls -la /app/
ls -la /tmp/metadata_wal/

# 7. Exit and restart if needed
exit
docker restart metadata-node-1
```

### Performance Testing

```bash
# 1. Start cluster
docker-compose up -d

# 2. Monitor resources
docker stats metadata-node-1 metadata-node-2 metadata-node-3

# 3. Send test traffic (from test shell or custom client)
docker exec -it test-shell bash

# 4. Capture metrics
docker stats --no-stream > metrics.txt
```

## 📝 Quick Tips

- **Logs disappearing?** Output is buffered. Use `docker-compose logs -f` to follow in real-time.
- **Port conflicts?** Check `netstat -an | grep 700` and kill conflicting processes.
- **Container won't start?** Check `docker-compose logs` for error messages.
- **Out of disk space?** Run `docker system prune -a --volumes` to clean up.
- **Slow builds?** Use `docker-compose build --parallel` for faster parallel builds.
- **Need clean slate?** Run `docker-compose down -v && docker system prune -a`.

## 🆘 Emergency Commands

```bash
# Kill all containers immediately
docker kill $(docker ps -q)

# Remove all containers
docker rm -f $(docker ps -aq)

# Remove all images
docker rmi -f $(docker images -q)

# Remove all volumes
docker volume rm $(docker volume ls -q)

# Complete reset
docker system prune -a --volumes -f
```

## 📚 Additional Resources

- [Docker Documentation](https://docs.docker.com/)
- [Docker Compose Reference](https://docs.docker.com/compose/compose-file/)
- [GDB Quick Reference](https://darkdust.net/files/GDB%20Cheat%20Sheet.pdf)
- [Valgrind Documentation](https://valgrind.org/docs/manual/quick-start.html)

---

**For detailed explanations, see:**
- [QUICKSTART.md](QUICKSTART.md) - Getting started guide
- [BUILD.md](BUILD.md) - Build instructions
- [DOCKER.md](DOCKER.md) - Docker usage guide
- [README.md](README.md) - Architecture overview
