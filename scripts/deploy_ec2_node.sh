#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
COMPOSE_FILE="${ROOT_DIR}/docker-compose-ec2.yml"
ENV_FILE="${1:-${ROOT_DIR}/deploy/ec2/.env.node}"

if [ ! -f "$ENV_FILE" ]; then
    echo "Environment file not found: $ENV_FILE"
    echo "Create it from: ${ROOT_DIR}/deploy/ec2/.env.node.template"
    exit 1
fi

read_env_value() {
    local key="$1"
    local raw
    raw=$(grep -E "^[[:space:]]*${key}=" "$ENV_FILE" | tail -n1 | sed -E "s/^[[:space:]]*${key}=//")
    # Trim surrounding single/double quotes if present.
    raw="${raw#\"}"
    raw="${raw%\"}"
    raw="${raw#\'}"
    raw="${raw%\'}"
    printf "%s" "$raw"
}

NODE_ID="$(read_env_value NODE_ID)"
TOTAL_NODES="$(read_env_value TOTAL_NODES)"
PEER_NODES="$(read_env_value PEER_NODES)"
STORAGE_NODES="$(read_env_value STORAGE_NODES)"

if [ -z "${NODE_ID:-}" ] || [ -z "${TOTAL_NODES:-}" ]; then
    echo "Missing NODE_ID or TOTAL_NODES in $ENV_FILE"
    exit 1
fi

if [ -z "${PEER_NODES:-}" ]; then
    echo "PEER_NODES is empty in $ENV_FILE"
    echo "Expected format: 2@<ip-or-dns>:8001 3@<ip-or-dns>:8001"
    exit 1
fi

if [ -z "${STORAGE_NODES:-}" ]; then
    echo "STORAGE_NODES is empty in $ENV_FILE"
    echo "Expected format: <node1>:9000 <node2>:9000 <node3>:9000"
    exit 1
fi

cd "$ROOT_DIR"

run_compose() {
    local action="$1"

    if command -v docker-compose >/dev/null 2>&1; then
        # Prefer docker-compose on hosts where docker compose plugin is missing.
        if docker-compose --env-file "$ENV_FILE" -f "$COMPOSE_FILE" config >/dev/null 2>&1; then
            docker-compose --env-file "$ENV_FILE" -f "$COMPOSE_FILE" $action
            return
        fi

        # Fallback for older docker-compose versions without --env-file support.
        local backup_env=""
        if [ -f .env ]; then
            backup_env=".env.backup.$$.tmp"
            cp .env "$backup_env"
        fi

        cp "$ENV_FILE" .env
        docker-compose -f "$COMPOSE_FILE" $action
        rm -f .env

        if [ -n "$backup_env" ] && [ -f "$backup_env" ]; then
            mv "$backup_env" .env
        fi
        return
    fi

    docker compose --env-file "$ENV_FILE" -f "$COMPOSE_FILE" $action
}

echo "========================================="
echo " Deploying EC2 Node"
echo "========================================="
echo "Compose file: $COMPOSE_FILE"
echo "Env file:     $ENV_FILE"
echo "Node ID:      ${NODE_ID}"
echo "Peers:        ${PEER_NODES}"
echo "Storage:      ${STORAGE_NODES}"
echo ""

run_compose "up -d --build"

echo ""
echo "Container status:"
run_compose "ps"
