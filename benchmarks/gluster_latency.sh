#!/bin/bash

MOUNT_DIR="/mnt/gluster-storage"
ITERATIONS=100
DATA="Benchmarking GlusterFS Replica 2"

# Check for 'bc' (needed for decimal math)
if ! command -v bc &> /dev/null; then
    echo "Installing 'bc' for math calculations..."
    sudo apt-get update && sudo apt-get install -y bc
fi

# Function to get time in milliseconds (3 decimal places)
get_time() {
    date +%s%3N
}

# Arrays to store results
declare -a mkdir_times create_times write_times read_times ls_times rm_times rmdir_times

echo "--- Starting 100-Iteration Benchmark on $MOUNT_DIR ---"
echo "Progress: [                                                  ] 0%"

for ((i=1; i<=ITERATIONS; i++))
do
    TEST_DIR="$MOUNT_DIR/bench_$i"
    TEST_FILE="$TEST_DIR/test.txt"

    # 1. MKDIR
    start=$(get_time)
    mkdir -p "$TEST_DIR"
    mkdir_times+=($(( $(get_time) - start )))

    # 2. CREATE
    start=$(get_time)
    touch "$TEST_FILE"
    create_times+=($(( $(get_time) - start )))

    # 3. WRITE + SYNC
    start=$(get_time)
    echo "$DATA" > "$TEST_FILE" && sync
    write_times+=($(( $(get_time) - start )))

    # 4. READ
    start=$(get_time)
    cat "$TEST_FILE" > /dev/null
    read_times+=($(( $(get_time) - start )))

    # 5. LS
    start=$(get_time)
    ls "$TEST_DIR" > /dev/null
    ls_times+=($(( $(get_time) - start )))

    # 6. RM FILE
    start=$(get_time)
    rm "$TEST_FILE"
    rm_times+=($(( $(get_time) - start )))

    # 7. RMDIR
    start=$(get_time)
    rmdir "$TEST_DIR"
    rmdir_times+=($(( $(get_time) - start )))

    # Update Progress Bar every 2 iterations
    if [ $((i % 2)) -eq 0 ]; then
        printf "\rProgress: [%-50s] %d%%" $(printf '#%.0s' $(seq 1 $((i/2)))) "$i"
    fi
done

echo -e "\n\n--- RESULTS (Average over $ITERATIONS runs) ---"

calc_avg() {
    local arr=("$@")
    local total=0
    for val in "${arr[@]}"; do total=$((total + val)); done
    echo "scale=2; $total / $ITERATIONS" | bc
}

echo "Average MKDIR:      $(calc_avg "${mkdir_times[@]}") ms"
echo "Average CREATE:     $(calc_avg "${create_times[@]}") ms"
echo "Average WRITE+SYNC: $(calc_avg "${write_times[@]}") ms"
echo "Average READ:       $(calc_avg "${read_times[@]}") ms"
echo "Average LS:         $(calc_avg "${ls_times[@]}") ms"
echo "Average RM FILE:    $(calc_avg "${rm_times[@]}") ms"
echo "Average RMDIR:      $(calc_avg "${rmdir_times[@]}") ms"
echo "------------------------------------------------"