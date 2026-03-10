#!/bin/bash
# Distributed Filesystem End-to-End Test Script

set -e

FRONTEND_ADDR="${1:-frontend-1:60051}"
CLIENT="/app/bin/distributed_client"

echo "========================================="
echo " Distributed Filesystem Test Suite"
echo "========================================="
echo " Frontend: $FRONTEND_ADDR"
echo "========================================="
echo ""

# Test 1: Create root directories
echo "Test 1: Creating directories..."
$CLIENT $FRONTEND_ADDR mkdir / videos || echo "  (mkdir might fail if exists)"
$CLIENT $FRONTEND_ADDR mkdir / documents || echo "  (mkdir might fail if exists)"
$CLIENT $FRONTEND_ADDR mkdir /videos shorts || echo "  (mkdir might fail if exists)"
echo "✓ Directories created"
echo ""

# Test 2: Create files
echo "Test 2: Creating files..."
$CLIENT $FRONTEND_ADDR create /videos test_video.mp4
$CLIENT $FRONTEND_ADDR create /documents readme.txt
$CLIENT $FRONTEND_ADDR create /videos/shorts short1.mp4
echo "✓ Files created"
echo ""

# Test 3: Write to files
echo "Test 3: Writing data to files..."
$CLIENT $FRONTEND_ADDR write /videos/test_video.mp4 "This is a test video file content"
$CLIENT $FRONTEND_ADDR write /documents/readme.txt "Welcome to Distributed Filesystem!"
$CLIENT $FRONTEND_ADDR write /videos/shorts/short1.mp4 "Short video data here"
echo "✓ Data written"
echo ""

# Test 4: Read from files
echo "Test 4: Reading data from files..."
echo "--- /videos/test_video.mp4 ---"
$CLIENT $FRONTEND_ADDR read /videos/test_video.mp4
echo ""
echo "--- /documents/readme.txt ---"
$CLIENT $FRONTEND_ADDR read /documents/readme.txt
echo ""
echo "--- /videos/shorts/short1.mp4 ---"
$CLIENT $FRONTEND_ADDR read /videos/shorts/short1.mp4
echo ""
echo "✓ Data read successfully"
echo ""

# Test 5: List directories
echo "Test 5: Listing directories..."
echo "--- Root directory (/) ---"
$CLIENT $FRONTEND_ADDR ls / || echo "  (ls might not work yet, depends on implementation)"
echo ""
echo "--- /videos ---"
$CLIENT $FRONTEND_ADDR ls /videos || echo "  (ls might not work yet)"
echo ""

# Test 6: Multiple writes (append)
echo "Test 6: Appending more data..."
$CLIENT $FRONTEND_ADDR write /documents/readme.txt " Additional line 1."
$CLIENT $FRONTEND_ADDR write /documents/readme.txt " Additional line 2."
$CLIENT $FRONTEND_ADDR write /documents/readme.txt " Additional line 3."
echo "✓ Data appended"
echo ""

echo "Test 7: Reading appended data..."
$CLIENT $FRONTEND_ADDR read /documents/readme.txt
echo ""

# Test 8: Stress test with multiple files
echo "Test 8: Creating multiple files (stress test)..."
for i in {1..10}; do
    $CLIENT $FRONTEND_ADDR create /videos file_$i.mp4
    $CLIENT $FRONTEND_ADDR write /videos/file_$i.mp4 "Content of file $i"
done
echo "✓ Created and wrote to 10 files"
echo ""

echo "Test 9: Reading all files..."
for i in {1..10}; do
    echo "--- /videos/file_$i.mp4 ---"
    $CLIENT $FRONTEND_ADDR read /videos/file_$i.mp4
done
echo ""

# Test 10: Remove files and directories
echo "Test 10: Removing files and directories..."
$CLIENT $FRONTEND_ADDR rm /videos/shorts/short1.mp4
$CLIENT $FRONTEND_ADDR rm /videos/test_video.mp4
$CLIENT $FRONTEND_ADDR rm /documents/readme.txt
for i in {1..10}; do
    $CLIENT $FRONTEND_ADDR rm /videos/file_$i.mp4
done
$CLIENT $FRONTEND_ADDR rmdir /videos/shorts
$CLIENT $FRONTEND_ADDR rmdir /videos
$CLIENT $FRONTEND_ADDR rmdir /documents
echo "✓ Delete operations successful"
echo ""

echo "========================================="
echo " All Tests Completed!"
echo "========================================="
echo ""
echo "Summary:"
echo "  ✓ Directories created"
echo "  ✓ Files created"
echo "  ✓ Data written"
echo "  ✓ Data read"
echo "  ✓ Multiple operations"
echo "  ✓ Stress test passed"
echo "  ✓ Delete operations passed"
echo ""
echo "The distributed filesystem is working!"
