#!/bin/bash
# Benchmark matrix for KV Store
# Usage: ./run_benchmarks.sh [server_host] [server_port]

HOST=${1:-127.0.0.1}
PORT=${2:-7000}
BENCHMARK=./build/kv-benchmark
RESULTS_DIR=benchmark_results
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

mkdir -p $RESULTS_DIR

echo "=== KV Store Benchmark Suite ==="
echo "Target: $HOST:$PORT"
echo "Results: $RESULTS_DIR/"
echo ""

# Sweep client concurrency
for THREADS in 1 2 4 8 16 32 64; do
    echo "--- Threads: $THREADS ---"
    $BENCHMARK --host $HOST --port $PORT --threads $THREADS --ops 10000 \
        2>&1 | tee -a $RESULTS_DIR/concurrency_sweep_$TIMESTAMP.txt
    echo ""
done

# Sweep value sizes
for VSIZE in 16 64 256 1024 4096; do
    echo "--- Value size: $VSIZE ---"
    $BENCHMARK --host $HOST --port $PORT --threads 8 --ops 5000 --value-size $VSIZE \
        2>&1 | tee -a $RESULTS_DIR/value_size_sweep_$TIMESTAMP.txt
    echo ""
done

# Write-heavy vs read-heavy
for RATIO in 0.0 0.25 0.5 0.75 1.0; do
    echo "--- Write ratio: $RATIO ---"
    $BENCHMARK --host $HOST --port $PORT --threads 8 --ops 5000 --ratio $RATIO \
        2>&1 | tee -a $RESULTS_DIR/ratio_sweep_$TIMESTAMP.txt
    echo ""
done

echo "=== Benchmark Complete ==="
