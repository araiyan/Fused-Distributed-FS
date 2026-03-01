# Docker Check and Start Guide

Write-Host ""
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host " Distributed Filesystem - Docker Check" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Check Docker
Write-Host "Checking Docker status..." -ForegroundColor Yellow

docker ps >$null 2>&1
$dockerRunning = ($LASTEXITCODE -eq 0)

if ($dockerRunning) {
    Write-Host "✓ Docker is running!" -ForegroundColor Green
} else {
    Write-Host "✗ Docker is NOT running" -ForegroundColor Red
}

Write-Host ""

if (-not $dockerRunning) {
    Write-Host "=========================================" -ForegroundColor Red
    Write-Host " Docker Desktop is NOT Running" -ForegroundColor Red
    Write-Host "=========================================" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please follow these steps:" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "1. Open Docker Desktop application" -ForegroundColor White
    Write-Host "   (Search in Start Menu for Docker Desktop)" -ForegroundColor Gray
    Write-Host ""
    Write-Host "2. Wait for Docker to fully start" -ForegroundColor White
    Write-Host "   (Look for whale icon in system tray when ready)" -ForegroundColor Gray
    Write-Host ""
    Write-Host "3. Verify by running: docker ps" -ForegroundColor White
    Write-Host ""
    Write-Host "4. Once Docker is running, execute:" -ForegroundColor White
    Write-Host "   .\scripts\quick_start.ps1" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "=========================================" -ForegroundColor Red
    exit 1
}

Write-Host "=========================================" -ForegroundColor Green
Write-Host " Ready to Build and Start!" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Green
Write-Host ""
Write-Host "Docker is ready. You can now:" -ForegroundColor Yellow
Write-Host ""
Write-Host "Option 1 - Quick Start:" -ForegroundColor Cyan
Write-Host "  .\scripts\quick_start.ps1" -ForegroundColor White
Write-Host ""
Write-Host "Option 2 - Step by Step:" -ForegroundColor Cyan
Write-Host "  1. Build: docker-compose -f docker-compose-full.yml build" -ForegroundColor White
Write-Host "  2. Start: docker-compose -f docker-compose-full.yml up -d" -ForegroundColor White
Write-Host "  3. Test: .\scripts\run_tests.ps1" -ForegroundColor White
Write-Host ""
Write-Host "=========================================" -ForegroundColor Green
Write-Host ""

# Ask if user wants to continue
$response = Read-Host "Do you want to start the quick start now? (yes/no)"
if ($response -eq "yes" -or $response -eq "y") {
    Write-Host ""
    Write-Host "Starting quick start..." -ForegroundColor Green
    & ".\scripts\quick_start.ps1"
} else {
    Write-Host ""
    Write-Host "Run quick_start.ps1 when ready!" -ForegroundColor Yellow
    Write-Host ""
}

