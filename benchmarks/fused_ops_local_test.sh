#!/bin/bash
set -euo pipefail

# Local-only FUSED ops benchmark.
# Tests mkdir, create/touch, write, read, rm separately against mounted FUSED FS.

ITERATIONS="${ITERATIONS:-100}"
MOUNT_POINT="${MOUNT_POINT:-/tmp/fused_ops_mount}"
RESULT_DIR="${RESULT_DIR:-scripts/fused_ops_results_local}"

if [ -x "./bin/fused" ]; then
    FUSED_BIN="./bin/fused"
elif [ -x "/usr/local/bin/fused_fs" ]; then
    FUSED_BIN="/usr/local/bin/fused_fs"
else
    echo "ERROR: Could not find FUSED binary at ./bin/fused or /usr/local/bin/fused_fs"
    exit 1
fi

mkdir -p "$RESULT_DIR"

mkdir_csv="$RESULT_DIR/mkdir.csv"
create_csv="$RESULT_DIR/create.csv"
write_csv="$RESULT_DIR/write.csv"
read_csv="$RESULT_DIR/read.csv"
rm_csv="$RESULT_DIR/rm.csv"

cleanup() {
    set +e
    if mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
        fusermount -u "$MOUNT_POINT" 2>/dev/null || umount -l "$MOUNT_POINT" 2>/dev/null || true
    fi
    pkill -f "fused_fs.*$MOUNT_POINT" 2>/dev/null || true
    pkill -f "$(basename "$FUSED_BIN").*$MOUNT_POINT" 2>/dev/null || true
    rm -rf "$MOUNT_POINT"
}

trap cleanup EXIT

cleanup
mkdir -p "$MOUNT_POINT"

echo "Mounting local FUSED filesystem at $MOUNT_POINT"
"$FUSED_BIN" "$MOUNT_POINT" -o allow_other &
FUSE_PID=$!

sleep 3
if ! mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
    echo "ERROR: Failed to mount FUSED at $MOUNT_POINT"
    kill "$FUSE_PID" 2>/dev/null || true
    exit 1
fi

echo "iteration,latency_ns" > "$mkdir_csv"
echo "iteration,latency_ns" > "$create_csv"
echo "iteration,latency_ns" > "$write_csv"
echo "iteration,latency_ns" > "$read_csv"
echo "iteration,latency_ns" > "$rm_csv"

echo "Running local FUSED ops tests for $ITERATIONS iterations"

for ((i=1; i<=ITERATIONS; i++)); do
    dir_path="$MOUNT_POINT/bench_dir_$i"
    file_path="$MOUNT_POINT/bench_file_$i.txt"
    rm_file_path="$MOUNT_POINT/bench_rm_$i.txt"
    payload="local-fused-op-payload-$i"

    # mkdir
    start=$(date +%s%N)
    mkdir "$dir_path"
    end=$(date +%s%N)
    echo "$i,$((end - start))" >> "$mkdir_csv"

    # create/touch
    start=$(date +%s%N)
    touch "$file_path"
    end=$(date +%s%N)
    echo "$i,$((end - start))" >> "$create_csv"

    # write (append-only friendly)
    start=$(date +%s%N)
    echo "$payload" >> "$file_path"
    end=$(date +%s%N)
    echo "$i,$((end - start))" >> "$write_csv"

    # read
    start=$(date +%s%N)
    cat "$file_path" > /dev/null
    end=$(date +%s%N)
    echo "$i,$((end - start))" >> "$read_csv"

    # rm (prepare dedicated file so rm latency is isolated)
    touch "$rm_file_path"
    echo "x" >> "$rm_file_path"
    start=$(date +%s%N)
    rm "$rm_file_path"
    end=$(date +%s%N)
    echo "$i,$((end - start))" >> "$rm_csv"

    # cleanup this iteration's mkdir/create/read/write artifacts
    rm -f "$file_path"
    rmdir "$dir_path"
done

echo "Done. Results written to $RESULT_DIR"
echo "  - $mkdir_csv"
echo "  - $create_csv"
echo "  - $write_csv"
echo "  - $read_csv"
echo "  - $rm_csv"
