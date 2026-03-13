#!/bin/bash

SERVER="54.176.177.247:60051"
CLIENT="/app/bin/distributed_client"
ITERATIONS=1000
OUTDIR="microbench_results"

mkdir -p $OUTDIR

bench_op() {
    NAME=$1
    CMD=$2
    OUTFILE="$OUTDIR/${NAME}.csv"

    echo "iteration,latency_ns" > $OUTFILE

    for ((i=1;i<=ITERATIONS;i++))
    do
        start=$(date +%s%N)

        $CLIENT $SERVER $CMD > /dev/null 2>&1

        end=$(date +%s%N)

        LATENCY=$((end - start))

        echo "$i,$LATENCY" >> $OUTFILE
    done

    echo "Finished $NAME"
}

echo "Starting microbenchmarks..."

bench_op mkdir "mkdir / benchdir"
bench_op create "create /benchdir benchfile"
bench_op write "write /benchdir/benchfile hello"
bench_op read "read /benchdir/benchfile"
bench_op ls "ls /benchdir"
bench_op rm "rm /benchdir/benchfile"
bench_op rmdir "rmdir /benchdir"

echo "All benchmarks complete."