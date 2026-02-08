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
| Simple | 215 ns | 3,253,045 | ~stable |
| Nested | 362 ns | 1,933,899 | +3.7% slower |
| Array | 490 ns | 1,427,540 | +4.0% slower |
| Complex | 650 ns | 1,076,608 | +3.2% slower |
| Creation | 348 ns | 2,018,018 | ~stable |

### JSONPointer (eval only, pre-compiled)

| Benchmark | Time | Iterations | vs Previous |
|---|---|---|---|
| Simple | 100 ns | 7,016,348 | +1.0% slower |
| Nested | 166 ns | 4,234,084 | +1.2% slower |
| Array | 216 ns | 3,269,378 | ~stable |
| Complex | 279 ns | 2,512,563 | ~stable |

### Plain Qt JSON (manual traversal, reference baseline)

| Benchmark | Time | Iterations | vs Previous |
|---|---|---|---|
| Simple | 133 ns | 5,278,477 | +6.4% slower |
| Nested | 225 ns | 3,131,921 | +5.1% slower |
| Array | 276 ns | 2,546,844 | +7.0% slower |
| Filter | 11.4 µs | 61,886 | +4.2% slower |
| Recursive | 17.5 µs | 40,101 | +5.1% slower |

### JSONPath

| Benchmark | Time | Iterations | vs Previous |
|---|---|---|---|
| Simple | 771 ns | 916,170 | ~stable |
| Nested | 986 ns | 716,927 | +3.4% slower |
| Array | 1.5 µs | 475,750 | **-52.2%** faster |
| Filter | 14.4 µs | 48,473 | ~stable |
| Recursive | 3.7 µs | 192,096 | ~stable |
| Creation | 1.5 µs | 493,970 | +7.8% slower |

### JSONPath (eval only, pre-compiled)

| Benchmark | Time | Iterations | vs Previous |
|---|---|---|---|
| Simple | 427 ns | 1,651,290 | ~stable |
| Nested | 466 ns | 1,488,782 | **-2.1%** faster |
| Array | 539 ns | 1,332,090 | **-75.2%** faster |
| Filter | 11.3 µs | 61,648 | ~stable |
| Recursive | 3.2 µs | 219,983 | ~stable |

## JSONPath vs Plain Qt Overhead (create + eval)

| Operation | Plain Qt | JSONPath | Overhead |
|---|---|---|---|
| Simple | 133 ns | 771 ns | 5.8x |
| Nested | 225 ns | 986 ns | 4.4x |
| Array | 276 ns | 1.5 µs | 5.3x |
| Filter | 11.4 µs | 14.4 µs | 1.3x |
| Recursive | 17.5 µs | 3.7 µs | 0.2x |

## JSONPath vs Plain Qt Overhead (eval only)

| Operation | Plain Qt | JSONPath Eval | Overhead |
|---|---|---|---|
| Simple | 133 ns | 427 ns | 3.2x |
| Nested | 225 ns | 466 ns | 2.1x |
| Array | 276 ns | 539 ns | 2.0x |
| Filter | 11.4 µs | 11.3 µs | 1.0x |
| Recursive | 17.5 µs | 3.2 µs | 0.2x |

## Previous Baseline

Compared against: `benchmark_2026-02-08_211546.json` (2026-02-08)

## Raw Data

Results are archived as JSON in `perf/results/` for historical comparison.
Run `perf/macos/bin/record_benchmarks.sh` to update this file.
