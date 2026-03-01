# Build and start the complete distributed filesystem (PowerShell)

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host " Building Distributed Filesystem" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Step 1: Build distributed core library
Write-Host "[1/4] Building distributed core..." -ForegroundColor Yellow
Push-Location distributed_core
make clean
make all
Pop-Location
Write-Host "✓ Distributed core built" -ForegroundColor Green
Write-Host ""

# Step 2: Generate protobuf code
Write-Host "[2/4] Generating protobuf code..." -ForegroundColor Yellow
make proto
Write-Host "✓ Protobuf code generated" -ForegroundColor Green
Write-Host ""

# Step 3: Build Docker images
Write-Host "[3/4] Building Docker images..." -ForegroundColor Yellow
docker-compose -f docker-compose-full.yml build
Write-Host "✓ Docker images built" -ForegroundColor Green
Write-Host ""

# Step 4: Start all containers
Write-Host "[4/4] Starting all containers..." -ForegroundColor Yellow
docker-compose -f docker-compose-full.yml up -d
Write-Host "✓ All containers started" -ForegroundColor Green
Write-Host ""

# Wait for services to be ready
Write-Host "Waiting for services to initialize (15 seconds)..." -ForegroundColor Yellow
Start-Sleep -Seconds 15

# Show status
Write-Host ""
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host " Container Status" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
docker-compose -f docker-compose-full.yml ps
Write-Host ""

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host " Distributed Filesystem is Running!" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Components:" -ForegroundColor Yellow
Write-Host "  Metadata Cluster: 3 nodes (ports 7001-7003)"
Write-Host "  Storage Nodes: 3 nodes (ports 50051-50053)"
Write-Host "  Frontend Coordinators: 3 nodes (ports 60051-60053)"
Write-Host ""
Write-Host "To run tests:" -ForegroundColor Yellow
Write-Host "  docker-compose -f docker-compose-full.yml --profile test up -d test-client"
Write-Host "  docker exec -it test-client bash"
Write-Host "  ./tests/distributed_test.sh frontend-1:60051"
Write-Host ""
Write-Host "To view logs:" -ForegroundColor Yellow
Write-Host "  docker-compose -f docker-compose-full.yml logs -f frontend-1"
Write-Host "  docker-compose -f docker-compose-full.yml logs -f storage-node-1"
Write-Host ""
Write-Host "To stop:" -ForegroundColor Yellow
Write-Host "  docker-compose -f docker-compose-full.yml down"
Write-Host ""
