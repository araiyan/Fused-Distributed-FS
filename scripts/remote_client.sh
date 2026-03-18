#!/bin/bash
set -euo pipefail

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <frontend-host:port> <command> [args...]"
    echo "Example: $0 ec2-44-201-10-20.compute-1.amazonaws.com:60051 mkdir / mydir"
    echo "Example: $0 ec2-44-201-10-20.compute-1.amazonaws.com:60051 rm /mydir/file.txt"
    echo "Example: $0 ec2-44-201-10-20.compute-1.amazonaws.com:60051 rmdir /mydir"
    echo "Example: $0 54.176.177.247:60051 upload ./test_10mb.mp4 / uploads/test_10mb.mp4 65536"
    exit 1
fi

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
IMAGE="fused-distributed-fs-interactive-shell:latest"
FRONTEND_ADDR="$1"
shift
COMMAND="$1"
shift

cd "$ROOT_DIR"

docker build -f Dockerfile.interactive -t "$IMAGE" . >/dev/null

to_docker_path() {
    local p="$1"
    if command -v cygpath >/dev/null 2>&1; then
        cygpath -m "$p"
    else
        printf "%s" "$p"
    fi
}

DOCKER_ARGS=(--rm -it)
CLIENT_ARGS=("$COMMAND")

# Special handling for upload so local file paths are available inside container.
# upload syntax: <local_file> <parent_path> <filename> [chunk_size]
if [ "$COMMAND" = "upload" ]; then
    if [ "$#" -lt 3 ]; then
        echo "Usage: $0 <frontend-host:port> upload <local_file> <parent_path> <filename> [chunk_size]"
        exit 1
    fi

    LOCAL_FILE="$1"
    shift

    if [ ! -f "$LOCAL_FILE" ]; then
        echo "Local file not found: $LOCAL_FILE"
        exit 1
    fi

    HOST_DIR="$(cd "$(dirname "$LOCAL_FILE")" && pwd)"
    BASE_NAME="$(basename "$LOCAL_FILE")"
    DOCKER_HOST_DIR="$(to_docker_path "$HOST_DIR")"
    CONTAINER_FILE="/upload-src/$BASE_NAME"

    DOCKER_ARGS+=( -v "$DOCKER_HOST_DIR:/upload-src" )
    CLIENT_ARGS+=( "$CONTAINER_FILE" )
    CLIENT_ARGS+=( "$@" )
else
    CLIENT_ARGS+=( "$@" )
fi

# On Git Bash/MSYS, absolute POSIX paths in args can be rewritten to Windows
# paths (e.g., /app -> C:/Program Files/Git/app). Disable conversion for docker.
MSYS_NO_PATHCONV=1 MSYS2_ARG_CONV_EXCL='*' \
docker run "${DOCKER_ARGS[@]}" "$IMAGE" /app/bin/distributed_client "$FRONTEND_ADDR" "${CLIENT_ARGS[@]}"
