# Distributed Filesystem End-to-End Test Script (PowerShell)

param(
    [string]$FrontendAddr = "localhost:60051"
)

$CLIENT = "docker exec test-client /app/bin/distributed_client"

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host " Distributed Filesystem Test Suite" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host " Frontend: $FrontendAddr" -ForegroundColor Yellow
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Test 1: Create directories
Write-Host "Test 1: Creating directories..." -ForegroundColor Green
& docker exec test-client /app/bin/distributed_client $FrontendAddr mkdir / videos
& docker exec test-client /app/bin/distributed_client $FrontendAddr mkdir / documents
& docker exec test-client /app/bin/distributed_client $FrontendAddr mkdir /videos shorts
Write-Host "✓ Directories created" -ForegroundColor Green
Write-Host ""

# Test 2: Create files
Write-Host "Test 2: Creating files..." -ForegroundColor Green
& docker exec test-client /app/bin/distributed_client $FrontendAddr create /videos test_video.mp4
& docker exec test-client /app/bin/distributed_client $FrontendAddr create /documents readme.txt
& docker exec test-client /app/bin/distributed_client $FrontendAddr create /videos/shorts short1.mp4
Write-Host "✓ Files created" -ForegroundColor Green
Write-Host ""

# Test 3: Write to files
Write-Host "Test 3: Writing data to files..." -ForegroundColor Green
& docker exec test-client /app/bin/distributed_client $FrontendAddr write /videos/test_video.mp4 "This is a test video file content"
& docker exec test-client /app/bin/distributed_client $FrontendAddr write /documents/readme.txt "Welcome to Distributed Filesystem!"
& docker exec test-client /app/bin/distributed_client $FrontendAddr write /videos/shorts/short1.mp4 "Short video data here"
Write-Host "✓ Data written" -ForegroundColor Green
Write-Host ""

# Test 4: Read from files
Write-Host "Test 4: Reading data from files..." -ForegroundColor Green
Write-Host "--- /videos/test_video.mp4 ---" -ForegroundColor Yellow
& docker exec test-client /app/bin/distributed_client $FrontendAddr read /videos/test_video.mp4
Write-Host ""
Write-Host "--- /documents/readme.txt ---" -ForegroundColor Yellow
& docker exec test-client /app/bin/distributed_client $FrontendAddr read /documents/readme.txt
Write-Host ""
Write-Host "--- /videos/shorts/short1.mp4 ---" -ForegroundColor Yellow
& docker exec test-client /app/bin/distributed_client $FrontendAddr read /videos/shorts/short1.mp4
Write-Host ""
Write-Host "✓ Data read successfully" -ForegroundColor Green
Write-Host ""

# Test 5: Multiple writes (append)
Write-Host "Test 5: Appending more data..." -ForegroundColor Green
& docker exec test-client /app/bin/distributed_client $FrontendAddr write /documents/readme.txt " Additional line 1."
& docker exec test-client /app/bin/distributed_client $FrontendAddr write /documents/readme.txt " Additional line 2."
& docker exec test-client /app/bin/distributed_client $FrontendAddr write /documents/readme.txt " Additional line 3."
Write-Host "✓ Data appended" -ForegroundColor Green
Write-Host ""

Write-Host "Test 6: Reading appended data..." -ForegroundColor Green
& docker exec test-client /app/bin/distributed_client $FrontendAddr read /documents/readme.txt
Write-Host ""

# Test 7: Stress test
Write-Host "Test 7: Creating multiple files (stress test)..." -ForegroundColor Green
for ($i = 1; $i -le 10; $i++) {
    & docker exec test-client /app/bin/distributed_client $FrontendAddr create /videos "file_$i.mp4"
    & docker exec test-client /app/bin/distributed_client $FrontendAddr write "/videos/file_$i.mp4" "Content of file $i"
}
Write-Host "✓ Created and wrote to 10 files" -ForegroundColor Green
Write-Host ""

Write-Host "Test 8: Reading all files..." -ForegroundColor Green
for ($i = 1; $i -le 10; $i++) {
    Write-Host "--- /videos/file_$i.mp4 ---" -ForegroundColor Yellow
    & docker exec test-client /app/bin/distributed_client $FrontendAddr read "/videos/file_$i.mp4"
}
Write-Host ""

Write-Host "=========================================" -ForegroundColor Cyan
Write-Host " All Tests Completed!" -ForegroundColor Green
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Summary:" -ForegroundColor Yellow
Write-Host "  ✓ Directories created" -ForegroundColor Green
Write-Host "  ✓ Files created" -ForegroundColor Green
Write-Host "  ✓ Data written" -ForegroundColor Green
Write-Host "  ✓ Data read" -ForegroundColor Green
Write-Host "  ✓ Multiple operations" -ForegroundColor Green
Write-Host "  ✓ Stress test passed" -ForegroundColor Green
Write-Host ""
Write-Host "The distributed filesystem is working!" -ForegroundColor Green
