# Performance Optimization Roadmap

**Created**: 2026-02-08
**Baseline**: See `perf/performance_baseline.md`

## Current State

| Benchmark | Plain Qt | JSONPath | Overhead |
|---|---|---|---|
| Simple (`$.name`) | 125 ns | 784 ns | 6.3x |
| Nested (`$.location.city`) | 216 ns | 971 ns | 4.5x |
| Array (`$.inventory[5].title`) | 257 ns | 3.1 µs | 12.0x |
| Filter (`$.inventory[?(@.price>20)].title`) | 10.7 µs | 14.4 µs | 1.3x |
| Recursive (`$..title`) | 16.4 µs | 3.6 µs | 0.2x |

## Milestone 1: Benchmark Accuracy

**Status**: In progress

Current benchmarks include `JSONPath::create()` inside the hot loop, conflating
parse time with evaluation time. Add pre-compiled evaluation benchmarks to
isolate the two costs. This establishes accurate baselines for milestones 2-4.

**Deliverables**:
- `BM_JSONPath_Eval_*` benchmarks (pre-compiled path, evaluate only)
- `BM_JSONPointer_Eval_*` benchmarks (pre-compiled pointer, evaluate only)
- Updated `performance_baseline.md` with new benchmarks

## Milestone 2: Definite Path Fast Path

**Status**: Pending
**Targets**: Simple (6.3x), Nested (4.5x), Array (12.0x)

For paths with no wildcards, filters, or recursion, bypass the entire
`fanOut` → `evaluateToken` → QJsonArray pipeline. Walk tokens sequentially,
returning a single `QJsonValue`. `isDefinitePath()` already exists.

**Expected gain**: 3-5x improvement on Simple/Nested/Array (approach Plain Qt speed)

## Milestone 3: Recursive+Key Token Fusion

**Status**: Pending
**Targets**: Recursive

Currently `$..title` emits ~800 descendants then filters by key. Token fusion
combines Recursive + Key into a single traversal that only collects matching
keys — the `evaluateByKey` approach wired at the token dispatch level.

**Expected gain**: 2-3x improvement on Recursive

## Milestone 4: Single-Value Result Path

**Status**: Pending
**Targets**: Simple, Nested

`fanOut` and `evaluateToken` always produce `QJsonArray` even for single
results. A dedicated single-value code path avoids intermediate array
allocation for definite tokens.

**Expected gain**: 1.5-2x improvement on Simple/Nested (stacks with M2)
