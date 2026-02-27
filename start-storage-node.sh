#!/bin/bash
set -e

echo "========================================="
echo " Storage Node Startup"
echo "========================================="
echo " Node ID: ${NODE_ID:-unknown}"
echo " TCP Port: ${STORAGE_PORT:-9000}"
echo " gRPC Port: ${GRPC_PORT:-50051}"
echo "========================================="

# Initialize FUSE filesystem state directory
mkdir -p /tmp/fused_backing

# Start gRPC server in background
echo "[1/2] Starting gRPC server on port ${GRPC_PORT:-50051}..."
/app/bin/fused_rpc_server &
GRPC_PID=$!

# Wait for gRPC to be ready
sleep 2

# Check if gRPC server started successfully
if ! kill -0 $GRPC_PID 2>/dev/null; then
    echo "ERROR: gRPC server failed to start"
    exit 1
fi

echo "[2/2] Starting TCP adapter on port ${STORAGE_PORT:-9000}..."
exec /app/bin/storage_tcp_adapter