# stop_cluster.ps1 - PowerShell script for Windows
# Stop the distributed metadata cluster

Set-Location $PSScriptRoot

Write-Host "=== Stopping Distributed Metadata Cluster ===" -ForegroundColor Yellow

docker-compose down

Write-Host "✓ Cluster stopped" -ForegroundColor Green
Write-Host ""
Write-Host "To remove volumes (DELETES ALL DATA):"
Write-Host "  docker-compose down -v"
