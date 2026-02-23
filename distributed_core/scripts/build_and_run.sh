#!/bin/bash
# build_and_run.sh - Build and run the distributed metadata cluster

set -e

echo "=== Building Distributed Core Metadata Cluster ==="

# Navigate to distributed_core directory
cd "$(dirname "$0")"

# Clean previous builds
echo "Cleaning previous builds..."
make clean || true
docker-compose down -v 2>/dev/null || true

# Build locally first (to catch compilation errors)
echo "Building locally..."
make all
make test_server

echo "✓ Local build successful"

# Build Docker images
echo "Building Docker images..."
docker-compose build

echo "✓ Docker images built"

# Start the cluster
echo "Starting 3-node metadata cluster..."
docker-compose up -d

# Wait for nodes to start
echo "Waiting for nodes to start..."
sleep 5

# Check status
echo ""
echo "=== Cluster Status ==="
docker-compose ps

# Show logs
echo ""
echo "=== Recent Logs ==="
docker-compose logs --tail=20

echo ""
echo "=== Cluster is running! ==="
echo "Metadata nodes:"
echo "  - Node 1: localhost:7001"
echo "  - Node 2: localhost:7002"
echo "  - Node 3: localhost:7003"
echo ""
echo "Commands:"
echo "  View logs:    docker-compose logs -f"
echo "  Stop cluster: docker-compose down"
echo "  Restart:      docker-compose restart"
echo ""
