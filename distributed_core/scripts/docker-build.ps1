# docker-build.ps1 - Build and run the distributed core in Docker (PowerShell)

Write-Host "=== Building Distributed Core Docker Image ===" -ForegroundColor Green

Set-Location $PSScriptRoot

# Stop any running containers
Write-Host "Stopping any running containers..." -ForegroundColor Yellow
docker-compose down 2>$null

# Build the Docker image
Write-Host "Building Docker image..." -ForegroundColor Yellow
docker-compose build --no-cache

if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ Docker image built successfully" -ForegroundColor Green
} else {
    Write-Host "✗ Docker build failed" -ForegroundColor Red
    exit 1
}

# Start the cluster
Write-Host "Starting 3-node metadata cluster..." -ForegroundColor Yellow
docker-compose up -d

# Wait for containers to start
Write-Host "Waiting for containers to start..." -ForegroundColor Yellow
Start-Sleep -Seconds 3

# Check status
Write-Host ""
Write-Host "=== Cluster Status ===" -ForegroundColor Cyan
docker-compose ps

# Show logs
Write-Host ""
Write-Host "=== Recent Logs ===" -ForegroundColor Cyan
docker-compose logs --tail=30

Write-Host ""
Write-Host "=== Cluster Running! ===" -ForegroundColor Green
Write-Host ""
Write-Host "Metadata nodes:"
Write-Host "  - Node 1: localhost:7001"
Write-Host "  - Node 2: localhost:7002"
Write-Host "  - Node 3: localhost:7003"
Write-Host ""
Write-Host "Commands:"
Write-Host "  View logs:     docker-compose logs -f"
Write-Host "  View node 1:   docker logs -f metadata-node-1"
Write-Host "  Stop cluster:  docker-compose down"
Write-Host "  Shell access:  docker exec -it metadata-node-1 /bin/bash"
Write-Host ""
