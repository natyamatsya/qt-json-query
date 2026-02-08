# Performance Baseline

**Date**: 2026-02-08
**Compiler**: Apple clang version 17.0.0 (clang-1700.6.3.2)
**Build**: Release (-O2 -DNDEBUG)
**Qt**: 6.8.3
**Architecture**: arm64 (Darwin)
**Logging**: Disabled (QT_LOGGING_RULES="\*=false")

## Benchmark Results

### JSONPointer

| Benchmark | Time | Iterations | vs Previous |
|---|---|---|---|
| Simple | 210 ns | 3,323,521 | **-9.1%** faster |
| Nested | 354 ns | 1,973,143 | **-3.0%** faster |
| Array | 479 ns | 1,414,948 | ~stable |
| Complex | 631 ns | 1,111,676 | ~stable |
| Creation | 350 ns | 2,003,136 | +1.7% slower |

### Plain Qt JSON (manual traversal, reference baseline)

| Benchmark | Time | Iterations | vs Previous |
|---|---|---|---|
| Simple | 125 ns | 5,589,358 | +1.6% slower |
| Nested | 216 ns | 3,252,199 | ~stable |
| Array | 257 ns | 2,718,616 | ~stable |
| Filter | 10.7 µs | 64,035 | **-3.3%** faster |
| Recursive | 16.4 µs | 43,575 | **-2.5%** faster |

### JSONPath

| Benchmark | Time | Iterations | vs Previous |
|---|---|---|---|
| Simple | 784 ns | 901,957 | **-58.8%** faster |
| Nested | 971 ns | 723,574 | **-62.7%** faster |
| Array | 3.1 µs | 229,416 | **-21.6%** faster |
| Filter | 14.4 µs | 48,757 | **-4.1%** faster |
| Recursive | 3.6 µs | 187,830 | **-95.5%** faster |
| Creation | 1.4 µs | 505,349 | **-4.5%** faster |

## JSONPath vs Plain Qt Overhead

| Operation | Plain Qt | JSONPath | Overhead |
|---|---|---|---|
| Simple | 125 ns | 784 ns | 6.3x |
| Nested | 216 ns | 971 ns | 4.5x |
| Array | 257 ns | 3.1 µs | 12.0x |
| Filter | 10.7 µs | 14.4 µs | 1.3x |
| Recursive | 16.4 µs | 3.6 µs | 0.2x |

## Previous Baseline

Compared against: `benchmark_2026-02-08_170726.json` (2026-02-08)

## Raw Data

Results are archived as JSON in `perf/results/` for historical comparison.
Run `perf/macos/bin/record_benchmarks.sh` to update this file.
