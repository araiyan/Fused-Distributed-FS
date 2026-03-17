#!/bin/bash

# Updated for your Gluster mount point
MOUNT_DIR="/mnt/gluster-storage"
TEST_DIR="$MOUNT_DIR/performance_test_$(date +%s)"
TEST_FILE="$TEST_DIR/bench.txt"
DATA="Benchmarking GlusterFS Replica 2 across AWS Public IPs."

# Function to get time in milliseconds
get_time() {
    date +%s%3N
}

run_test() {
    # Ensure mount exists
    if [ ! -d "$MOUNT_DIR" ]; then
        echo "Error: $MOUNT_DIR not found. Is Gluster mounted?"
        exit 1
    fi

    echo "--- Starting GlusterFS Benchmark on $MOUNT_DIR ---"

    # 1. MKDIR (Metadata op)
    start=$(get_time)
    mkdir -p "$TEST_DIR"
    echo "mkdir: $(( $(get_time) - start )) ms"

    # 2. CREATE (File creation)
    start=$(get_time)
    touch "$TEST_FILE"
    echo "touch/create: $(( $(get_time) - start )) ms"

    # 3. WRITE + SYNC (Network I/O)
    # We add 'sync' to ensure the data is pushed to the other node
    start=$(get_time)
    echo "$DATA" > "$TEST_FILE" && sync
    echo "write + sync: $(( $(get_time) - start )) ms"

    # 4. READ (Local/Remote read)
    start=$(get_time)
    cat "$TEST_FILE" > /dev/null
    echo "read: $(( $(get_time) - start )) ms"

    # 5. LS (Directory listing)
    start=$(get_time)
    ls "$TEST_DIR" > /dev/null
    echo "ls: $(( $(get_time) - start )) ms"

    # 6. DELETE FILE
    start=$(get_time)
    rm "$TEST_FILE"
    echo "rm (file): $(( $(get_time) - start )) ms"

    # 7. DELETE DIR
    start=$(get_time)
    rmdir "$TEST_DIR"
    echo "rmdir: $(( $(get_time) - start )) ms"

    echo "--- Test Complete ---"
}

run_test