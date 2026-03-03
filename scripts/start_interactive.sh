#!/bin/bash
set -e

echo "========================================="
echo " Starting Distributed Filesystem"
echo " with Interactive Shell"
echo "========================================="
echo ""

# Navigate to project root
cd "$(dirname "$0")/.."

echo "[1/3] Stopping any existing containers..."
docker-compose -f docker-compose-full.yml down 2>/dev/null || true
echo ""

echo "[2/3] Building and starting all containers..."
docker-compose -f docker-compose-full.yml up -d --build
echo ""

echo "[3/3] Waiting for services to be ready..."
sleep 5

# Check container status
echo ""
echo "Container Status:"
docker-compose -f docker-compose-full.yml ps

echo ""
echo "========================================="
echo " System Ready!"
echo "========================================="
echo ""
echo "To access the interactive shell, run:"
echo "  docker exec -it interactive-shell bash"
echo ""
echo "Once inside, type 'fs-test' to see available commands."
echo ""
echo "Example usage:"
echo "  /app/bin/distributed_client frontend-1:60051 mkdir / mydir"
echo "  /app/bin/distributed_client frontend-1:60051 create /mydir file.txt"
echo "  /app/bin/distributed_client frontend-1:60051 write /mydir file.txt 'Hello World'"
echo "  /app/bin/distributed_client frontend-1:60051 read /mydir file.txt"
echo "  /app/bin/distributed_client frontend-1:60051 ls /"
echo ""
echo "To stop the system, run:"
echo "  docker-compose -f docker-compose-full.yml down"
echo ""
