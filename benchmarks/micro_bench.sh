#!/bin/bash

#############################################################################
# FUSE Filesystem Microbenchmarks
# Compares FUSED filesystem against tmpfs baseline
#############################################################################

set -e

# Configuration - USE ABSOLUTE PATHS
FUSE_MOUNT="/mnt/fused"
TMPFS_MOUNT="/dev/shm/benchmark_test"
OUTPUT_CSV="/app/benchmarks/benchmark_results.csv"

# Test parameters (YouTube Shorts workload)
NUM_SMALL_FILES=1000      # For file creation test
NUM_MEDIUM_FILES=100      # For read/write tests
FILE_SIZE_MB=20           # Typical YouTube Short size
NUM_THREADS=10            # Concurrent operations

# Colors for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

#############################################################################
# Helper Functions
#############################################################################

log() {
    echo -e "${BLUE}[BENCHMARK]${NC} $1"
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Write CSV header
init_csv() {
    echo "filesystem,test,throughput_mbps,avg_latency_ms,total_time_sec,files_per_sec" > $OUTPUT_CSV
    log "Initialized $OUTPUT_CSV"
}

# Append result to CSV
record_result() {
    local fs=$1
    local test=$2
    local throughput=$3
    local latency=$4
    local total_time=$5
    local files_per_sec=$6
    
    echo "$fs,$test,$throughput,$latency,$total_time,$files_per_sec" >> $OUTPUT_CSV
}

# Clean up test directory
cleanup_test_dir() {
    local dir=$1
    log "Cleaning up $dir"
    rm -rf $dir/* 2>/dev/null || true
    sync
    sleep 1
}

#############################################################################
# Test 1: Sequential Write (Stress Test)
#############################################################################

test_sequential_write() {
    local test_dir=$1
    local fs_name=$2
    
    log "Running Sequential Write Test on $fs_name..."
    cleanup_test_dir $test_dir
    
    local start_time=$(date +%s.%N)
    
    for i in $(seq 1 $NUM_MEDIUM_FILES); do
        dd if=/dev/zero of=$test_dir/video_$i.mp4 bs=1M count=$FILE_SIZE_MB 2>/dev/null
    done
    
    sync
    local end_time=$(date +%s.%N)
    
    # Calculate metrics
    local total_time=$(echo "$end_time - $start_time" | bc)
    local total_mb=$(echo "$NUM_MEDIUM_FILES * $FILE_SIZE_MB" | bc)
    local throughput=$(echo "scale=2; $total_mb / $total_time" | bc)
    local avg_latency=$(echo "scale=2; ($total_time * 1000) / $NUM_MEDIUM_FILES" | bc)
    local files_per_sec=$(echo "scale=2; $NUM_MEDIUM_FILES / $total_time" | bc)
    
    success "Sequential Write: $throughput MB/s, ${avg_latency}ms avg latency"
    record_result "$fs_name" "sequential_write" "$throughput" "$avg_latency" "$total_time" "$files_per_sec"
}

#############################################################################
# Test 2: Sequential Read (Stress Test)
#############################################################################

test_sequential_read() {
    local test_dir=$1
    local fs_name=$2
    
    log "Running Sequential Read Test on $fs_name..."
    
    # First, create files to read
    cleanup_test_dir $test_dir
    for i in $(seq 1 $NUM_MEDIUM_FILES); do
        dd if=/dev/zero of=$test_dir/video_$i.mp4 bs=1M count=$FILE_SIZE_MB 2>/dev/null
    done
    sync
    
    # Clear page cache (if possible)
    echo 3 > /proc/sys/vm/drop_caches 2>/dev/null || warn "Cannot clear cache (need root)"
    
    local start_time=$(date +%s.%N)
    
    for i in $(seq 1 $NUM_MEDIUM_FILES); do
        dd if=$test_dir/video_$i.mp4 of=/dev/null bs=1M 2>/dev/null
    done
    
    local end_time=$(date +%s.%N)
    
    # Calculate metrics
    local total_time=$(echo "$end_time - $start_time" | bc)
    local total_mb=$(echo "$NUM_MEDIUM_FILES * $FILE_SIZE_MB" | bc)
    local throughput=$(echo "scale=2; $total_mb / $total_time" | bc)
    local avg_latency=$(echo "scale=2; ($total_time * 1000) / $NUM_MEDIUM_FILES" | bc)
    local files_per_sec=$(echo "scale=2; $NUM_MEDIUM_FILES / $total_time" | bc)
    
    success "Sequential Read: $throughput MB/s, ${avg_latency}ms avg latency"
    record_result "$fs_name" "sequential_read" "$throughput" "$avg_latency" "$total_time" "$files_per_sec"
}

#############################################################################
# Test 3: File Creation (Metadata Overhead)
#############################################################################

test_file_creation() {
    local test_dir=$1
    local fs_name=$2
    
    log "Running File Creation Test on $fs_name..."
    cleanup_test_dir $test_dir
    
    local start_time=$(date +%s.%N)
    
    for i in $(seq 1 $NUM_SMALL_FILES); do
        touch $test_dir/file_$i.txt 2>/dev/null
    done
    
    sync
    local end_time=$(date +%s.%N)
    
    # Calculate metrics
    local total_time=$(echo "$end_time - $start_time" | bc)
    local avg_latency=$(echo "scale=2; ($total_time * 1000) / $NUM_SMALL_FILES" | bc)
    local files_per_sec=$(echo "scale=2; $NUM_SMALL_FILES / $total_time" | bc)
    
    success "File Creation: $files_per_sec files/sec, ${avg_latency}ms avg latency"
    record_result "$fs_name" "file_creation" "N/A" "$avg_latency" "$total_time" "$files_per_sec"
}

#############################################################################
# Test 4: Mixed Workload (70% Read, 30% Write, Interleaved)
#############################################################################

test_mixed_workload() {
    local test_dir=$1
    local fs_name=$2
    
    log "Running Mixed Workload Test on $fs_name (70% read, 30% write)..."
    cleanup_test_dir $test_dir
    
    # Pre-populate some files for reading
    local num_existing=70
    for i in $(seq 1 $num_existing); do
        dd if=/dev/zero of=$test_dir/existing_$i.mp4 bs=1M count=$FILE_SIZE_MB 2>/dev/null
    done
    sync
    
    local start_time=$(date +%s.%N)
    
    # Interleaved operations: 7 reads, 3 writes, repeat
    local ops_done=0
    for batch in $(seq 1 10); do
        # 7 reads
        for i in $(seq 1 7); do
            local file_num=$((($batch - 1) * 7 + $i))
            if [ $file_num -le $num_existing ]; then
                dd if=$test_dir/existing_$file_num.mp4 of=/dev/null bs=1M 2>/dev/null
                ops_done=$((ops_done + 1))
            fi
        done
        
        # 3 writes
        for i in $(seq 1 3); do
            local file_num=$((($batch - 1) * 3 + $i))
            dd if=/dev/zero of=$test_dir/new_$file_num.mp4 bs=1M count=$FILE_SIZE_MB 2>/dev/null
            ops_done=$((ops_done + 1))
        done
    done
    
    sync
    local end_time=$(date +%s.%N)
    
    # Calculate metrics
    local total_time=$(echo "$end_time - $start_time" | bc)
    local total_mb=$(echo "$ops_done * $FILE_SIZE_MB" | bc)
    local throughput=$(echo "scale=2; $total_mb / $total_time" | bc)
    local avg_latency=$(echo "scale=2; ($total_time * 1000) / $ops_done" | bc)
    local ops_per_sec=$(echo "scale=2; $ops_done / $total_time" | bc)
    
    success "Mixed Workload: $throughput MB/s, ${avg_latency}ms avg latency"
    record_result "$fs_name" "mixed_workload" "$throughput" "$avg_latency" "$total_time" "$ops_per_sec"
}

#############################################################################
# Test 5: Concurrent Operations (Multithreaded)
#############################################################################

test_concurrent_write() {
    local test_dir=$1
    local fs_name=$2
    
    log "Running Concurrent Write Test on $fs_name ($NUM_THREADS threads)..."
    cleanup_test_dir $test_dir
    
    local start_time=$(date +%s.%N)
    
    # Spawn multiple background processes
    for thread in $(seq 1 $NUM_THREADS); do
        (
            for i in $(seq 1 10); do
                dd if=/dev/zero of=$test_dir/thread${thread}_file${i}.mp4 bs=1M count=$FILE_SIZE_MB 2>/dev/null
            done
        ) &
    done
    
    # Wait for all threads
    wait
    
    sync
    local end_time=$(date +%s.%N)
    
    # Calculate metrics
    local total_files=$(echo "$NUM_THREADS * 10" | bc)
    local total_time=$(echo "$end_time - $start_time" | bc)
    local total_mb=$(echo "$total_files * $FILE_SIZE_MB" | bc)
    local throughput=$(echo "scale=2; $total_mb / $total_time" | bc)
    local avg_latency=$(echo "scale=2; ($total_time * 1000) / $total_files" | bc)
    local files_per_sec=$(echo "scale=2; $total_files / $total_time" | bc)
    
    success "Concurrent Write: $throughput MB/s aggregate, ${avg_latency}ms avg latency"
    record_result "$fs_name" "concurrent_write" "$throughput" "$avg_latency" "$total_time" "$files_per_sec"
}

test_concurrent_read() {
    local test_dir=$1
    local fs_name=$2
    
    log "Running Concurrent Read Test on $fs_name ($NUM_THREADS threads)..."
    
    # Pre-populate files
    cleanup_test_dir $test_dir
    for thread in $(seq 1 $NUM_THREADS); do
        for i in $(seq 1 10); do
            dd if=/dev/zero of=$test_dir/thread${thread}_file${i}.mp4 bs=1M count=$FILE_SIZE_MB 2>/dev/null
        done
    done
    sync
    
    local start_time=$(date +%s.%N)
    
    # Spawn multiple readers
    for thread in $(seq 1 $NUM_THREADS); do
        (
            for i in $(seq 1 10); do
                dd if=$test_dir/thread${thread}_file${i}.mp4 of=/dev/null bs=1M 2>/dev/null
            done
        ) &
    done
    
    # Wait for all threads
    wait
    
    local end_time=$(date +%s.%N)
    
    # Calculate metrics
    local total_files=$(echo "$NUM_THREADS * 10" | bc)
    local total_time=$(echo "$end_time - $start_time" | bc)
    local total_mb=$(echo "$total_files * $FILE_SIZE_MB" | bc)
    local throughput=$(echo "scale=2; $total_mb / $total_time" | bc)
    local avg_latency=$(echo "scale=2; ($total_time * 1000) / $total_files" | bc)
    local files_per_sec=$(echo "scale=2; $total_files / $total_time" | bc)
    
    success "Concurrent Read: $throughput MB/s aggregate, ${avg_latency}ms avg latency"
    record_result "$fs_name" "concurrent_read" "$throughput" "$avg_latency" "$total_time" "$files_per_sec"
}

#############################################################################
# Main Execution
#############################################################################

main() {
    log "Starting FUSE Filesystem Benchmarks"
    log "Configuration:"
    log "  - File size: ${FILE_SIZE_MB}MB (YouTube Short)"
    log "  - Medium file tests: $NUM_MEDIUM_FILES files"
    log "  - Small file tests: $NUM_SMALL_FILES files"
    log "  - Concurrent threads: $NUM_THREADS"
    echo ""
    
    # Initialize results file
    init_csv
    
    # Prepare tmpfs test directory
    mkdir -p $TMPFS_MOUNT
    
    # Check if FUSE is mounted
    if ! mountpoint -q $FUSE_MOUNT; then
        warn "FUSE not mounted at $FUSE_MOUNT"
        warn "Please mount FUSE filesystem first"
        exit 1
    fi
    
    #########################################################################
    # Run tests on FUSE
    #########################################################################
    
    log "========================================="
    log "Testing FUSE Filesystem"
    log "========================================="
    
    test_sequential_write "$FUSE_MOUNT" "fuse"
    test_sequential_read "$FUSE_MOUNT" "fuse"
    test_file_creation "$FUSE_MOUNT" "fuse"
    test_mixed_workload "$FUSE_MOUNT" "fuse"
    test_concurrent_write "$FUSE_MOUNT" "fuse"
    test_concurrent_read "$FUSE_MOUNT" "fuse"
    
    #########################################################################
    # Run tests on tmpfs (baseline)
    #########################################################################
    
    log "========================================="
    log "Testing tmpfs (baseline)"
    log "========================================="
    
    test_sequential_write "$TMPFS_MOUNT" "tmpfs"
    test_sequential_read "$TMPFS_MOUNT" "tmpfs"
    test_file_creation "$TMPFS_MOUNT" "tmpfs"
    test_mixed_workload "$TMPFS_MOUNT" "tmpfs"
    test_concurrent_write "$TMPFS_MOUNT" "tmpfs"
    test_concurrent_read "$TMPFS_MOUNT" "tmpfs"
    
    #########################################################################
    # Summary
    #########################################################################
    
    echo ""
    success "Benchmarks complete!"
    log "Results saved to: $OUTPUT_CSV"
    log ""
    log "To generate graphs, run:"
    log "  python3 /app/benchmarks/plot_results.py"
}

# Run main
main