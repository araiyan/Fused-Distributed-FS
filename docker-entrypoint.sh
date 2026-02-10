#!/bin/bash
# Docker entrypoint script for FUSED filesystem

set -e

echo "================================================"
echo "FUSED - Distributed File System"
echo "================================================"
echo ""

# Check if running with required privileges
if [ ! -c /dev/fuse ]; then
    echo "ERROR: /dev/fuse not available"
    echo "Please run with: docker run --device /dev/fuse --cap-add SYS_ADMIN ..."
    exit 1
fi

# Get mount point from arguments (default to /mnt/fused)
MOUNT_POINT="${1:-/mnt/fused}"

# Check for append-only mode
APPEND_ONLY_FLAG=""
if [ "${FUSED_APPEND_ONLY}" = "1" ]; then
    APPEND_ONLY_FLAG="-o append_only"
    echo "Append-only mode: ENABLED"
fi

# Ensure mount point exists
mkdir -p "$MOUNT_POINT"

echo "Mount point: $MOUNT_POINT"
echo "Starting FUSED filesystem..."
echo ""

# Check if we should run in foreground or background
if [ "${FUSED_FOREGROUND}" = "1" ]; then
    echo "Running in foreground mode (CTRL+C to stop)"
    exec /usr/local/bin/fused_fs -f -d $APPEND_ONLY_FLAG "$MOUNT_POINT"
else
    echo "Running in background mode"
    /usr/local/bin/fused_fs $APPEND_ONLY_FLAG "$MOUNT_POINT"
    
    # Wait a bit for mount
    sleep 2
    
    # Check if mounted successfully
    if mountpoint -q "$MOUNT_POINT"; then
        echo ""
        echo "## Filesystem mounted successfully! :) ##"
        echo ""
        echo "You can now access the filesystem at: $MOUNT_POINT"
        echo ""
        echo "Examples:"
        echo "  docker exec -it <container> ls -la $MOUNT_POINT"
        echo "  docker exec -it <container> sh -c 'echo \"Hello\" > $MOUNT_POINT/test.txt'"
        echo "  docker exec -it <container> cat $MOUNT_POINT/test.txt"
        echo ""
        echo "To get an interactive shell:"
        echo "  docker exec -it <container> /bin/bash"
        echo ""
        
        # Keep container running
        echo "Press CTRL+C to stop..."
        tail -f /dev/null
    else
        echo "## X Failed to mount filesystem :( ##"
        exit 1
    fi
fi
