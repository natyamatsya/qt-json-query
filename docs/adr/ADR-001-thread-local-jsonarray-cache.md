# ADR-001: Thread-Local QJsonArray Cache for Definite Path Evaluation

**Status**: Accepted
**Date**: 2026-02-08
**Context**: JSONPath performance optimization (Milestone 2 follow-up)

## Problem

`JSONPath::evaluate()` returns `std::expected<QJsonArray, Error>`. For definite
paths (only key/index selectors), the result is always 0 or 1 elements. Every
call constructs a new `QJsonArray{result}`, which in Qt 6 allocates a CBOR
backing store on the heap — costing ~320 ns per call.

This overhead dominates definite path evaluation, where the actual traversal
(key lookups, index access) takes only ~100-200 ns. The `evaluateSingle()` API
avoids this by returning `QJsonValue` directly, but `evaluate()` must return
`QJsonArray` per its contract.

## Decision

Use a **thread-local pre-allocated single-element QJsonArray** to avoid
repeated heap allocation on the definite path hot path:

```cpp
static QJsonArray& reusableSingleElementArray()
{
    thread_local auto arr{QJsonArray{QJsonValue{}}};
    return arr;
}
```

On each `evaluate()` call for a definite path, we replace the element in-place
(`arr[0] = result`) and return the array. The return triggers Qt's implicit
sharing (COW) — the caller gets a shallow copy (refcount increment), and the
thread-local retains the original allocation for the next call.

## Consequences

### Performance

| Benchmark | Before | After | Improvement |
|---|---|---|---|
| Simple (`$.name`) | 425 ns | 347 ns | -18% |
| Nested (`$.location.city`) | 478 ns | 406 ns | -15% |
| Array (`$.inventory[5].title`) | 537 ns | 453 ns | -16% |

Saves ~75-85 ns per call by avoiding the QJsonArray heap allocation.

### Worst case

If the caller holds the returned `QJsonArray` across the next `evaluate()`
call, the thread-local's internal data has refcount > 1. The next `arr[0] = x`
triggers a COW detach — allocating a new backing store, identical cost to the
pre-optimization path. **No regression possible.**

### Thread safety

- `thread_local` ensures no cross-thread sharing
- Qt's implicit sharing is safe within a single thread
- Memory: ~48 bytes per thread, freed on thread exit

### Recursive calls

If `evaluate()` is called recursively (e.g., from a filter sub-expression),
the thread-local is overwritten. This is safe because the outer caller already
holds a COW copy of the previous result. The COW copy detaches transparently
if mutated.

### Scope

This optimization only applies to the **definite path shortcut** in
`JSONPath::evaluate(const QJsonValue&)`. Non-definite paths (wildcards,
filters, recursive descent) go through the full evaluation pipeline which
manages its own array pooling.

## Alternatives Considered

- **Array pool (acquirePooledArray)**: Doesn't help — moving the array out
  of the pool defeats reuse since the internal CBOR data is moved away.
- **Placement new**: The overhead is in the CBOR backing store allocation,
  not the QJsonArray object itself. Placement new for the object doesn't help.
- **Accept the overhead**: The ~320 ns overhead made `evaluate()` 2-3x slower
  than hand-written Qt code. The optimization closes this gap meaningfully.

## References

- `src/json-query/json-path/JSONPath.cpp` — implementation
- `perf/PERFORMANCE_ROADMAP.md` — broader optimization context
- `perf/performance_baseline.md` — benchmark data
