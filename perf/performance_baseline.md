# Performance Baseline

**Date**: 2026-07-03
**Compiler**: MSVC 19.51.36248 (x64)
**Build**: Release (Ninja, /O2 /DNDEBUG)
**Qt**: 6.8.5 (msvc2022_64)
**OS / CPU**: Windows 11 Pro, 32 logical cores @ 4.2 GHz
**Logging**: Disabled (`QT_LOGGING_RULES="*=false"`)
**Method**: `json_benchmark --benchmark_repetitions=5`, medians reported.
Raw data: `perf/results/benchmark_2026-07-03_win-msvc.json`.

Supersedes the 2026-02-08 macOS baseline (Apple clang 17, arm64, Qt 6.8.3 —
raw data kept in `perf/results/benchmark_2026-02-08_220034.json`), which
predated the thread_local `ArrayPool`, the JSONPath pimpl, and the removal of
the recursive-descent caps/dedup. **Absolute times are not comparable across
the two platforms; compare the same-machine overhead ratios instead.**

## Benchmark Results

### JSONPointer (create + eval)

| Benchmark | Time |
|---|---|
| Simple | 265 ns |
| Nested | 482 ns |
| Array | 615 ns |
| Complex | 944 ns |
| Creation | 341 ns |

### JSONPointer (eval only, pre-compiled)

| Benchmark | Time |
|---|---|
| Simple | 93 ns |
| Nested | 166 ns |
| Array | 145 ns |
| Complex | 178 ns |

### Plain Qt JSON (manual traversal, reference baseline)

| Benchmark | Time |
|---|---|
| Simple | 172 ns |
| Nested | 353 ns |
| Array | 300 ns |
| Filter | 13.7 µs |
| Recursive | 21.3 µs |

### JSONPath (create + eval)

| Benchmark | Time |
|---|---|
| Simple | 918 ns |
| Nested | 1.12 µs |
| Array | 1.64 µs |
| Filter | 19.7 µs |
| Recursive | 287 µs |
| Creation | 1.41 µs |

### JSONPath (eval only, pre-compiled)

| Benchmark | Time |
|---|---|
| Simple | 240 ns |
| Nested | 296 ns |
| Array | 296 ns |
| Filter | 11.6 µs |
| Recursive | 246 µs |

### JSONPath evaluateSingle (eval only, pre-compiled, no QJsonArray)

| Benchmark | Time |
|---|---|
| Simple | 71 ns |
| Nested | 121 ns |
| Array | 119 ns |

## JSONPath vs Plain Qt Overhead (create + eval)

| Operation | Plain Qt | JSONPath | Overhead | (2026-02-08 macOS) |
|---|---|---|---|---|
| Simple | 172 ns | 918 ns | 5.3x | (5.4x) |
| Nested | 353 ns | 1.12 µs | 3.2x | (4.3x) |
| Array | 300 ns | 1.64 µs | 5.5x | (5.3x) |
| Filter | 13.7 µs | 19.7 µs | 1.4x | (1.3x) |
| Recursive | 21.3 µs | 287 µs | 13.5x | (0.2x¹) |

## JSONPath vs Plain Qt Overhead (eval only)

| Operation | Plain Qt | JSONPath Eval | Overhead | (2026-02-08 macOS) |
|---|---|---|---|---|
| Simple | 172 ns | 240 ns | 1.4x | (2.8x) |
| Nested | 353 ns | 296 ns | 0.8x | (1.8x) |
| Array | 300 ns | 296 ns | 1.0x | (1.8x) |
| Filter | 13.7 µs | 11.6 µs | 0.8x | (1.1x) |
| Recursive | 21.3 µs | 246 µs | 11.5x | (0.2x¹) |

## JSONPath vs Plain Qt Overhead (evaluateSingle)

| Operation | Plain Qt | evaluateSingle | Overhead | (2026-02-08 macOS) |
|---|---|---|---|---|
| Simple | 172 ns | 71 ns | 0.4x | (0.8x) |
| Nested | 353 ns | 121 ns | 0.3x | (0.7x) |
| Array | 300 ns | 119 ns | 0.4x | (0.8x) |

¹ **The historical "recursive descent faster than plain Qt" figure was an
artifact of a bug.** The old `kMaxStackDepth{100}` / `kMaxResults` caps and
the RFC-violating result dedup (both removed in the 2026-07-03 M1 hardening
batch, see ROADMAP) silently truncated `$..` traversals on the 100-book
benchmark document — the old benchmark measured an incomplete traversal.
The corrected full traversal costs 246 µs vs 21.3 µs plain (11.5x); recursive
descent is now the primary optimization target
(see `PERFORMANCE_ROADMAP.md`, Milestone 3).

## Allocation Counts (global operator new, Release)

Measured with `perf/src/allocation_probe.cpp` (target `allocation_probe`).
Qt containers allocate via `malloc` (QArrayData), so these counts isolate the
library's C++-side allocations (pimpl `unique_ptr`, `shared_ptr` control
blocks, internal vectors) from Qt container storage.

| Call | operator new calls |
|---|---|
| `JSONPointer::create` | 1 (exactly the pimpl) |
| `JSONPath::create` (with or without filter) | 6 (pimpl + compiled state) |
| `JSONPath` copy construction | 2 (impl clone; filter tokens are shared_ptr-shared, not deep-cloned) |
| `JSONPath::evaluate` (small doc) | 9–15 |
| `JSONSchema::create` (small schema) | 33 (incl. shared_ptr control block) |

## Raw Data

Results are archived as JSON in `perf/results/` for historical comparison.
On macOS, run `perf/macos/bin/record_benchmarks.sh`; on Windows, run
`json_benchmark --benchmark_repetitions=5 --benchmark_format=json` from a
Release build (`. .\Init-DevEnv.ps1`, preset `release-msvc`,
`-DBUILD_BENCHMARKING=ON`).

**Note:** a fresh macOS (arm64) run of this baseline is still pending; until
then, cross-platform ratio comparisons above use the superseded 2026-02-08
macOS numbers as the only available reference.
