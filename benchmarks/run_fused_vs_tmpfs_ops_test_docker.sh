#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE="fused-fs:latest"
ITERATIONS="${ITERATIONS:-100}"
RESULT_DIR="${RESULT_DIR:-benchmarks/fused_vs_tmpfs_results}"
FUSED_MOUNT="${FUSED_MOUNT:-/tmp/fused_vs_tmpfs_mount}"
TMPFS_MOUNT="${TMPFS_MOUNT:-/tmp/tmpfs_vs_fused_mount}"

DOCKER_MOUNT_SOURCE="$ROOT_DIR"
if command -v cygpath >/dev/null 2>&1; then
    DOCKER_MOUNT_SOURCE="$(cygpath -m "$ROOT_DIR")"
fi

cd "$ROOT_DIR"

echo "========================================="
echo " FUSED vs tmpfs Ops Test (Docker)"
echo "========================================="
echo "Iterations: $ITERATIONS"
echo "Result directory: $RESULT_DIR"
echo ""

echo "[1/3] Building Docker image..."
docker build -f Dockerfile -t "$IMAGE" .

echo ""
echo "[2/3] Running comparison benchmark inside Docker container..."
MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL="*" docker run --rm -t \
  --entrypoint /bin/bash \
  --privileged \
  --device /dev/fuse \
  --cap-add SYS_ADMIN \
  -v "$DOCKER_MOUNT_SOURCE:/workspace" \
  -w /workspace \
  -e ITERATIONS="$ITERATIONS" \
  -e RESULT_DIR="$RESULT_DIR" \
  -e FUSED_MOUNT="$FUSED_MOUNT" \
  -e TMPFS_MOUNT="$TMPFS_MOUNT" \
  "$IMAGE" \
  -lc "chmod +x benchmarks/fused_vs_tmpfs_ops_test.sh && benchmarks/fused_vs_tmpfs_ops_test.sh"

echo ""
echo "[3/3] Done"
echo "Results are available on host at: $ROOT_DIR/$RESULT_DIR"
echo "  - $ROOT_DIR/$RESULT_DIR/raw_results.csv"
echo "  - $ROOT_DIR/$RESULT_DIR/summary.txt"
