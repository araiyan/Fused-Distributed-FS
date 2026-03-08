#!/bin/bash

compose() {
    if command -v docker-compose >/dev/null 2>&1; then
        docker-compose -f docker-compose-full.yml "$@"
    else
        docker compose -f docker-compose-full.yml "$@"
    fi
}

print_banner() {
    local title="$1"
    echo "========================================="
    echo " ${title}"
    echo "========================================="
    echo ""
}