# Simple Docker Check Script

Write-Host "Checking Docker..." -ForegroundColor Yellow

docker ps >$null 2>&1

if ($LASTEXITCODE -eq 0) {
    Write-Host "Docker is running!" -ForegroundColor Green
    Write-Host ""
    Write-Host "To start the system, run:" -ForegroundColor Yellow
    Write-Host "  .\scripts\quick_start.ps1" -ForegroundColor Cyan
    Write-Host ""
} else {
    Write-Host "Docker is NOT running!" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please start Docker Desktop first." -ForegroundColor Yellow
    Write-Host "Then run: .\scripts\quick_start.ps1" -ForegroundColor Cyan
    Write-Host ""
}
