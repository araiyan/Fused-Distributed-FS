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
echo "To run the filesystem:"
echo ""
echo "  1. Using docker-compose (recommended):"
echo "     docker-compose up -d"
echo ""
echo "  2. Using docker run:"
echo "     docker run -d --name fused_fs --privileged \\"
echo "       --device /dev/fuse --cap-add SYS_ADMIN \\"
echo "       fused-fs:latest"
echo ""
echo "To test the filesystem:"
echo "     docker exec -it fused_fs ls -la /mnt/fused"
echo "     docker exec -it fused_fs sh -c 'echo \"test\" > /mnt/fused/test.txt'"
echo "     docker exec -it fused_fs cat /mnt/fused/test.txt"
echo ""
echo "To get a shell in the container:"
echo "     docker exec -it fused_fs /bin/bash"
echo ""
