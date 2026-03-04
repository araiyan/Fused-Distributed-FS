#!/bin/bash
# Helper script to run benchmarks in Docker

echo "Starting FUSE benchmarks in Docker..."

# Check if container is running
if ! docker ps | grep -q fused_fs; then
    echo "Starting container..."
    docker-compose up -d
    sleep 3
fi

# Check if FUSE is mounted
if ! docker exec fused_fs mountpoint -q /mnt/fused 2>/dev/null; then
    echo "Error: FUSE not mounted in container"
    exit 1
fi

# Make sure script is executable
docker exec fused_fs chmod +x /app/benchmarks/micro_bench.sh

# Run benchmarks
echo "Running benchmarks (this may take 5-10 minutes)..."
docker exec fused_fs /app/benchmarks/micro_bench.sh

# Check if benchmarks completed successfully
if [ $? -ne 0 ]; then
    echo "Error: Benchmarks failed"
    exit 1
fi

# Generate graphs
echo "Generating graphs..."
docker exec fused_fs python3 /app/benchmarks/plot_results.py

# Copy results from actual location (/app, not /app/benchmarks)
echo "Copying results to local directory..."
docker cp fused_fs:/app/benchmark_results.csv ./benchmarks/
docker cp fused_fs:/app/throughput_comparison.png ./benchmarks/
docker cp fused_fs:/app/latency_comparison.png ./benchmarks/
docker cp fused_fs:/app/overhead_percentage.png ./benchmarks/
docker cp fused_fs:/app/files_per_second.png ./benchmarks/

echo ""
echo "✓ Benchmarks complete!"
echo "Results saved to ./benchmarks/"
ls -lh ./benchmarks/*.png ./benchmarks/*.csv