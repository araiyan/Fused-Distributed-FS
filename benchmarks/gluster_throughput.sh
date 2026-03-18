#!/bin/bash

MOUNT_DIR="/mnt/gluster-storage"
ITERATIONS=10
SIZES=("1M" "10M" "100M")

# Function to get time in seconds with nanosecond precision
get_time() {
    date +%s.%N
}

echo "--- Starting Throughput Test (10 Iterations per size) ---"
echo "Target: $MOUNT_DIR"
echo "---------------------------------------------------------"

for SIZE in "${SIZES[@]}"
do
    echo "Testing Size: $SIZE"
    TOTAL_UPLOAD_TIME=0
    TOTAL_DELETE_TIME=0

    for ((i=1; i<=ITERATIONS; i++))
    do
        TEST_FILE="$MOUNT_DIR/throughput_test_${SIZE}_$i.bin"

        # 1. UPLOAD (Write)
        # bs=1M means we write in 1MB chunks; count determines total size
        # conv=fdatasync ensures data is physically written to the remote brick
        start=$(get_time)
        dd if=/dev/urandom of="$TEST_FILE" bs="$SIZE" count=1 conv=fdatasync status=none
        end=$(get_time)
        
        diff=$(echo "$end - $start" | bc)
        TOTAL_UPLOAD_TIME=$(echo "$TOTAL_UPLOAD_TIME + $diff" | bc)

        # 2. DELETE (rm)
        start=$(get_time)
        rm "$TEST_FILE"
        end=$(get_time)
        
        diff=$(echo "$end - $start" | bc)
        TOTAL_DELETE_TIME=$(echo "$TOTAL_DELETE_TIME + $diff" | bc)
        
        printf "."
    done

    # Calculate Averages
    AVG_UP=$(echo "scale=3; $TOTAL_UPLOAD_TIME / $ITERATIONS" | bc)
    AVG_DEL=$(echo "scale=3; $TOTAL_DELETE_TIME / $ITERATIONS" | bc)
    
    # Convert SIZE string to numeric bytes for MB/s calculation
    NUM_SIZE=$(echo $SIZE | sed 's/M//')
    THROUGHPUT=$(echo "scale=2; $NUM_SIZE / $AVG_UP" | bc)

    echo -e "\nAverage $SIZE Upload: $AVG_UP sec ($THROUGHPUT MB/s)"
    echo "Average $SIZE Delete: $AVG_DEL sec"
    echo "---------------------------------------------------------"
done

echo "Test Complete."