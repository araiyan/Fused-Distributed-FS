#!/bin/bash
# Run Distributed Filesystem Tests

FRONTEND_ADDR="${1:-frontend-1:60051}"

echo "========================================="
echo " Running Distributed FS Tests"
echo "========================================="
echo ""

# Check if test-client is running
if ! docker ps -q -f name=test-client > /dev/null; then
    echo "Starting test-client container..."
    docker-compose -f docker-compose-full.yml --profile test up -d test-client
    sleep 5
fi

echo "Executing test suite..."
echo ""

# Test 1: Mkdir
echo "Test 1: Creating directories..."
docker exec test-client /app/bin/distributed_client $FRONTEND_ADDR mkdir / videos || true
docker exec test-client /app/bin/distributed_client $FRONTEND_ADDR mkdir / documents || true
docker exec test-client /app/bin/distributed_client $FRONTEND_ADDR mkdir /videos shorts || true
echo ""

# Test 2: Create files
echo "Test 2: Creating files..."
docker exec test-client /app/bin/distributed_client $FRONTEND_ADDR create /videos test_video.mp4
docker exec test-client /app/bin/distributed_client $FRONTEND_ADDR create /documents readme.txt
docker exec test-client /app/bin/distributed_client $FRONTEND_ADDR create /videos/shorts short1.mp4
echo ""

# Test 3: Write data
echo "Test 3: Writing data..."
docker exec test-client /app/bin/distributed_client $FRONTEND_ADDR write /videos/test_video.mp4 "This is a test video file"
docker exec test-client /app/bin/distributed_client $FRONTEND_ADDR write /documents/readme.txt "Welcome to Distributed FS"
docker exec test-client /app/bin/distributed_client $FRONTEND_ADDR write /videos/shorts/short1.mp4 "Short video content"
echo ""

# Test 4: Read data
echo "Test 4: Reading data..."
echo "--- /videos/test_video.mp4 ---"
docker exec test-client /app/bin/distributed_client $FRONTEND_ADDR read /videos/test_video.mp4
echo ""
echo "--- /documents/readme.txt ---"
docker exec test-client /app/bin/distributed_client $FRONTEND_ADDR read /documents/readme.txt
echo ""
echo "--- /videos/shorts/short1.mp4 ---"
docker exec test-client /app/bin/distributed_client $FRONTEND_ADDR read /videos/shorts/short1.mp4
echo ""

# Test 5: Append
echo "Test 5: Appending data..."
docker exec test-client /app/bin/distributed_client $FRONTEND_ADDR write /documents/readme.txt " - Line 2"
docker exec test-client /app/bin/distributed_client $FRONTEND_ADDR write /documents/readme.txt " - Line 3"
echo ""

echo "Reading appended file..."
docker exec test-client /app/bin/distributed_client $FRONTEND_ADDR read /documents/readme.txt
echo ""

echo "========================================="
echo " Tests Complete!"
echo "========================================="
