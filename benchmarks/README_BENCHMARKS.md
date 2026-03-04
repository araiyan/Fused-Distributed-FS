# FUSE Filesystem Benchmarks

Microbenchmarks comparing FUSED filesystem performance against tmpfs baseline.

## Prerequisites

- FUSE filesystem mounted at `/mnt/fused`
- Python 3 with pandas and matplotlib:
```bash
  pip install pandas matplotlib
```

## Running Benchmarks

### 1. Mount FUSE filesystem
```bash
# In one terminal, start FUSE
./bin/fused /mnt/fused

# Or in Docker
docker-compose up -d
```

### 2. Run benchmark script
```bash
cd tests
chmod +x benchmark.sh
./benchmark.sh
```

This will run 6 tests on both filesystems:
- Sequential write (100 files × 20MB)
- Sequential read (100 files × 20MB)
- File creation (1000 files)
- Mixed workload (70% read, 30% write)
- Concurrent write (10 threads)
- Concurrent read (10 threads)

**Runtime:** ~5-10 minutes depending on system

### 3. Generate graphs
```bash
python3 plot_results.py
```

This creates 4 PNG files:
- `throughput_comparison.png` - MB/s comparison
- `latency_comparison.png` - Latency comparison
- `overhead_percentage.png` - FUSE overhead %
- `files_per_second.png` - Operation rates

## Output Files

- `benchmark_results.csv` - Raw data (import into Excel/Google Sheets)
- `*.png` - Graphs for presentation slides

## Customizing Tests

Edit `benchmark.sh` to adjust:
```bash
NUM_SMALL_FILES=1000      # File creation test
NUM_MEDIUM_FILES=100      # Read/write tests
FILE_SIZE_MB=20           # File size (YouTube Short)
NUM_THREADS=10            # Concurrent operations
```

## Expected Results

Typical FUSE overhead vs tmpfs:
- **Throughput:** 20-40% slower (FUSE userspace overhead)
- **Latency:** 20-40% higher per operation
- **Scalability:** Should scale similarly with threads

## Troubleshooting

**Error: "FUSE not mounted"**
```bash
# Check if mounted:
mountpoint /mnt/fused

# If not, mount it:
./bin/fused /mnt/fused
```

**Error: "Cannot clear cache"**
```bash
# Run as root to drop caches:
sudo ./benchmark.sh
```

**Python import errors:**
```bash
pip3 install pandas matplotlib numpy
```