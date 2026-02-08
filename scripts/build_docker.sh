#!/bin/bash
# Build and run FUSED filesystem in Docker
 
set -e

echo "================================================"
echo "Building FUSED Filesystem Docker Container"
echo "================================================"
echo ""

# Build the Docker image
echo "Building Docker image..."
docker build -t fused-fs:latest .

echo ""
echo "================================================"
echo "Build complete!"
echo "================================================"
echo ""
