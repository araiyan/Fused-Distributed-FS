# Run Distributed Filesystem Tests
# Simple PowerShell script to execute tests

param(
    [string]$Frontend = "frontend-1:60051"
)

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host " Running Distributed FS Tests" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Check if test-client is running
$container = docker ps -q -f name=test-client
if ([string]::IsNullOrEmpty($container)) {
    Write-Host "Starting test-client container..." -ForegroundColor Yellow
    docker-compose -f docker-compose-full.yml --profile test up -d test-client
    Start-Sleep -Seconds 5
}

Write-Host "Executing test suite..." -ForegroundColor Yellow
Write-Host ""

# Test 1: Mkdir
Write-Host "Test 1: Creating directories..." -ForegroundColor Green
docker exec test-client /app/bin/distributed_client $Frontend mkdir / videos
docker exec test-client /app/bin/distributed_client $Frontend mkdir / documents
docker exec test-client /app/bin/distributed_client $Frontend mkdir /videos shorts
Write-Host ""

# Test 2: Create files
Write-Host "Test 2: Creating files..." -ForegroundColor Green
docker exec test-client /app/bin/distributed_client $Frontend create /videos test_video.mp4
docker exec test-client /app/bin/distributed_client $Frontend create /documents readme.txt
docker exec test-client /app/bin/distributed_client $Frontend create /videos/shorts short1.mp4
Write-Host ""

# Test 3: Write data
Write-Host "Test 3: Writing data..." -ForegroundColor Green
docker exec test-client /app/bin/distributed_client $Frontend write /videos/test_video.mp4 "This is a test video file"
docker exec test-client /app/bin/distributed_client $Frontend write /documents/readme.txt "Welcome to Distributed FS"
docker exec test-client /app/bin/distributed_client $Frontend write /videos/shorts/short1.mp4 "Short video content"
Write-Host ""

# Test 4: Read data
Write-Host "Test 4: Reading data..." -ForegroundColor Green
Write-Host "--- /videos/test_video.mp4 ---" -ForegroundColor Yellow
docker exec test-client /app/bin/distributed_client $Frontend read /videos/test_video.mp4
Write-Host ""
Write-Host "--- /documents/readme.txt ---" -ForegroundColor Yellow
docker exec test-client /app/bin/distributed_client $Frontend read /documents/readme.txt
Write-Host ""
Write-Host "--- /videos/shorts/short1.mp4 ---" -ForegroundColor Yellow
docker exec test-client /app/bin/distributed_client $Frontend read /videos/shorts/short1.mp4
Write-Host ""

# Test 5: Append
Write-Host "Test 5: Appending data..." -ForegroundColor Green
docker exec test-client /app/bin/distributed_client $Frontend write /documents/readme.txt " - Line 2"
docker exec test-client /app/bin/distributed_client $Frontend write /documents/readme.txt " - Line 3"
Write-Host ""

Write-Host "Reading appended file..." -ForegroundColor Yellow
docker exec test-client /app/bin/distributed_client $Frontend read /documents/readme.txt
Write-Host ""

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host " Tests Complete!" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Cyan
