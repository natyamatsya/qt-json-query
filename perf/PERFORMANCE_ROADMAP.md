# Performance Optimization Roadmap

**Created**: 2026-02-08
**Re-baselined**: 2026-07-03 (Windows/MSVC and macOS/AppleClang —
see `perf/performance_baseline.md`)

## Current State (2026-07-03, Release, 5-rep medians)

Overhead vs plain Qt manual traversal (same machine; absolute times in
`performance_baseline.md`):

| Benchmark | eval-only (Win) | eval-only (macOS) | create+eval (Win) | create+eval (macOS) |
|---|---|---|---|---|
| Simple (`$.name`) | 1.4x | 2.9x | 5.3x | 5.7x |
| Nested (`$.location.city`) | 0.8x | 1.9x | 3.2x | 4.2x |
| Array (`$.inventory[5].title`) | 1.0x | 1.8x | 5.5x | 5.4x |
| Filter (`$.inventory[?(@.price>20)].title`) | 0.8x | 1.2x | 1.4x | 1.5x |
| Recursive (`$..title`) | 11.5x | 20.0x | 13.5x | 20.0x |

`evaluateSingle` (no QJsonArray result): 0.3–0.4x of plain Qt on Windows,
≈1.0x on macOS (Simple/Nested/Array). The ratios differ because plain Qt
traversal is relatively much faster on macOS/arm64, not because the library
is slower there in absolute terms.

Note: the 2026-02-08 table was measured on macOS/arm64 (Apple clang 17);
ratios shifted with the intervening optimization work and toolchain updates,
so milestone targets below were re-assessed against the new numbers.

## Milestone 1: Benchmark Accuracy

**Status**: Done (2026-02)

Pre-compiled `BM_JSONPath_Eval_*` / `BM_JSONPointer_Eval_*` benchmarks isolate
parse cost from evaluation cost; `performance_baseline.md` reports both.

## Milestone 2: Definite Path Fast Path

**Status**: Re-assess — platform-dependent. On Windows, eval-only overhead is
≈1x (Simple 1.4x, Nested 0.8x, Array 1.0x): the original target ("approach
Plain Qt speed") is effectively met for evaluation. On macOS/arm64, eval-only
is still 1.8–2.9x — plain Qt traversal is proportionally faster there, so the
fixed per-call machinery (token dispatch, result-array handling) weighs more.
The create+eval gap (3–5.7x) is dominated by `create()` (~0.8–1.4 µs vs
~0.2–0.3 µs evaluation) on both platforms: any further work here should
target parse/compile cost or the macOS per-call overhead, not Windows
evaluation.

## Milestone 3: Recursive+Key Token Fusion — **top priority**

**Status**: Pending
**Target**: Recursive `$..title` eval-only — 246 µs vs 21.3 µs plain (11.5x)
on Windows; 195 µs vs 9.7 µs plain (20.0x) on macOS.

The 2026-02-08 baseline showed recursive descent *faster* than plain Qt
(0.2x); that figure was an artifact — the old depth/result caps and the
RFC-violating dedup (removed in the M1 hardening batch, see repo ROADMAP)
silently truncated the traversal being measured. The corrected full traversal
exposes the real cost: `$..title` emits every descendant, then filters by
key. Token fusion (Recursive + Key in one traversal collecting only matching
keys) is the standing approach; re-estimate the gain against the corrected
baseline before starting.

## Milestone 4: Single-Value Result Path

**Status**: Effectively delivered by `evaluateSingle` (0.3–0.4x of plain Qt
on Windows, ≈1.0x on macOS — i.e. at or below a hand-written traversal, no
intermediate QJsonArray). Remaining idea: route definite paths inside
`evaluate()` through the same machinery when the nodelist is provably
single-valued — only worth it if profiling shows the array wrapper matters
in practice.
