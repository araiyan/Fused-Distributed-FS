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

cd "$ROOT_DIR"

echo "========================================="
echo " Deploying EC2 Node"
echo "========================================="
echo "Compose file: $COMPOSE_FILE"
echo "Env file:     $ENV_FILE"
echo ""

docker compose --env-file "$ENV_FILE" -f "$COMPOSE_FILE" up -d --build

echo ""
echo "Container status:"
docker compose --env-file "$ENV_FILE" -f "$COMPOSE_FILE" ps
