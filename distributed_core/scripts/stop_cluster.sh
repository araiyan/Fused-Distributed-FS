#!/bin/bash
# stop_cluster.sh - Stop the distributed metadata cluster

cd "$(dirname "$0")"

echo "=== Stopping Distributed Metadata Cluster ==="

docker-compose down

echo "✓ Cluster stopped"
echo ""
echo "To remove volumes (DELETES ALL DATA):"
echo "  docker-compose down -v"
