#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE="fused-distributed-fs-interactive-shell:latest"
DEFAULT_FRONTEND="${1:-<frontend-host:port>}"

cd "$ROOT_DIR"

docker build -f Dockerfile.interactive -t "$IMAGE" . >/dev/null

echo "========================================="
echo " Remote Interactive Shell"
echo "========================================="
echo "Inside the shell, run commands like:"
echo "  /app/bin/distributed_client ${DEFAULT_FRONTEND} mkdir / mydir"
echo "  /app/bin/distributed_client ${DEFAULT_FRONTEND} create /mydir file.txt"
echo "  /app/bin/distributed_client ${DEFAULT_FRONTEND} write /mydir/file.txt 'Hello World'"
echo "  /app/bin/distributed_client ${DEFAULT_FRONTEND} read /mydir/file.txt"
echo "  /app/bin/distributed_client ${DEFAULT_FRONTEND} rm /mydir/file.txt"
echo "  /app/bin/distributed_client ${DEFAULT_FRONTEND} rmdir /mydir"
echo "  /app/bin/distributed_client ${DEFAULT_FRONTEND} ls /"
echo ""

docker run --rm -it "$IMAGE" bash
