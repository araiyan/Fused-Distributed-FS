#!/bin/bash
set -euo pipefail

# Compare per-operation latency for FUSED vs tmpfs.
# Operations: mkdir, create/touch, write, read, rm

ITERATIONS="${ITERATIONS:-100}"
RESULT_DIR="${RESULT_DIR:-benchmarks/fused_vs_tmpfs_results}"
FUSED_MOUNT="${FUSED_MOUNT:-/tmp/fused_vs_tmpfs_mount}"
TMPFS_MOUNT="${TMPFS_MOUNT:-/tmp/tmpfs_vs_fused_mount}"

if [ -x "./bin/fused" ]; then
    FUSED_BIN="./bin/fused"
elif [ -x "/usr/local/bin/fused_fs" ]; then
    FUSED_BIN="/usr/local/bin/fused_fs"
else
    echo "ERROR: Could not find FUSED binary at ./bin/fused or /usr/local/bin/fused_fs"
    exit 1
fi

mkdir -p "$RESULT_DIR"
RAW_CSV="$RESULT_DIR/raw_results.csv"
SUMMARY_TXT="$RESULT_DIR/summary.txt"

cleanup() {
    set +e

    if mountpoint -q "$FUSED_MOUNT" 2>/dev/null; then
        fusermount -u "$FUSED_MOUNT" 2>/dev/null || umount -l "$FUSED_MOUNT" 2>/dev/null || true
    fi
    if mountpoint -q "$TMPFS_MOUNT" 2>/dev/null; then
        umount "$TMPFS_MOUNT" 2>/dev/null || umount -l "$TMPFS_MOUNT" 2>/dev/null || true
    fi

    pkill -f "fused_fs.*$FUSED_MOUNT" 2>/dev/null || true
    pkill -f "$(basename "$FUSED_BIN").*$FUSED_MOUNT" 2>/dev/null || true

    rm -rf "$FUSED_MOUNT" "$TMPFS_MOUNT"
}

trap cleanup EXIT

cleanup
mkdir -p "$FUSED_MOUNT" "$TMPFS_MOUNT"

echo "Mounting FUSED at $FUSED_MOUNT"
"$FUSED_BIN" "$FUSED_MOUNT" -o allow_other &
FUSE_PID=$!

sleep 3
if ! mountpoint -q "$FUSED_MOUNT" 2>/dev/null; then
    echo "ERROR: Failed to mount FUSED at $FUSED_MOUNT"
    kill "$FUSE_PID" 2>/dev/null || true
    exit 1
fi

echo "Mounting tmpfs at $TMPFS_MOUNT"
mount -t tmpfs -o size=512m tmpfs "$TMPFS_MOUNT"

echo "filesystem,operation,iteration,latency_ns" > "$RAW_CSV"

measure_op() {
    local fs_name="$1"
    local operation="$2"
    local iter="$3"
    shift 3

    local start end
    start=$(date +%s%N)
    "$@"
    end=$(date +%s%N)

    echo "$fs_name,$operation,$iter,$((end - start))" >> "$RAW_CSV"
}

run_iteration_for_fs() {
    local fs_name="$1"
    local mount_point="$2"
    local i="$3"

    local dir_path="$mount_point/bench_dir_$i"
    local file_path="$mount_point/bench_file_$i.txt"
    local rm_file_path="$mount_point/bench_rm_$i.txt"
    local payload="fused-vs-tmpfs-$fs_name-$i"

    measure_op "$fs_name" "mkdir" "$i" mkdir "$dir_path"
    measure_op "$fs_name" "create" "$i" touch "$file_path"
    measure_op "$fs_name" "write" "$i" bash -lc "echo '$payload' >> '$file_path'"
    measure_op "$fs_name" "read" "$i" cat "$file_path"

    touch "$rm_file_path"
    echo "x" >> "$rm_file_path"
    measure_op "$fs_name" "rm" "$i" rm "$rm_file_path"

    rm -f "$file_path"
    rmdir "$dir_path"
}

echo "Running FUSED vs tmpfs benchmark for $ITERATIONS iterations"
for ((i=1; i<=ITERATIONS; i++)); do
    run_iteration_for_fs "fused" "$FUSED_MOUNT" "$i"
    run_iteration_for_fs "tmpfs" "$TMPFS_MOUNT" "$i"
done

{
    echo "--- RESULTS (Average over $ITERATIONS runs) ---"

    for op in mkdir create write read rm; do
        fused_ms=$(awk -F',' -v fs="fused" -v op="$op" '
            NR>1 && $1==fs && $2==op {sum+=$4; count++}
            END { if (count>0) printf "%.2f", (sum/count)/1000000.0; else printf "N/A" }
        ' "$RAW_CSV")

        tmpfs_ms=$(awk -F',' -v fs="tmpfs" -v op="$op" '
            NR>1 && $1==fs && $2==op {sum+=$4; count++}
            END { if (count>0) printf "%.2f", (sum/count)/1000000.0; else printf "N/A" }
        ' "$RAW_CSV")

        overhead_pct="N/A"
        if [ "$fused_ms" != "N/A" ] && [ "$tmpfs_ms" != "N/A" ]; then
            overhead_pct=$(awk -v f="$fused_ms" -v t="$tmpfs_ms" 'BEGIN { if (t==0) printf "N/A"; else printf "%.2f", ((f-t)/t)*100.0 }')
        fi

        printf "Average %-7s FUSED=%8s ms | tmpfs=%8s ms | overhead=%8s%%\n" \
            "${op^^}:" "$fused_ms" "$tmpfs_ms" "$overhead_pct"
    done
} | tee "$SUMMARY_TXT"

echo ""
echo "Saved raw results to: $RAW_CSV"
echo "Saved summary to: $SUMMARY_TXT"
