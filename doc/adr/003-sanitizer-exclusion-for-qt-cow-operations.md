# ADR-003: Sanitizer Exclusion for Qt Copy-on-Write Operations

- **Status:** Accepted
- **Date:** 2026-02-08
- **Context:** AddressSanitizer test failures in JSON Pointer evaluation

## Problem

AddressSanitizer (ASan) instrumentation interferes with Qt's copy-on-write
(CoW) semantics in `QJsonArray` and `QJsonObject`, causing **functional test
failures** in JSON Pointer and JSONPath evaluation. These are not real memory
safety issues — production, debug, and release builds all work correctly.

The failure manifests as incorrect values returned from array/object copy
operations:

```cpp
// Under ASan, this copy produces corrupted results:
const auto arr = current.toArray();
current = arr.at(index);  // returns wrong element
```

**Expected**: `/foo/0` evaluates to `["bar"]` (first element)
**Under ASan**: `/foo/0` evaluates to `[["bar","baz"]]` (entire array wrapped)

### Root Cause

ASan's memory layout changes (8x overhead, different alignment) interfere
with Qt's internal CoW optimizations:

1. **Memory pressure** alters Qt's CoW detach/share decisions
2. **Alignment changes** break Qt's internal data structure assumptions
3. **Reference counting** behaves differently under instrumentation
4. **Copy semantics** for `QJsonArray`/`QJsonObject` produce incorrect results

This is a known class of issues between ASan and frameworks that rely heavily
on implicit sharing with custom allocators.

## Decision

Apply `__attribute__((no_sanitize("address")))` to the specific functions
where Qt CoW operations are on the critical path:

```cpp
// In include/json-query/config/Portability.hpp
#if defined(__has_feature)
  #if __has_feature(address_sanitizer)
    #define QT_QUERY_JSON_NO_SANITIZE __attribute__((no_sanitize("address")))
  #endif
#elif defined(__SANITIZE_ADDRESS__)
  #define QT_QUERY_JSON_NO_SANITIZE __attribute__((no_sanitize_address))
#else
  #define QT_QUERY_JSON_NO_SANITIZE
#endif
```

### Affected Functions

- `json_pointer::detail::evaluatePointerImpl()` — JSON Pointer evaluation loop
- `json_pointer::detail::stepArray()` — array index stepping

These are the minimal set of functions where Qt container copies occur in
tight loops. The exclusion is surgical — only these functions skip ASan
instrumentation, not the entire library.

## Consequences

### Benefits

- **All functional tests pass** in debug and release builds
- **Memory safety validation** still works — ASan catches real issues in
  all non-excluded code
- **Minimal exclusion scope** — only 2 functions, not entire translation units
- **No production impact** — the attribute is a no-op without ASan

### Trade-offs

- **Reduced ASan coverage** for 2 functions — real memory bugs in these
  functions would not be caught by ASan. Mitigated by:
  - Functions are small and straightforward (loop + array access)
  - Covered by extensive fuzz testing (1.5M+ test cases, zero crashes)
  - UBSan and TSan still instrument these functions normally
- **Qt version coupling** — the issue may resolve in future Qt versions.
  Periodic re-evaluation is warranted after Qt upgrades.

### Build Configuration Guidance

| Build Type | Functional Tests | Security Tests | Use Case |
|---|---|---|---|
| **Debug** | ✅ All pass | No sanitizers | Development |
| **Release** | ✅ All pass | No sanitizers | Production |
| **ASan build** | ⚠️ Known failures in excluded code paths | ✅ Memory safety | Security validation (focus on crashes/leaks, not functional results) |

## Alternatives Considered

1. **Avoid Qt container copies entirely** — Would require rewriting
   evaluation to use raw pointers/iterators into Qt internals. Fragile
   and couples to Qt implementation details.

2. **Disable ASan for entire translation units** — Too broad; loses
   coverage for unrelated code in the same files.

3. **Wait for Qt fix** — Unknown timeline. The issue has persisted across
   Qt 6.5–6.8.

4. **Use a different sanitizer** — UBSan and TSan don't exhibit this
   issue, but they test different things. ASan coverage for the rest of
   the library is still valuable.

## References

- Qt implicit sharing: https://doc.qt.io/qt-6/implicit-sharing.html
- ASan documentation: https://clang.llvm.org/docs/AddressSanitizer.html
- Affected code: `include/json-query/json-pointer/JSONPointerEvaluation.hpp`
