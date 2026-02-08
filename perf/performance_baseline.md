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
| Simple | 231 ns | 3,219,990 | +8.5% slower |
| Nested | 365 ns | 1,931,120 | +3.1% slower |
| Array | 475 ns | 1,457,295 | **-1.0%** faster |
| Complex | 631 ns | 1,123,325 | ~stable |
| Creation | 344 ns | 2,084,208 | **-2.3%** faster |

### Plain Qt JSON (manual traversal, reference baseline)

| Benchmark | Time | Iterations | vs Previous |
|---|---|---|---|
| Simple | 123 ns | 5,650,356 | **-2.4%** faster |
| Nested | 214 ns | 3,308,738 | ~stable |
| Array | 258 ns | 2,680,750 | ~stable |
| Filter | 11.1 µs | 65,091 | ~stable |
| Recursive | 16.8 µs | 41,937 | ~stable |

### JSONPath

| Benchmark | Time | Iterations | vs Previous |
|---|---|---|---|
| Simple | 1.9 µs | 368,885 | ~stable |
| Nested | 2.6 µs | 263,461 | **-8.7%** faster |
| Array | 3.9 µs | 184,187 | +4.3% slower |
| Filter | 15.0 µs | 46,549 | ~stable |
| Recursive | 81.1 µs | 8,520 | ~stable |
| Creation | 1.5 µs | 483,903 | +3.3% slower |

## JSONPath vs Plain Qt Overhead

| Operation | Plain Qt | JSONPath | Overhead |
|---|---|---|---|
| Simple | 123 ns | 1.9 µs | 15.5x |
| Nested | 214 ns | 2.6 µs | 12.2x |
| Array | 258 ns | 3.9 µs | 15.2x |
| Filter | 11.1 µs | 15.0 µs | 1.4x |
| Recursive | 16.8 µs | 81.1 µs | 4.8x |

## Previous Baseline

Compared against: `benchmark_2026-02-08_170631.json` (2026-02-08)

## Raw Data

Results are archived as JSON in `perf/results/` for historical comparison.
Run `perf/macos/bin/record_benchmarks.sh` to update this file.
