# Code Quality Cleanup Milestones

Date: 2026-02-08
Status: **COMPLETED**

Findings from cross-module code quality review comparing JSONPointer, JSONPath,
and JSONSchema implementations. All actionable items have been resolved.

---

## JSONPath Cleanup

### M1: Remove Debug Logging from Public API

**Files**: `src/json-query/json-path/JSONPath.cpp`

Removed ~20 lines of verbose `qCDebug` token dumps from the public `evaluate()`
methods. Also removed the now-unused `JSONPathLog.hpp` include.

### M2: Remove `using namespace` from Public Header

**Files**: `include/json-query/json-path/JSONPath.hpp`,
`src/json-query/json-path/JSONPathFilterEmbeddedParsers.cpp`

Removed `using namespace Qt::StringLiterals;` from the public header. The one
dependent file used `_L1` literals — replaced with project-standard `_qt_l1`
from `QtStringLiterals.hpp`.

### M3: Consolidate Header Count  (partial)

**Deleted 4 headers**:

- `JSONPathFilterHelpers.hpp` → merged into `JSONPathFilter.hpp`
- `JSONPathFilterFunctions.hpp` → merged into `JSONPathFilter.hpp`
- `JSONPathFilterOrchestration.hpp` → merged into `JSONPathFilter.hpp`
- `JSONPathFilterComparisonDispatcher.hpp` → merged into `JSONPathFilterComparison.hpp`

**Kept separate** (with justification):

- `JSONPathFilterExistenceParsers.hpp` — has template names (`ExistencePatternDef`)
  that conflict with local specializations in `JSONPathFilterParsers.cpp`.
  Merging would cause template redefinition errors.
- `JSONPathWildcardRecursive.hpp`, `JSONPathPointerConversion.hpp`,
  `JSONPathOption.hpp` — intentionally extracted to break include-weight
  coupling. Comments in the headers document this design choice.

**Net result**: 24 → 20 headers (−4).

### M4: ADR-001 Full Migration Audit

Audited all `.toArray()` call sites in the JSONPath module. All instances are
safe: inline method chains (`.toArray().size()`), argument passing, or copy-init.
Zero migrations needed.

---

## JSONSchema Cleanup

### M5: Extract Ref/DynamicRef Validation Helper

**File**: `src/json-query/json-schema/JSONSchemaValidate.cpp`

Extracted `validateWithResourceScope()` helper that optionally pushes a
`DynamicScopeGuard` before validation. Replaced two identical 11-line blocks
with single-line calls.

### M6: Document Raw Index Loop in UniqueItems

**File**: `include/json-query/json-schema/internal/ValidateArray.hpp`

`std::views::enumerate` is unavailable in the current libc++ version.
Documented the raw index loop with a comment explaining the pairwise comparison
requirement and toolchain limitation. Changed loop variable type to `qsizetype`
for consistency with Qt APIs.

### M7: Reduce Public Header Include Weight

**Files**: `include/json-query/json-schema/JSONSchema.hpp`,
`src/json-query/json-schema/JSONSchema.cpp`

Replaced `#include "internal/SchemaNode.hpp"` with a forward declaration of
`CompiledSchema`. Moved destructor and private constructor out of the header
into the `.cpp` file to allow the `shared_ptr` deleter to see the complete type.

---

## JSONPointer

No action items. JSONPointer is compact, well-structured, and fully compliant
with project code style. It serves as the reference implementation for quality.

---

## Final Verification

All tests pass after all changes:

- **RFC 9535 JSONPath**: 443/443 passed, 1 skipped
- **JSON Schema**: 116/116 passed
