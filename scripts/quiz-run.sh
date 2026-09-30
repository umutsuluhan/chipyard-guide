#!/usr/bin/env bash
set -euo pipefail

WS="${WS:-/workspace}"

SIM="$WS/chipyard/sims/verilator/simulator-chipyard.harness-BaselineConfig"
BENCH="$WS/output/isp_bench.riscv"
SRC="$WS/course/benchmarks"

# Build benchmark if needed
if [[ ! -f "$BENCH" ]]; then
    echo "Building ISP benchmark..."
    make -C "$SRC"
fi

echo "Running ISP benchmark on BaselineConfig..."
echo

exec "$SIM" "$BENCH"