# Performance Baseline

**Date**: 2026-07-03 (both platforms)
**Method**: `json_benchmark --benchmark_repetitions=5`, medians reported.
**Logging**: Disabled (`QT_LOGGING_RULES="*=false"`)

|  | Windows | macOS |
|---|---|---|
| Compiler | MSVC 19.51.36248 (x64) | Apple clang 21.0.0 (arm64) |
| Build | Release (Ninja, /O2 /DNDEBUG) | Release (Ninja, -O2 -DNDEBUG) |
| Qt | 6.8.5 (msvc2022_64) | 6.8.3 (macos) |
| OS / CPU | Windows 11 Pro, 32 logical cores @ 4.2 GHz | macOS 27, Apple M2 Max (12 cores) |
| Raw data | `perf/results/benchmark_2026-07-03_win-msvc.json` | `perf/results/benchmark_2026-07-03_macos-appleclang.json` |

Supersedes the 2026-02-08 macOS baseline (Apple clang 17, arm64, Qt 6.8.3 —
raw data kept in `perf/results/benchmark_2026-02-08_220034.json`), which
predated the thread_local `ArrayPool`, the JSONPath pimpl, and the removal of
the recursive-descent caps/dedup. **Absolute times are not comparable across
the two platforms; compare the same-machine overhead ratios instead.**

## Benchmark Results

### JSONPointer (create + eval)

| Benchmark | Windows | macOS |
|---|---|---|
| Simple | 265 ns | 125 ns |
| Nested | 482 ns | 215 ns |
| Array | 615 ns | 295 ns |
| Complex | 944 ns | 380 ns |
| Creation | 341 ns | 186 ns |

### JSONPointer (eval only, pre-compiled)

| Benchmark | Windows | macOS |
|---|---|---|
| Simple | 93 ns | 69 ns |
| Nested | 166 ns | 115 ns |
| Array | 145 ns | 151 ns |
| Complex | 178 ns | 195 ns |

### Plain Qt JSON (manual traversal, reference baseline)

| Benchmark | Windows | macOS |
|---|---|---|
| Simple | 172 ns | 67 ns |
| Nested | 353 ns | 119 ns |
| Array | 300 ns | 147 ns |
| Filter | 13.7 µs | 6.4 µs |
| Recursive | 21.3 µs | 9.7 µs |

### JSONPath (create + eval)

| Benchmark | Windows | macOS |
|---|---|---|
| Simple | 918 ns | 378 ns |
| Nested | 1.12 µs | 503 ns |
| Array | 1.64 µs | 789 ns |
| Filter | 19.7 µs | 9.4 µs |
| Recursive | 287 µs | 195 µs |
| Creation | 1.41 µs | 806 ns |

### JSONPath (eval only, pre-compiled)

| Benchmark | Windows | macOS |
|---|---|---|
| Simple | 240 ns | 192 ns |
| Nested | 296 ns | 231 ns |
| Array | 296 ns | 271 ns |
| Filter | 11.6 µs | 7.5 µs |
| Recursive | 246 µs | 195 µs |

### JSONPath evaluateSingle (eval only, pre-compiled, no QJsonArray)

| Benchmark | Windows | macOS |
|---|---|---|
| Simple | 71 ns | 67 ns |
| Nested | 121 ns | 112 ns |
| Array | 119 ns | 148 ns |

## JSONPath vs Plain Qt Overhead (create + eval)

| Operation | Windows | macOS |
|---|---|---|
| Simple | 5.3x | 5.7x |
| Nested | 3.2x | 4.2x |
| Array | 5.5x | 5.4x |
| Filter | 1.4x | 1.5x |
| Recursive | 13.5x | 20.0x¹ |

## JSONPath vs Plain Qt Overhead (eval only)

| Operation | Windows | macOS |
|---|---|---|
| Simple | 1.4x | 2.9x |
| Nested | 0.8x | 1.9x |
| Array | 1.0x | 1.8x |
| Filter | 0.8x | 1.2x |
| Recursive | 11.5x | 20.0x¹ |

## JSONPath vs Plain Qt Overhead (evaluateSingle)

| Operation | Windows | macOS |
|---|---|---|
| Simple | 0.4x | 1.0x |
| Nested | 0.3x | 0.9x |
| Array | 0.4x | 1.0x |

**The overhead ratios are platform-dependent** because plain Qt traversal is
relatively much faster on macOS/arm64: eval-only overhead is ≈1x plain Qt on
Windows but 1.8–2.9x on macOS; `evaluateSingle` is 0.3–0.4x on Windows and
≈1.0x on macOS. Recursive descent is the outlier on both platforms.

¹ **The historical "recursive descent faster than plain Qt" figure
(2026-02-08: 0.2x) was an artifact of a bug.** The old `kMaxStackDepth{100}` /
`kMaxResults` caps and the RFC-violating result dedup (both removed in the
2026-07-03 M1 hardening batch, see ROADMAP) silently truncated `$..`
traversals on the 100-book benchmark document — the old benchmark measured an
incomplete traversal. The corrected full traversal costs 246 µs vs 21.3 µs
plain on Windows (11.5x) and 195 µs vs 9.7 µs on macOS (20.0x); recursive
descent is now the primary optimization target
(see `PERFORMANCE_ROADMAP.md`, Milestone 3).

## Allocation Counts (global operator new, Release)

Measured with `perf/src/allocation_probe.cpp` (target `allocation_probe`).
Qt containers allocate via `malloc` (QArrayData), so these counts isolate the
library's C++-side allocations (pimpl `unique_ptr`, `shared_ptr` control
blocks, internal vectors) from Qt container storage. Counts vary with the
standard-library implementation (vector growth, `shared_ptr` layout), hence
the per-platform columns.

| Call | Windows (MSVC STL) | macOS (libc++) |
|---|---|---|
| `JSONPointer::create` | 1 | 1 (the inline token vector's buffer — no pimpl since 0.5.0) |
| `JSONPath::create` (with or without filter) | 6 | 5 (pimpl + compiled state) |
| `JSONPath` copy construction | 2 | 2 (impl clone; filter tokens are shared_ptr-shared, not deep-cloned) |
| `JSONPath::evaluate` (small doc) | 9–15 | 18–27 |
| `JSONSchema::create` (small schema) | 33 | 13 (incl. shared_ptr control block) |

## Raw Data

Results are archived as JSON in `perf/results/` for historical comparison.
On macOS, run from a Release build (`-DBUILD_BENCHMARKING=ON`):
`QT_LOGGING_RULES="*=false" ./build-release/benchmarks/json_benchmark
--benchmark_repetitions=5 --benchmark_format=json --benchmark_out=…`.
On Windows, run the same command from a Release build
(`. .\Init-DevEnv.ps1`, preset `release-msvc`, `-DBUILD_BENCHMARKING=ON`).
