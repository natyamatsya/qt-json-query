#!/usr/bin/env bash
# record_benchmarks.sh — Run Google Benchmark and archive the results
#
# Usage:
#   ./perf/macos/bin/record_benchmarks.sh [build-dir]
#
# Defaults:
#   build-dir = build-release
#
# The script:
#   1. Builds json_benchmark in Release mode (if needed)
#   2. Runs the benchmark with the baseline methodology
#      (5 repetitions, logging disabled, JSON output)
#   3. Archives the raw JSON to perf/results/
#   4. Prints the median tables and overhead ratios in the
#      perf/performance_baseline.md format
#
# It deliberately does NOT rewrite performance_baseline.md: since the
# 2026-07-03 re-baseline that file is a curated dual-platform (Windows +
# macOS) document. Update its macOS columns from the printed tables and
# check the surrounding prose still holds.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="${1:-build-release}"
RESULTS_DIR="$PROJECT_ROOT/perf/results"
BENCHMARK_BIN="$PROJECT_ROOT/$BUILD_DIR/benchmarks/json_benchmark"
REPETITIONS=5

die() { echo "ERROR: $*" >&2; exit 1; }

# ── 1. Build ─────────────────────────────────────────────────────────────

if [[ ! -x "$BENCHMARK_BIN" ]]; then
    echo "Building json_benchmark in $BUILD_DIR …"
    if [[ ! -d "$PROJECT_ROOT/$BUILD_DIR" ]]; then
        cmake -S "$PROJECT_ROOT" -B "$PROJECT_ROOT/$BUILD_DIR" -G Ninja \
              -DCMAKE_BUILD_TYPE=Release -DJSON_QUERY_BUILD_BENCHMARKS=ON \
              -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH:-}" 2>&1 | tail -3
    fi
    cmake --build "$PROJECT_ROOT/$BUILD_DIR" --target json_benchmark --parallel \
        || die "Failed to build json_benchmark"
fi

# ── 2. Run benchmark ────────────────────────────────────────────────────

mkdir -p "$RESULTS_DIR"

DATE_HUMAN="$(date +%Y-%m-%d)"
JSON_OUT="$RESULTS_DIR/benchmark_${DATE_HUMAN}_macos-appleclang.json"
if [[ -e "$JSON_OUT" ]]; then
    JSON_OUT="$RESULTS_DIR/benchmark_$(date +%Y-%m-%d_%H%M%S)_macos-appleclang.json"
fi

echo "Running benchmarks ($REPETITIONS repetitions) …"
QT_LOGGING_RULES="*=false" "$BENCHMARK_BIN" \
    --benchmark_format=json \
    --benchmark_out="$JSON_OUT" \
    --benchmark_repetitions="$REPETITIONS" \
    > /dev/null

echo "Raw results saved to $JSON_OUT"
echo ""

# ── 3. Report medians in the performance_baseline.md format ────────────

python3 - "$JSON_OUT" << 'EOF'
import json, sys

with open(sys.argv[1]) as f:
    data = json.load(f)

medians = {b['run_name']: b['real_time']
           for b in data['benchmarks']
           if b.get('aggregate_name') == 'median'}


def fmt(v):
    if v >= 100_000:
        return f"{v/1000:.0f} µs"
    if v >= 1000:
        return f"{v/1000:.1f} µs"
    return f"{v:.0f} ns"


SECTIONS = [
    ("JSONPointer (create + eval)", "BM_JSONPointer_", [
        "Simple", "Nested", "Array", "Complex", "Creation"]),
    ("JSONPointer (eval only, pre-compiled)", "BM_JSONPointer_Eval_", [
        "Simple", "Nested", "Array", "Complex"]),
    ("Plain Qt JSON (manual traversal, reference baseline)", "BM_Plain_", [
        "Simple", "Nested", "Array", "Filter", "Recursive"]),
    ("JSONPath (create + eval)", "BM_JSONPath_", [
        "Simple", "Nested", "Array", "Filter", "Recursive", "Creation"]),
    ("JSONPath (eval only, pre-compiled)", "BM_JSONPath_Eval_", [
        "Simple", "Nested", "Array", "Filter", "Recursive"]),
    ("JSONPath evaluateSingle (eval only, pre-compiled, no QJsonArray)",
     "BM_JSONPath_EvalSingle_", ["Simple", "Nested", "Array"]),
]

missing = []
for title, prefix, names in SECTIONS:
    print(f"### {title}\n")
    print("| Benchmark | Time |")
    print("|---|---|")
    for name in names:
        key = prefix + name
        if key not in medians:
            missing.append(key)
            continue
        print(f"| {name} | {fmt(medians[key])} |")
    print()

OVERHEADS = [
    ("create + eval", "BM_JSONPath_{}",
     ["Simple", "Nested", "Array", "Filter", "Recursive"]),
    ("eval only", "BM_JSONPath_Eval_{}",
     ["Simple", "Nested", "Array", "Filter", "Recursive"]),
    ("evaluateSingle", "BM_JSONPath_EvalSingle_{}",
     ["Simple", "Nested", "Array"]),
]

for title, pattern, names in OVERHEADS:
    print(f"## JSONPath vs Plain Qt Overhead ({title})\n")
    print("| Operation | Plain Qt | JSONPath | Overhead |")
    print("|---|---|---|---|")
    for name in names:
        plain, path = medians.get(f"BM_Plain_{name}"), medians.get(pattern.format(name))
        if plain is None or path is None:
            continue
        print(f"| {name} | {fmt(plain)} | {fmt(path)} | {path/plain:.1f}x |")
    print()

# Repetition spread of the plain-Qt controls — a large spread means the
# machine was busy and the run should be repeated (see the interleaved-run
# note in perf/performance_baseline.md history).
print("Control spread across repetitions (re-run if any exceeds ~5%):")
for name in ["BM_Plain_Simple", "BM_Plain_Nested", "BM_Plain_Array",
             "BM_Plain_Filter", "BM_Plain_Recursive"]:
    reps = [b['real_time'] for b in data['benchmarks']
            if b['run_name'] == name and b.get('run_type') == 'iteration']
    if reps:
        spread = (max(reps) / min(reps) - 1) * 100
        print(f"  {name:24s} {spread:5.1f}%")

if missing:
    print(f"\nWARNING: benchmarks missing from the run: {', '.join(missing)}")
EOF

echo ""
echo "✓ Raw JSON archived at $JSON_OUT"
echo "→ Update the macOS columns in perf/performance_baseline.md from the"
echo "  tables above (and perf/PERFORMANCE_ROADMAP.md if ratios shifted)."
