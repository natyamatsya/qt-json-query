#!/usr/bin/env bash
# record_benchmarks.sh — Run Google Benchmark and update performance_baseline.md
#
# Usage:
#   ./perf/macos/bin/record_benchmarks.sh [build-dir]
#
# Defaults:
#   build-dir = build-release
#
# The script:
#   1. Builds json_benchmark in Release mode (if needed)
#   2. Runs the benchmark with JSON output
#   3. Archives the raw JSON to perf/results/
#   4. Regenerates perf/performance_baseline.md with current + previous comparison

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
BUILD_DIR="${1:-build-release}"
RESULTS_DIR="$PROJECT_ROOT/perf/results"
BASELINE_MD="$PROJECT_ROOT/perf/performance_baseline.md"
BENCHMARK_BIN="$PROJECT_ROOT/$BUILD_DIR/benchmarks/json_benchmark"

# ── Helpers ──────────────────────────────────────────────────────────────

die() { echo "ERROR: $*" >&2; exit 1; }

# Extract a benchmark's real_time from the JSON output (in ns).
# $1 = JSON file, $2 = benchmark name
bench_ns() {
    python3 -c "
import json, sys
with open('$1') as f:
    data = json.load(f)
for b in data['benchmarks']:
    if b['name'] == '$2':
        print(int(b['real_time']))
        sys.exit(0)
print('—')
"
}

# Format nanoseconds as a human-readable string (ns, µs, or ms).
fmt_ns() {
    python3 -c "
v = $1
if v >= 1_000_000:
    print(f'{v/1_000_000:.1f} ms')
elif v >= 1_000:
    print(f'{v/1_000:.1f} µs')
else:
    print(f'{v} ns')
"
}

# ── 1. Build ─────────────────────────────────────────────────────────────

if [[ ! -x "$BENCHMARK_BIN" ]]; then
    echo "Building json_benchmark in $BUILD_DIR …"
    if [[ ! -d "$PROJECT_ROOT/$BUILD_DIR" ]]; then
        cmake -S "$PROJECT_ROOT" -B "$PROJECT_ROOT/$BUILD_DIR" -G Ninja \
              -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKING=ON \
              -DCMAKE_PREFIX_PATH="${CMAKE_PREFIX_PATH:-}" 2>&1 | tail -3
    fi
    cmake --build "$PROJECT_ROOT/$BUILD_DIR" --target json_benchmark --parallel \
        || die "Failed to build json_benchmark"
fi

# ── 2. Run benchmark ────────────────────────────────────────────────────

mkdir -p "$RESULTS_DIR"

TIMESTAMP="$(date +%Y-%m-%d_%H%M%S)"
DATE_HUMAN="$(date +%Y-%m-%d)"
JSON_OUT="$RESULTS_DIR/benchmark_${TIMESTAMP}.json"

echo "Running benchmarks …"
QT_LOGGING_RULES="*=false" "$BENCHMARK_BIN" \
    --benchmark_format=json \
    --benchmark_out="$JSON_OUT" \
    --benchmark_repetitions=1 \
    2>&1 | grep -v '^$'

echo "Raw results saved to $JSON_OUT"

# ── 3. Detect compiler & system info ────────────────────────────────────

COMPILER=$("${CXX:-c++}" --version 2>&1 | head -1)
QT_VERSION=$(python3 -c "
import re, pathlib
cml = pathlib.Path('$PROJECT_ROOT/$BUILD_DIR/CMakeCache.txt')
if cml.exists():
    txt = cml.read_text()
    # Try Qt6_VERSION:STRING first
    m = re.search(r'Qt6_VERSION:STRING=(.+)', txt)
    if not m:
        # Extract from Qt6_DIR path (e.g., /path/to/qt-6.8.3/6.8.3/macos/...)
        m = re.search(r'Qt6_DIR:PATH=.*/(\d+\.\d+\.\d+)/', txt)
    print(m.group(1) if m else 'unknown')
else:
    print('unknown')
")
ARCH="$(uname -m)"
OS="$(uname -s)"

# ── 4. Find previous baseline for comparison ────────────────────────────

PREV_JSON=""
for f in $(ls -t "$RESULTS_DIR"/benchmark_*.json 2>/dev/null); do
    if [[ "$f" != "$JSON_OUT" ]]; then
        PREV_JSON="$f"
        break
    fi
done

# ── 5. Generate performance_baseline.md ─────────────────────────────────

BENCHMARKS=(
    "BM_JSONPointer_Simple"
    "BM_JSONPointer_Nested"
    "BM_JSONPointer_Array"
    "BM_JSONPointer_Complex"
    "BM_JSONPointer_Creation"
    "BM_Plain_Simple"
    "BM_Plain_Nested"
    "BM_Plain_Array"
    "BM_Plain_Filter"
    "BM_Plain_Recursive"
    "BM_JSONPath_Simple"
    "BM_JSONPath_Nested"
    "BM_JSONPath_Array"
    "BM_JSONPath_Filter"
    "BM_JSONPath_Recursive"
    "BM_JSONPath_Creation"
)

echo "Generating $BASELINE_MD …"

cat > "$BASELINE_MD" << HEADER
# Performance Baseline

**Date**: $DATE_HUMAN
**Compiler**: $COMPILER
**Build**: Release (-O2 -DNDEBUG)
**Qt**: $QT_VERSION
**Architecture**: $ARCH ($OS)
**Logging**: Disabled (QT_LOGGING_RULES="\*=false")

## Benchmark Results

### JSONPointer

| Benchmark | Time | Iterations |$(if [[ -n "$PREV_JSON" ]]; then echo " vs Previous |"; fi)
|---|---|---|$(if [[ -n "$PREV_JSON" ]]; then echo "---|"; fi)
HEADER

# Helper to write a section of benchmarks
write_section() {
    local prefix="$1"
    shift
    for name in "$@"; do
        local ns
        ns=$(bench_ns "$JSON_OUT" "$name")
        if [[ "$ns" == "—" ]]; then continue; fi
        local time_str
        time_str=$(fmt_ns "$ns")
        local iters
        iters=$(python3 -c "
import json
with open('$JSON_OUT') as f:
    data = json.load(f)
for b in data['benchmarks']:
    if b['name'] == '$name':
        print(f\"{b['iterations']:,}\")
        break
")
        local short_name="${name#BM_${prefix}_}"

        if [[ -n "$PREV_JSON" ]]; then
            local prev_ns
            prev_ns=$(bench_ns "$PREV_JSON" "$name")
            local delta=""
            if [[ "$prev_ns" != "—" && "$prev_ns" -gt 0 ]]; then
                delta=$(python3 -c "
cur = $ns
prev = $prev_ns
pct = ((cur - prev) / prev) * 100
if abs(pct) < 1:
    print('~stable')
elif pct < 0:
    print(f'**{pct:+.1f}%** faster')
else:
    print(f'{pct:+.1f}% slower')
")
            fi
            echo "| $short_name | $time_str | $iters | $delta |" >> "$BASELINE_MD"
        else
            echo "| $short_name | $time_str | $iters |" >> "$BASELINE_MD"
        fi
    done
}

write_section "JSONPointer" \
    "BM_JSONPointer_Simple" \
    "BM_JSONPointer_Nested" \
    "BM_JSONPointer_Array" \
    "BM_JSONPointer_Complex" \
    "BM_JSONPointer_Creation"

cat >> "$BASELINE_MD" << 'PLAIN_HEADER'

### Plain Qt JSON (manual traversal, reference baseline)

PLAIN_HEADER

echo "| Benchmark | Time | Iterations |$(if [[ -n "$PREV_JSON" ]]; then echo " vs Previous |"; fi)" >> "$BASELINE_MD"
echo "|---|---|---|$(if [[ -n "$PREV_JSON" ]]; then echo "---|"; fi)" >> "$BASELINE_MD"

write_section "Plain" \
    "BM_Plain_Simple" \
    "BM_Plain_Nested" \
    "BM_Plain_Array" \
    "BM_Plain_Filter" \
    "BM_Plain_Recursive"

cat >> "$BASELINE_MD" << 'PATH_HEADER'

### JSONPath

PATH_HEADER

echo "| Benchmark | Time | Iterations |$(if [[ -n "$PREV_JSON" ]]; then echo " vs Previous |"; fi)" >> "$BASELINE_MD"
echo "|---|---|---|$(if [[ -n "$PREV_JSON" ]]; then echo "---|"; fi)" >> "$BASELINE_MD"

write_section "JSONPath" \
    "BM_JSONPath_Simple" \
    "BM_JSONPath_Nested" \
    "BM_JSONPath_Array" \
    "BM_JSONPath_Filter" \
    "BM_JSONPath_Recursive" \
    "BM_JSONPath_Creation"

# ── 6. Overhead summary ─────────────────────────────────────────────────

cat >> "$BASELINE_MD" << 'OVERHEAD_HEADER'

## JSONPath vs Plain Qt Overhead

OVERHEAD_HEADER

echo "| Operation | Plain Qt | JSONPath | Overhead |" >> "$BASELINE_MD"
echo "|---|---|---|---|" >> "$BASELINE_MD"

OVERHEAD_PAIRS=(
    "Simple:BM_Plain_Simple:BM_JSONPath_Simple"
    "Nested:BM_Plain_Nested:BM_JSONPath_Nested"
    "Array:BM_Plain_Array:BM_JSONPath_Array"
    "Filter:BM_Plain_Filter:BM_JSONPath_Filter"
    "Recursive:BM_Plain_Recursive:BM_JSONPath_Recursive"
)

for pair in "${OVERHEAD_PAIRS[@]}"; do
    IFS=: read -r label plain_name path_name <<< "$pair"
    local_plain=$(bench_ns "$JSON_OUT" "$plain_name")
    local_path=$(bench_ns "$JSON_OUT" "$path_name")
    if [[ "$local_plain" == "—" || "$local_path" == "—" ]]; then continue; fi
    overhead=$(python3 -c "print(f'{$local_path / $local_plain:.1f}x')")
    plain_str=$(fmt_ns "$local_plain")
    path_str=$(fmt_ns "$local_path")
    echo "| $label | $plain_str | $path_str | ${overhead} |" >> "$BASELINE_MD"
done

# ── 7. Historical note ──────────────────────────────────────────────────

if [[ -n "$PREV_JSON" ]]; then
    PREV_DATE=$(basename "$PREV_JSON" | sed 's/benchmark_\(.*\)_.*/\1/')
    cat >> "$BASELINE_MD" << EOF

## Previous Baseline

Compared against: \`$(basename "$PREV_JSON")\` ($PREV_DATE)
EOF
fi

cat >> "$BASELINE_MD" << EOF

## Raw Data

Results are archived as JSON in \`perf/results/\` for historical comparison.
Run \`perf/macos/bin/record_benchmarks.sh\` to update this file.
EOF

echo ""
echo "✓ Updated $BASELINE_MD"
echo "✓ Raw JSON archived at $JSON_OUT"
