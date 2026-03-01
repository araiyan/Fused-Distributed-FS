# QUICK START - Distributed Filesystem

## Prerequisites

**IMPORTANT**: Make sure Docker Desktop is running!

## Step-by-Step Instructions

### 1. Start Docker Desktop

Open Docker Desktop application and wait for it to fully start.

### 2. Build and Start the System

**Windows (PowerShell):**
```powershell
cd c:\Users\Raiya\source\repos\Fused-Distributed-FS
.\scripts\quick_start.ps1
```

**Alternative (Manual):**
```powershell
docker-compose -f docker-compose-full.yml build
docker-compose -f docker-compose-full.yml up -d
Start-Sleep -Seconds 20
docker-compose -f docker-compose-full.yml ps
```

### 3. Start Test Client

```powershell
docker-compose -f docker-compose-full.yml --profile test up -d test-client
```

### 4. Run Tests

```powershell
.\scripts\run_tests.ps1
```

**Or manually:**
```powershell
# Create directory
docker exec test-client /app/bin/distributed_client frontend-1:60051 mkdir / videos

# Create file
docker exec test-client /app/bin/distributed_client frontend-1:60051 create /videos test.mp4

# Write to file
docker exec test-client /app/bin/distributed_client frontend-1:60051 write /videos/test.mp4 "Hello World!"

# Read from file
docker exec test-client /app/bin/distributed_client frontend-1:60051 read /videos/test.mp4
```

### 5. View Logs (Optional)

```powershell
# Frontend coordinator logs
docker logs frontend-1 -f

# Storage node logs
docker logs storage-node-1 -f

# Metadata cluster logs
docker logs metadata-node-1 -f
```

### 6. Stop the System

```powershell
docker-compose -f docker-compose-full.yml down
```

## What Gets Tested

- ✅ Creating directories across distributed system
- ✅ Creating files with metadata stored in Paxos cluster
- ✅ Writing data to files stored on storage nodes
- ✅ Reading data from distributed storage
- ✅ Multiple write operations (append mode)
- ✅ Stress testing with 10+ files

## Architecture

9 Docker containers:
- 3 Metadata nodes (Paxos consensus)
- 3 Storage nodes (File storage with gRPC)
- 3 Frontend coordinators (Client API)

Client → Frontend → Metadata Cluster + Storage Nodes

## Troubleshooting

**"Docker is not running"**
- Start Docker Desktop and wait for it to fully initialize

**"Container not found"**
- Make sure all services started: `docker-compose -f docker-compose-full.yml ps`
- Restart: `docker-compose -f docker-compose-full.yml restart`

**"Build failed"**
- Clean build: `docker-compose -f docker-compose-full.yml build --no-cache`

**"Connection refused"**
- Wait longer for services to initialize (try 30 seconds)
- Check logs: `docker logs frontend-1`
