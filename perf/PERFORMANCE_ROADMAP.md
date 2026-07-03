# Performance Optimization Roadmap

**Created**: 2026-02-08
**Re-baselined**: 2026-07-03 (Windows/MSVC — see `perf/performance_baseline.md`)

## Current State (2026-07-03, MSVC Release, medians)

| Benchmark | Plain Qt | JSONPath eval-only | Overhead | create + eval | Overhead |
|---|---|---|---|---|---|
| Simple (`$.name`) | 172 ns | 240 ns | 1.4x | 918 ns | 5.3x |
| Nested (`$.location.city`) | 353 ns | 296 ns | 0.8x | 1.12 µs | 3.2x |
| Array (`$.inventory[5].title`) | 300 ns | 296 ns | 1.0x | 1.64 µs | 5.5x |
| Filter (`$.inventory[?(@.price>20)].title`) | 13.7 µs | 11.6 µs | 0.8x | 19.7 µs | 1.4x |
| Recursive (`$..title`) | 21.3 µs | 246 µs | 11.5x | 287 µs | 13.5x |

`evaluateSingle` (no QJsonArray result): 0.3–0.4x of plain Qt on
Simple/Nested/Array — faster than a hand-written traversal.

Note: the 2026-02-08 table was measured on macOS/arm64; ratios shifted with
the platform change and the intervening optimization work, so milestone
targets below were re-assessed against the new numbers.

## Milestone 1: Benchmark Accuracy

**Status**: Done (2026-02)

Pre-compiled `BM_JSONPath_Eval_*` / `BM_JSONPointer_Eval_*` benchmarks isolate
parse cost from evaluation cost; `performance_baseline.md` reports both.

## Milestone 2: Definite Path Fast Path

**Status**: Re-assess — eval-only overhead is now ≈1x (Simple 1.4x,
Nested 0.8x, Array 1.0x), so the original target ("approach Plain Qt speed")
is effectively met for evaluation. What remains of the create+eval gap
(3–5.5x) is dominated by `create()` (~1.4 µs vs ~0.3 µs evaluation): any
further work here should target parse/compile cost, not evaluation.

## Milestone 3: Recursive+Key Token Fusion — **top priority**

**Status**: Pending
**Target**: Recursive `$..title` — 246 µs eval-only vs 21.3 µs plain Qt
(11.5x)

The 2026-02-08 baseline showed recursive descent *faster* than plain Qt
(0.2x); that figure was an artifact — the old depth/result caps and the
RFC-violating dedup (removed in the M1 hardening batch, see repo ROADMAP)
silently truncated the traversal being measured. The corrected full traversal
exposes the real cost: `$..title` emits every descendant, then filters by
key. Token fusion (Recursive + Key in one traversal collecting only matching
keys) is the standing approach; re-estimate the gain against the corrected
baseline before starting.

## Milestone 4: Single-Value Result Path

**Status**: Effectively delivered by `evaluateSingle` (0.3–0.4x of plain Qt,
no intermediate QJsonArray). Remaining idea: route definite paths inside
`evaluate()` through the same machinery when the nodelist is provably
single-valued — only worth it if profiling shows the array wrapper matters
in practice.
