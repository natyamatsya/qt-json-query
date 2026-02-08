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
| Simple | 223 ns | 3,135,260 | +6.2% slower |
| Nested | 359 ns | 1,952,117 | +1.4% slower |
| Array | 497 ns | 1,410,474 | +3.8% slower |
| Complex | 634 ns | 1,105,199 | ~stable |
| Creation | 346 ns | 2,034,712 | **-1.1%** faster |

### JSONPointer (eval only, pre-compiled)

| Benchmark | Time | Iterations | vs Previous |
|---|---|---|---|
| Simple | 102 ns | 6,837,740 |  |
| Nested | 165 ns | 4,247,856 |  |
| Array | 216 ns | 3,253,998 |  |
| Complex | 279 ns | 2,498,545 |  |

### Plain Qt JSON (manual traversal, reference baseline)

| Benchmark | Time | Iterations | vs Previous |
|---|---|---|---|
| Simple | 134 ns | 5,230,360 | +7.2% slower |
| Nested | 225 ns | 3,114,184 | +4.2% slower |
| Array | 287 ns | 2,540,042 | +11.7% slower |
| Filter | 11.1 µs | 62,669 | +3.8% slower |
| Recursive | 16.6 µs | 42,464 | +1.4% slower |

### JSONPath

| Benchmark | Time | Iterations | vs Previous |
|---|---|---|---|
| Simple | 780 ns | 902,457 | ~stable |
| Nested | 978 ns | 700,729 | ~stable |
| Array | 3.1 µs | 228,960 | ~stable |
| Filter | 14.5 µs | 48,403 | ~stable |
| Recursive | 3.6 µs | 192,480 | ~stable |
| Creation | 1.4 µs | 505,601 | ~stable |

### JSONPath (eval only, pre-compiled)

| Benchmark | Time | Iterations | vs Previous |
|---|---|---|---|
| Simple | 452 ns | 1,546,616 |  |
| Nested | 501 ns | 1,392,869 |  |
| Array | 2.2 µs | 320,140 |  |
| Filter | 11.3 µs | 62,684 |  |
| Recursive | 3.2 µs | 219,765 |  |

## JSONPath vs Plain Qt Overhead (create + eval)

| Operation | Plain Qt | JSONPath | Overhead |
|---|---|---|---|
| Simple | 134 ns | 780 ns | 5.8x |
| Nested | 225 ns | 978 ns | 4.3x |
| Array | 287 ns | 3.1 µs | 10.8x |
| Filter | 11.1 µs | 14.5 µs | 1.3x |
| Recursive | 16.6 µs | 3.6 µs | 0.2x |

## JSONPath vs Plain Qt Overhead (eval only)

| Operation | Plain Qt | JSONPath Eval | Overhead |
|---|---|---|---|
| Simple | 134 ns | 452 ns | 3.4x |
| Nested | 225 ns | 501 ns | 2.2x |
| Array | 287 ns | 2.2 µs | 7.6x |
| Filter | 11.1 µs | 11.3 µs | 1.0x |
| Recursive | 16.6 µs | 3.2 µs | 0.2x |

## Previous Baseline

Compared against: `benchmark_2026-02-08_205956.json` (2026-02-08)

## Raw Data

Results are archived as JSON in `perf/results/` for historical comparison.
Run `perf/macos/bin/record_benchmarks.sh` to update this file.
