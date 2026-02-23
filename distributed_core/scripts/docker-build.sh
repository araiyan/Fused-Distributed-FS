#!/bin/bash
# docker-build.sh - Build and run the distributed core in Docker

set -e

echo "=== Building Distributed Core Docker Image ==="

cd "$(dirname "$0")"

# Stop any running containers
echo "Stopping any running containers..."
docker-compose down 2>/dev/null || true

# Build the Docker image
echo "Building Docker image..."
docker-compose build --no-cache

echo "✓ Docker image built successfully"

# Start the cluster
echo "Starting 3-node metadata cluster..."
docker-compose up -d

# Wait for containers to start
echo "Waiting for containers to start..."
sleep 3

# Check status
echo ""
echo "=== Cluster Status ==="
docker-compose ps

# Show logs
echo ""
echo "=== Recent Logs ==="
docker-compose logs --tail=30

echo ""
echo "=== Cluster Running! ==="
echo ""
echo "Metadata nodes:"
echo "  - Node 1: localhost:7001"
echo "  - Node 2: localhost:7002"
echo "  - Node 3: localhost:7003"
echo ""
echo "Commands:"
echo "  View logs:     docker-compose logs -f"
echo "  View node 1:   docker logs -f metadata-node-1"
echo "  Stop cluster:  docker-compose down"
echo "  Shell access:  docker exec -it metadata-node-1 /bin/bash"
echo ""
