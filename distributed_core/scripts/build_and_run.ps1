# build_and_run.ps1 - PowerShell script for Windows
# Build and run the distributed metadata cluster

Write-Host "=== Building Distributed Core Metadata Cluster ===" -ForegroundColor Green

# Navigate to distributed_core directory
Set-Location $PSScriptRoot

# Clean previous builds
Write-Host "Cleaning previous builds..." -ForegroundColor Yellow
if (Test-Path "*.o") { Remove-Item *.o -Force }
docker-compose down -v 2>$null

# Build Docker images
Write-Host "Building Docker images..." -ForegroundColor Yellow
docker-compose build

if ($LASTEXITCODE -eq 0) {
    Write-Host "✓ Docker images built" -ForegroundColor Green
} else {
    Write-Host "✗ Docker build failed" -ForegroundColor Red
    exit 1
}

# Start the cluster
Write-Host "Starting 3-node metadata cluster..." -ForegroundColor Yellow
docker-compose up -d

# Wait for nodes to start
Write-Host "Waiting for nodes to start..." -ForegroundColor Yellow
Start-Sleep -Seconds 5

# Check status
Write-Host ""
Write-Host "=== Cluster Status ===" -ForegroundColor Cyan
docker-compose ps

# Show logs
Write-Host ""
Write-Host "=== Recent Logs ===" -ForegroundColor Cyan
docker-compose logs --tail=20

Write-Host ""
Write-Host "=== Cluster is running! ===" -ForegroundColor Green
Write-Host "Metadata nodes:"
Write-Host "  - Node 1: localhost:7001"
Write-Host "  - Node 2: localhost:7002"
Write-Host "  - Node 3: localhost:7003"
Write-Host ""
Write-Host "Commands:"
Write-Host "  View logs:    docker-compose logs -f"
Write-Host "  Stop cluster: docker-compose down"
Write-Host "  Restart:      docker-compose restart"
Write-Host ""
