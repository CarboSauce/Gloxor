#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [ $# -lt 1 ]; then
    echo "Usage: $0 <build_directory> [benchmark_binary_name]"
    echo "Example: $0 ../../build/RelWithDebInfo/test bench_main"
    exit 1
fi

BUILD_DIR="$1"
BENCH_NAME="${2:-benchmarks}"
BENCH_BIN="${BUILD_DIR}/${BENCH_NAME}"
RESULTS_DIR="${BUILD_DIR}/benchmark_results"
PLOT_SCRIPT="${SCRIPT_DIR}/bench_plot.py"
OUTPUT_IMAGE="${BUILD_DIR}/allocator_comparison.png"

if [ ! -d "${BUILD_DIR}" ]; then
    echo "Error: Build directory '${BUILD_DIR}' does not exist."
    exit 1
fi

if [ ! -x "${BENCH_BIN}" ]; then
    echo "Error: Benchmark binary '${BENCH_BIN}' not found or not executable."
    exit 1
fi

mkdir -p "${RESULTS_DIR}"

echo "=========================================="
echo " Running Benchmarks from: ${BENCH_BIN}"
echo " Storing JSONs in:        ${RESULTS_DIR}"
echo "=========================================="

BENCH_GROUPS=("Glox" "Mimalloc")

for GROUP in "${BENCH_GROUPS[@]}"; do
    JSON_FILE="${RESULTS_DIR}/${GROUP}.json"
    echo "[+] Running benchmark group: '${GROUP}' -> ${JSON_FILE}"

    "${BENCH_BIN}" \
        --benchmark_filter="${GROUP}.*" \
        --benchmark_out_format=json \
        --benchmark_out="${JSON_FILE}"
done

echo "[+] Benchmarks finished."

if [ -f "${PLOT_SCRIPT}" ]; then
    echo "[+] Generating comparison plot..."
    python3 "${PLOT_SCRIPT}" "${RESULTS_DIR}" -o "${OUTPUT_IMAGE}"
else
    echo "Warning: Graphing script '${PLOT_SCRIPT}' not found."
    echo "Skipping graph generation."
fi
