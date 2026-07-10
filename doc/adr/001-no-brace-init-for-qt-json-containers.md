# ADR-001: No Brace-Initialization for QJsonArray and QJsonObject

- **Status:** Accepted (amended 2026-07-10: compiler-divergence note — the
  footgun can be invisible on MSVC and fire only on GCC/clang)
- **Date:** 2026-02-07
- **Context:** Bug investigation during JSON Schema test failures

## Problem

Nine JSON Schema unit tests were failing silently due to a subtle C++ language
interaction between our project-wide brace-initialization style and Qt's
`QJsonArray` type.

The pattern:

```cpp
const auto arr{someValue.toArray()};   // BUG: double-wraps the array
```

produces a **completely different result** than:

```cpp
const auto arr = someValue.toArray();  // Correct: copies the array
```

## Root Cause

`QJsonArray` has two relevant properties:

1. An `std::initializer_list<QJsonValue>` constructor.
2. An **implicit** conversion to `QJsonValue` (via `QJsonValue(QJsonArray)`).

When brace-initialization is used, C++ list-initialization rules
**prefer the `initializer_list` constructor** over copy/move constructors
whenever a viable conversion exists. So:

```cpp
QJsonArray original = /* ["red", "green", "blue"] */;
QJsonArray copy{original};
//  ↑ Calls QJsonArray(std::initializer_list<QJsonValue>{ QJsonValue(original) })
//  ↑ Result: [["red", "green", "blue"]]  — the original wrapped as a single element!
```

This is **correct C++ behavior** per the standard (§12.6.2.2 — if an
`initializer_list` constructor is viable, it wins over non-`initializer_list`
constructors in list-initialization). It is not a compiler or Qt bug.

## Impact

Every `const auto x{expr.toArray()}` in the codebase silently wrapped the
resulting array inside another array. This caused:

- **Enum validation failures** — `["red","green","blue"]` became
  `[["red","green","blue"]]`, so no string ever matched.
- **Type-array parsing failures** — `["string","number"]` became a
  single-element array containing an array.
- **Required-array parsing failures** — `["name","age"]` became unparseable.
- **Schema compilation failures** — any schema keyword backed by a JSON array
  was silently corrupted.

All 9 test failures traced back to this single root cause.

## Decision

Use the `BraceSafe<T>` wrapper (`include/json-query/utils/BraceSafe.hpp`) via
the `asArray()` / `asObject()` factory functions. This preserves our
brace-init style while eliminating the footgun:

```cpp
// ✅ Preferred — brace-init style, safe via BraceSafe wrapper
const auto arr{asArray(value)};       // BraceSafe<QJsonArray>, not QJsonArray
for (const auto& item : arr) { ... }  // range-for works
arr.size();                            // forwarded
arr[0];                                // forwarded
someFunc(arr);                         // implicit conversion to const QJsonArray&
someFunc(arr.get());                   // explicit unwrap when needed

// ✅ Also acceptable — copy-init as a simpler fallback
const auto arr = value.toArray();

// ❌ Dangerous — triggers initializer_list constructor
const auto arr{value.toArray()};
```

`BraceSafe<T>` is a thin aggregate that wraps the container. Because `auto`
deduces `BraceSafe<QJsonArray>` (not `QJsonArray`), brace-init uses the
aggregate/copy constructor — the `initializer_list` trap never fires.

The wrapper forwards `begin()`/`end()`, `size()`, `isEmpty()`, `operator[]`,
`contains()`, `find()`, and provides implicit conversion to `const T&` for
seamless interop with functions expecting the raw container.

### Why the footgun exists

The exception applies to any Qt type that has both:

- An `std::initializer_list<U>` constructor, AND
- An implicit conversion to `U`

Currently known affected types:

| Type | `initializer_list` element | Implicit conversion |
| --- | --- | --- |
| `QJsonArray` | `QJsonValue` | `QJsonArray` → `QJsonValue` ✅ triggers bug |
| `QJsonObject` | `QPair<QString, QJsonValue>` | No single-value conversion — **safe today** |

`QJsonObject` is not currently affected because a lone `QJsonObject` cannot
implicitly convert to `QPair<QString, QJsonValue>`. We provide `asObject()`
anyway for consistency and future-proofing.

### Compiler divergence (2026-07-10 amendment)

For the *same-type* single-element case (`const auto x{exprYieldingQJsonArray}`
where the element already is a `QJsonArray`), compilers disagree: MSVC applies
the single-element same-type rule ([dcl.init.list]/3.2, CWG DR 1467) and
copies, while GCC and clang (Linux and Apple) select the `initializer_list`
constructor and wrap. Consequence: code violating this ADR can pass every
local MSVC build and test run and fail only on the GCC/clang CI legs — which
is exactly how the JSONPatch unit tests failed when first written with
`const auto patch{parsePatchJson(...)}`. Treat green-on-MSVC as no evidence of
brace-init safety; only copy-init (or `asArray()`/`asObject()`) is portable.

## Scope

This affects **all code** that stores the result of `.toArray()` in a local
variable, not just the JSON Schema module. The JSONPath module has ~20
instances that should also be migrated.

## Alternatives Considered

1. **Use `auto x(expr.toArray())`** — Direct-initialization with parentheses
   avoids `initializer_list`. Works, but inconsistent with the rest of the
   codebase and less readable.

2. **Use `QJsonArray x = expr.toArray()`** — Explicit type avoids `auto`
   deduction ambiguity. More verbose; `auto` with `=` is sufficient.

3. **Wrap `.toArray()` in a helper** — e.g., `safeArray(value)` that returns
   by value through a non-`initializer_list` path. Over-engineered for this
   issue.

4. **Disable the project brace-init rule entirely** — Too broad; brace-init
   is valuable for preventing narrowing conversions elsewhere.

## References

- [CppCoreGuidelines ES.23](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Res-list)
  — Prefer `{}` initializer syntax, but notes the `initializer_list` pitfall.
- [Scott Meyers, Effective Modern C++, Item 7](https://www.oreilly.com/library/view/effective-modern-c/9781491908419/)
  — "Distinguish between `()` and `{}` when creating objects."
- Qt documentation: `QJsonArray::QJsonArray(std::initializer_list<QJsonValue>)`
