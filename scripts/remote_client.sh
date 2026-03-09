#!/bin/bash
set -euo pipefail

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <frontend-host:port> <command> [args...]"
    echo "Example: $0 ec2-44-201-10-20.compute-1.amazonaws.com:60051 mkdir / mydir"
    exit 1
fi

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE="fused-distributed-fs-interactive-shell:latest"
FRONTEND_ADDR="$1"
shift

cd "$ROOT_DIR"

docker build -f Dockerfile.interactive -t "$IMAGE" . >/dev/null

# On Git Bash/MSYS, absolute POSIX paths in args can be rewritten to Windows
# paths (e.g., /app -> C:/Program Files/Git/app). Disable conversion for docker.
MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' \
docker run --rm -it "$IMAGE" /app/bin/distributed_client "$FRONTEND_ADDR" "$@"
