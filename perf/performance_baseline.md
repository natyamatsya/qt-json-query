> **Historical baseline (2026-02-08) — superseded.** Measurements predate the
> 2026-07-03 changes: thread_local ArrayPool (concurrent evaluation ~4.6x
> faster at 8 threads), JSONPath pimpl (+1 allocation in create()), and the
> removal of recursive-descent dedup/limits. Re-baseline before drawing
> conclusions from these numbers.

# Performance Baseline

**Date**: 2026-02-08
**Compiler**: Apple clang version 17.0.0 (clang-1700.6.3.2)
**Build**: Release (-O2 -DNDEBUG)
**Qt**: 6.8.3
**Architecture**: arm64 (Darwin)
**Logging**: Disabled (QT_LOGGING_RULES="\*=false")

## Benchmark Results

### JSONPointer

| Benchmark | Time | Iterations |
|---|---|---|
| Simple | 211 ns | 3,329,845 |
| Nested | 350 ns | 2,003,704 |
| Array | 474 ns | 1,483,469 |
| Complex | 626 ns | 1,123,415 |
| Creation | 346 ns | 2,028,351 |

### JSONPointer (eval only, pre-compiled)

| Benchmark | Time | Iterations |
|---|---|---|
| Simple | 99 ns | 6,998,880 |
| Nested | 165 ns | 4,214,938 |
| Array | 215 ns | 3,247,220 |
| Complex | 280 ns | 2,510,220 |

### Plain Qt JSON (manual traversal, reference baseline)

| Benchmark | Time | Iterations |
|---|---|---|
| Simple | 126 ns | 5,573,382 |
| Nested | 217 ns | 3,227,859 |
| Array | 259 ns | 2,702,932 |
| Filter | 11.0 µs | 63,691 |
| Recursive | 16.5 µs | 42,423 |

### JSONPath

| Benchmark | Time | Iterations |
|---|---|---|
| Simple | 681 ns | 1,026,182 |
| Nested | 931 ns | 791,801 |
| Array | 1.4 µs | 500,229 |
| Filter | 14.4 µs | 49,096 |
| Recursive | 3.7 µs | 191,528 |
| Creation | 1.4 µs | 492,594 |

### JSONPath (eval only, pre-compiled)

| Benchmark | Time | Iterations |
|---|---|---|
| Simple | 349 ns | 1,999,983 |
| Nested | 396 ns | 1,772,753 |
| Array | 456 ns | 1,541,762 |
| Filter | 11.8 µs | 62,067 |
| Recursive | 3.2 µs | 219,660 |

### JSONPath evaluateSingle (eval only, pre-compiled, no QJsonArray)

| Benchmark | Time | Iterations |
|---|---|---|
| Simple | 96 ns | 7,319,726 |
| Nested | 161 ns | 4,332,648 |
| Array | 212 ns | 3,302,572 |

## JSONPath vs Plain Qt Overhead (create + eval)

| Operation | Plain Qt | JSONPath | Overhead |
|---|---|---|---|
| Simple | 126 ns | 681 ns | 5.4x |
| Nested | 217 ns | 931 ns | 4.3x |
| Array | 259 ns | 1.4 µs | 5.3x |
| Filter | 11.0 µs | 14.4 µs | 1.3x |
| Recursive | 16.5 µs | 3.7 µs | 0.2x |

## JSONPath vs Plain Qt Overhead (eval only)

| Operation | Plain Qt | JSONPath Eval | Overhead |
|---|---|---|---|
| Simple | 126 ns | 349 ns | 2.8x |
| Nested | 217 ns | 396 ns | 1.8x |
| Array | 259 ns | 456 ns | 1.8x |
| Filter | 11.0 µs | 11.8 µs | 1.1x |
| Recursive | 16.5 µs | 3.2 µs | 0.2x |

## JSONPath vs Plain Qt Overhead (evaluateSingle)

| Operation | Plain Qt | evaluateSingle | Overhead |
|---|---|---|---|
| Simple | 126 ns | 96 ns | 0.8x |
| Nested | 217 ns | 161 ns | 0.7x |
| Array | 259 ns | 212 ns | 0.8x |

## Raw Data

Results are archived as JSON in `perf/results/` for historical comparison.
Run `perf/macos/bin/record_benchmarks.sh` to update this file.
