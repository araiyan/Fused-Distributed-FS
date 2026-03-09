#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
COMPOSE_FILE="${ROOT_DIR}/docker-compose-ec2.yml"
ENV_FILE="${1:-${ROOT_DIR}/deploy/ec2/.env.node}"

if [ ! -f "$ENV_FILE" ]; then
    echo "Environment file not found: $ENV_FILE"
    exit 1
fi

cd "$ROOT_DIR"

if command -v docker-compose >/dev/null 2>&1; then
    if docker-compose --env-file "$ENV_FILE" -f "$COMPOSE_FILE" config >/dev/null 2>&1; then
        docker-compose --env-file "$ENV_FILE" -f "$COMPOSE_FILE" down
        exit 0
    fi

    backup_env=""
    if [ -f .env ]; then
        backup_env=".env.backup.$$.tmp"
        cp .env "$backup_env"
    fi

    cp "$ENV_FILE" .env
    docker-compose -f "$COMPOSE_FILE" down
    rm -f .env

    if [ -n "$backup_env" ] && [ -f "$backup_env" ]; then
        mv "$backup_env" .env
    fi
    exit 0
fi

docker compose --env-file "$ENV_FILE" -f "$COMPOSE_FILE" down
