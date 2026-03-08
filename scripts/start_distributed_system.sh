#!/bin/bash
# Build and start the complete distributed filesystem

set -e

# Step 1: Build distributed core library
cd "$(dirname "$0")/.."
source ./scripts/_common.sh

print_banner "Building Distributed Filesystem"

# Step 1: Build distributed core library
echo "[1/4] Building distributed core..."
cd distributed_core
make clean
make all
cd ..
echo "✓ Distributed core built"
echo ""

# Step 2: Generate protobuf code
echo "[2/4] Generating protobuf code..."
make clean-proto || true
make proto
echo "✓ Protobuf code generated"
echo ""

# Step 3: Build Docker images
echo "[3/4] Building Docker images..."
compose build
echo "✓ Docker images built"
echo ""

# Step 4: Start all containers
echo "[4/4] Starting all containers..."
compose up -d
echo "✓ All containers started"
echo ""

# Wait for services to be ready
echo "Waiting for services to initialize (15 seconds)..."
sleep 15

# Show status
echo ""
echo "========================================="
echo " Container Status"
echo "========================================="
compose ps
echo ""

echo "========================================="
echo " Distributed Filesystem is Running!"
echo "========================================="
echo ""
echo "Components:"
echo "  Metadata Cluster: 3 nodes (ports 7001-7003)"
echo "  Storage Nodes: 3 nodes (ports 50051-50053)"
echo "  Frontend Coordinators: 3 nodes (ports 60051-60053)"
echo ""
echo "To run tests:"
echo "  docker compose -f docker-compose-full.yml --profile test up -d test-client"
echo "  docker exec -it test-client bash"
echo "  ./tests/distributed_test.sh frontend-1:60051"
echo ""
echo "To view logs:"
echo "  docker compose -f docker-compose-full.yml logs -f frontend-1"
echo "  docker compose -f docker-compose-full.yml logs -f storage-node-1"
echo ""
echo "To stop:"
echo "  docker compose -f docker-compose-full.yml down"
echo ""
