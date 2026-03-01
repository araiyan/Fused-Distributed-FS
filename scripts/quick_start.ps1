# Quick Start - Build and Run Distributed Filesystem
# PowerShell script for Windows

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host " Distributed Filesystem - Quick Start" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Check if Docker is running
Write-Host "Checking Docker..." -ForegroundColor Yellow
docker ps > $null 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Docker is not running. Please start Docker Desktop." -ForegroundColor Red
    exit 1
}
Write-Host "✓ Docker is running" -ForegroundColor Green
Write-Host ""

# Stop any existing containers
Write-Host "Stopping existing containers (if any)..." -ForegroundColor Yellow
docker-compose -f docker-compose-full.yml down 2>$null
Write-Host "✓ Cleaned up" -ForegroundColor Green
Write-Host ""

# Build all images
Write-Host "Building Docker images (this may take a few minutes)..." -ForegroundColor Yellow
docker-compose -f docker-compose-full.yml build
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Docker build failed" -ForegroundColor Red
    exit 1
}
Write-Host "✓ Docker images built" -ForegroundColor Green
Write-Host ""

# Start all services
Write-Host "Starting all services..." -ForegroundColor Yellow
docker-compose -f docker-compose-full.yml up -d
if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Failed to start services" -ForegroundColor Red
    exit 1
}
Write-Host "✓ Services started" -ForegroundColor Green
Write-Host ""

# Wait for initialization
Write-Host "Waiting for services to initialize (20 seconds)..." -ForegroundColor Yellow
Start-Sleep -Seconds 20
Write-Host ""

# Check status
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host " System Status" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
docker-compose -f docker-compose-full.yml ps
Write-Host ""

# Show logs preview
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host " Recent Logs (Frontend 1)" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
docker logs frontend-1 --tail 20
Write-Host ""

Write-Host "=========================================" -ForegroundColor Green
Write-Host " Distributed Filesystem is Ready!" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Green
Write-Host ""
Write-Host "System Components:" -ForegroundColor Yellow
Write-Host "  → Metadata Cluster: 3 nodes (ports 7001-7003)" -ForegroundColor White
Write-Host "  → Storage Nodes: 3 nodes (ports 50051-50053)" -ForegroundColor White
Write-Host "  → Frontend Coordinators: 3 nodes (ports 60051-60053)" -ForegroundColor White
Write-Host ""
Write-Host "Next Steps:" -ForegroundColor Yellow
Write-Host ""
Write-Host "1. Start test client:" -ForegroundColor Cyan
Write-Host "   docker-compose -f docker-compose-full.yml --profile test up -d test-client" -ForegroundColor White
Write-Host ""
Write-Host "2. Run tests:" -ForegroundColor Cyan
Write-Host "   docker exec -it test-client bash" -ForegroundColor White
Write-Host "   cd /app/tests" -ForegroundColor White
Write-Host "   ./distributed_test.sh frontend-1:60051" -ForegroundColor White
Write-Host ""
Write-Host "3. Manual testing:" -ForegroundColor Cyan
Write-Host "   docker exec test-client /app/bin/distributed_client frontend-1:60051 mkdir / videos" -ForegroundColor White
Write-Host "   docker exec test-client /app/bin/distributed_client frontend-1:60051 create /videos test.mp4" -ForegroundColor White
Write-Host "   docker exec test-client /app/bin/distributed_client frontend-1:60051 write /videos/test.mp4 'Hello World'" -ForegroundColor White
Write-Host "   docker exec test-client /app/bin/distributed_client frontend-1:60051 read /videos/test.mp4" -ForegroundColor White
Write-Host ""
Write-Host "4. View logs:" -ForegroundColor Cyan
Write-Host "   docker-compose -f docker-compose-full.yml logs -f frontend-1" -ForegroundColor White
Write-Host "   docker logs storage-node-1 -f" -ForegroundColor White
Write-Host ""
Write-Host "5. Stop system:" -ForegroundColor Cyan
Write-Host "   docker-compose -f docker-compose-full.yml down" -ForegroundColor White
Write-Host ""
