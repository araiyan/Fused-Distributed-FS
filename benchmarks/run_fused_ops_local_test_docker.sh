#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE="fused-fs:latest"
ITERATIONS="${ITERATIONS:-100}"
RESULT_DIR="${RESULT_DIR:-benchmarks/fused_ops_results_local}"
MOUNT_POINT="${MOUNT_POINT:-/tmp/fused_ops_mount}"

# Git Bash on Windows rewrites POSIX-like args passed to docker.
DOCKER_MOUNT_SOURCE="$ROOT_DIR"
if command -v cygpath >/dev/null 2>&1; then
    DOCKER_MOUNT_SOURCE="$(cygpath -m "$ROOT_DIR")"
fi

cd "$ROOT_DIR"

echo "========================================="
echo " Local FUSED Ops Test (Docker)"
echo "========================================="
echo "Iterations: $ITERATIONS"
echo "Container mount point: $MOUNT_POINT"
echo "Result directory: $RESULT_DIR"
echo ""

echo "[1/3] Building Docker image..."
docker build -f Dockerfile -t "$IMAGE" .

echo ""
echo "[2/3] Running local ops test inside Docker container..."
MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL="*" docker run --rm -t \
  --entrypoint /bin/bash \
  --privileged \
  --device /dev/fuse \
  --cap-add SYS_ADMIN \
  -v "$DOCKER_MOUNT_SOURCE:/workspace" \
  -w /workspace \
  -e ITERATIONS="$ITERATIONS" \
  -e RESULT_DIR="$RESULT_DIR" \
  -e MOUNT_POINT="$MOUNT_POINT" \
  "$IMAGE" \
  -lc "chmod +x benchmarks/fused_ops_local_test.sh && benchmarks/fused_ops_local_test.sh"

echo ""
echo "[3/3] Done"
echo "Results are available on host at: $ROOT_DIR/$RESULT_DIR"
echo "  - $ROOT_DIR/$RESULT_DIR/mkdir.csv"
echo "  - $ROOT_DIR/$RESULT_DIR/create.csv"
echo "  - $ROOT_DIR/$RESULT_DIR/write.csv"
echo "  - $ROOT_DIR/$RESULT_DIR/read.csv"
echo "  - $ROOT_DIR/$RESULT_DIR/rm.csv"
