#!/bin/bash

SERVER="54.176.177.247:60051"
CLIENT="/app/bin/distributed_client"
ITERATIONS=1000
OUTDIR="microbench_results"

if [ -d "$OUTDIR" ]; then
    # If the directory exists, delete it and all its contents
    rm -rf "$OUTDIR"
fi
mkdir -p $OUTDIR

echo "Starting microbenchmarks..."

for ((i=1;i<=ITERATIONS;i++))
do
    echo "iteration $i"

    # mkdir
    start=$(date +%s%N)
    $CLIENT $SERVER mkdir / benchdir > /dev/null 2>&1
    end=$(date +%s%N)

    LATENCY=$((end - start))
    echo "$i,$LATENCY" >> "$OUTDIR/mkdir.csv"

    # create
    start=$(date +%s%N)
    $CLIENT $SERVER create /benchdir benchfile > /dev/null 2>&1
    end=$(date +%s%N)

    LATENCY=$((end - start))
    echo "$i,$LATENCY" >> "$OUTDIR/create.csv"

    # write
    start=$(date +%s%N)
    $CLIENT $SERVER write /benchdir/benchfile hello > /dev/null 2>&1
    end=$(date +%s%N)

    LATENCY=$((end - start))
    echo "$i,$LATENCY" >> "$OUTDIR/write.csv"

    # read
    start=$(date +%s%N)
    $CLIENT $SERVER read /benchdir/benchfile > /dev/null 2>&1
    end=$(date +%s%N)

    LATENCY=$((end - start))
    echo "$i,$LATENCY" >> "$OUTDIR/read.csv"

    # ls
    start=$(date +%s%N)
    $CLIENT $SERVER ls /benchdir > /dev/null 2>&1
    end=$(date +%s%N)

    LATENCY=$((end - start))
    echo "$i,$LATENCY" >> "$OUTDIR/ls.csv"

    # rm
    start=$(date +%s%N)
    $CLIENT $SERVER rm /benchdir/benchfile > /dev/null 2>&1
    end=$(date +%s%N)

    LATENCY=$((end - start))
    echo "$i,$LATENCY" >> "$OUTDIR/rm.csv"

    # rmdir
    start=$(date +%s%N)
    $CLIENT $SERVER rmdir /benchdir > /dev/null 2>&1
    end=$(date +%s%N)

    LATENCY=$((end - start))
    echo "$i,$LATENCY" >> "$OUTDIR/rmdir.csv"
done

# bench_op create "create /benchdir benchfile"


# bench_op write "write /benchdir/benchfile hello"
# bench_op read "read /benchdir/benchfile"
# bench_op ls "ls /benchdir"
# bench_op rm "rm /benchdir/benchfile"
# bench_op rmdir "rmdir /benchdir"

echo "All benchmarks complete."