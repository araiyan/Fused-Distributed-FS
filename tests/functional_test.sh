#!/bin/bash
set -e

MOUNT_POINT="/mnt/fused_test"
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

PASS=0
FAIL=0

test_case() {
    if eval "$2" > /dev/null 2>&1; then
        echo -e "${GREEN}✓${NC} $1"
        ((PASS++))
    else
        echo -e "${RED}✗${NC} $1"
        ((FAIL++))
    fi
}

cleanup() {
    echo "Cleaning up..."
    fusermount -u "$MOUNT_POINT" 2>/dev/null || umount -l "$MOUNT_POINT" 2>/dev/null || true
    pkill -f "fused_fs.*$MOUNT_POINT" 2>/dev/null || true
    rm -rf "$MOUNT_POINT" /tmp/fused_backing_test
    sleep 1
}

trap cleanup EXIT

echo "Starting FUSED functional tests..."

# Cleanup any previous state
cleanup

# Create mount point
mkdir -p "$MOUNT_POINT"

# Mount filesystem in background WITHOUT -f flag
/usr/local/bin/fused_fs "$MOUNT_POINT" -o allow_other &
FUSE_PID=$!

# Wait for mount to be ready
sleep 3

# Verify it's actually mounted
if ! mount | grep -q "$MOUNT_POINT" && ! mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
    echo -e "${RED}✗${NC} Failed to mount filesystem"
    exit 1
fi

echo -e "${GREEN}✓${NC} Filesystem mounted"

# Run tests
test_case "List root directory" "ls $MOUNT_POINT"
test_case "Stat root directory" "stat $MOUNT_POINT"
test_case "Create file" "touch $MOUNT_POINT/test.txt"
test_case "Stat created file" "stat $MOUNT_POINT/test.txt"
test_case "Append to file" "echo 'Hello' >> $MOUNT_POINT/test.txt"
test_case "Read file" "cat $MOUNT_POINT/test.txt | grep -q 'Hello'"
test_case "Reject non-append" "! echo 'bad' > $MOUNT_POINT/test.txt 2>/dev/null"
test_case "Create directory" "mkdir $MOUNT_POINT/testdir"
test_case "Create file in subdir" "touch $MOUNT_POINT/testdir/sub.txt"
test_case "List subdirectory" "ls $MOUNT_POINT/testdir | grep -q 'sub.txt'"
test_case "Multiple appends" "echo 'Line2' >> $MOUNT_POINT/test.txt && echo 'Line3' >> $MOUNT_POINT/test.txt"
test_case "Verify file size" "[ \$(stat -c %s $MOUNT_POINT/test.txt) -gt 0 ]"

echo ""
echo "========================================"
echo "Results: ${GREEN}$PASS passed${NC}, ${RED}$FAIL failed${NC}"
echo "========================================"

[ $FAIL -eq 0 ] && exit 0 || exit 1