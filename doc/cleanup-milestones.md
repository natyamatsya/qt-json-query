# Code Quality Cleanup Milestones

Date: 2026-02-08

Findings from cross-module code quality review comparing JSONPointer, JSONPath,
and JSONSchema implementations.

---

## JSONPath Cleanup

JSONPath has the most technical debt. These milestones are ordered by impact.

### M1: Remove Debug Logging from Public API

**Files**: `src/json-query/json-path/JSONPath.cpp`

The public `evaluate()` and `evaluateAll()` methods contain verbose `qCDebug`
loops that dump every token on every call. These should be removed — the public
API layer should be silent; debug logging belongs in the internal evaluation
pipeline only.

**Scope**: ~20 lines deleted from `JSONPath.cpp`.

### M2: Remove `using namespace` from Public Header

**File**: `include/json-query/json-path/JSONPath.hpp`

Line 30 has `using namespace Qt::StringLiterals;` which pollutes the namespace
of every consumer that includes the header. Replace with scoped `using`
declarations inside function bodies, or use the project's `_qt_s` literal
operator pattern.

**Scope**: 1 line changed in header, audit call sites that rely on it.

### M3: Consolidate Header Count

**Directory**: `include/json-query/json-path/` (24+ headers + `internal/`)

Many headers are small and tightly coupled (e.g., `JSONPathFilterHelpers.hpp`
at 1 KB, `JSONPathFilterFunctions.hpp` at 1.2 KB). Consolidate related filter
headers into fewer files:

- Merge `JSONPathFilterHelpers.hpp` + `JSONPathFilterFunctions.hpp` +
  `JSONPathFilterComparison.hpp` + `JSONPathFilterComparisonDispatcher.hpp`
  into a single `JSONPathFilter.hpp` (or at most two: parse + eval).
- Merge `JSONPathWildcardRecursive.hpp` into `JSONPathEvaluate.hpp`.

**Scope**: ~6-8 headers consolidated, no behavioral change.

### M4: ADR-001 Full Migration

**Scope**: Audit all remaining `.toArray()` and `.toObject()` call sites in the
JSONPath module. The ADR noted ~20 unmigrated instances. Most are inline
(`.toArray().size()`) which are safe, but each should be verified.

Run: `grep -rn '\.toArray()' src/json-query/json-path/` and classify each as
safe-inline vs needs-migration.

---

## JSONSchema Cleanup

JSONSchema is the cleanest module. These are polish items.

### M5: Extract Ref/DynamicRef Validation Helper

**File**: `src/json-query/json-schema/JSONSchemaValidate.cpp`

The `RefSchema` and `DynamicRefSchema` branches in `validateNode()` both
contain identical scope-push-then-validate logic (lines 159-170 and 192-203).
Extract into a helper:

```cpp
void validateWithResourceScope(ValidateContext& ctx,
                               std::size_t targetIndex,
                               const QJsonValue& instance,
                               const QString& instancePath,
                               const QString& schemaPath,
                               ValidateNodeFn& validateNode);
```

**Scope**: ~15 lines extracted, two call sites simplified.

### M6: Fix Raw Index Loop in UniqueItems

**File**: `include/json-query/json-schema/internal/ValidateArray.hpp`

The uniqueItems check uses a raw O(n²) nested index loop. Options:

1. Use `std::views::enumerate` + inner range (preferred per AGENTS.md)
2. Add a comment documenting why raw indices are needed (adjacent comparison)
3. Consider a hash-based O(n) approach for large arrays

**Scope**: ~5 lines refactored.

### M7: Reduce Public Header Include Weight

**File**: `include/json-query/json-schema/JSONSchema.hpp`

Currently includes `internal/SchemaNode.hpp`, exposing the full `CompiledSchema`
and `ObjectSchema` types to consumers. Since `JSONSchema` only holds a
`shared_ptr<const internal::CompiledSchema>`, a forward declaration suffices:

```cpp
namespace json_query::json_schema::internal { struct CompiledSchema; }
```

Move the `#include "internal/SchemaNode.hpp"` to `JSONSchema.cpp` and
`JSONSchemaCompile.cpp`.

**Scope**: 1 header change, 2 source file include additions.

---

## JSONPointer

No action items. JSONPointer is compact, well-structured, and fully compliant
with project code style. It serves as the reference implementation for quality.

---

## Priority Order

| Priority | Milestone | Module | Impact |
|----------|-----------|--------|--------|
| 1 | M2 | JSONPath | Namespace pollution in public header |
| 2 | M5 | JSONSchema | DRY violation in validation |
| 3 | M1 | JSONPath | Debug noise in public API |
| 4 | M7 | JSONSchema | Header weight / encapsulation |
| 5 | M6 | JSONSchema | Style guide compliance |
| 6 | M4 | JSONPath | ADR-001 audit |
| 7 | M3 | JSONPath | Header consolidation (largest scope) |
