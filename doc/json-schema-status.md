# JSON Schema (Draft 2020-12) — Implementation Status & Milestone Plan

> Last updated: 2026-02-07

## Current Status

The JSON Schema implementation covers the **core Draft 2020-12 keyword set** and claims
**1994/1994** official compliance tests passing. The architecture is clean: a three-phase
compilation pipeline (symbol table → code generation → reference linking) produces a flat
`std::vector<SchemaNode>` for cache-friendly validation.

### Keyword Coverage

| Category | Keywords | Status |
|---|---|---|
| Type constraints | `type`, `enum`, `const` | ✅ Complete |
| String | `minLength`, `maxLength`, `pattern`, `format` | ✅ Complete |
| Numeric | `minimum`, `maximum`, `exclusiveMinimum`, `exclusiveMaximum`, `multipleOf` | ✅ Complete |
| Array | `minItems`, `maxItems`, `uniqueItems`, `prefixItems`, `items`, `contains` | ✅ Complete |
| Object | `properties`, `patternProperties`, `additionalProperties`, `required`, `propertyNames`, `minProperties`, `maxProperties`, `dependentRequired`, `dependentSchemas` | ✅ Complete |
| Combinators | `allOf`, `anyOf`, `oneOf`, `not`, `if`/`then`/`else` | ✅ Complete |
| References | `$ref`, `$defs`, `definitions`, `$anchor`, `$id`, `$schema` | ✅ Complete |
| Format validation | `date-time`, `date`, `time`, `email`, `hostname`, `ipv4`, `ipv6`, `uri`, `uri-reference`, `uri-template`, `uuid`, `json-pointer`, `relative-json-pointer`, `regex` | ✅ Complete |
| Boolean schemas | `true` / `false` | ✅ Complete |
| Metadata | `title`, `description`, `$comment` | ✅ Stored (not validated) |

### Identified Gaps

| Feature | Current State | Gap |
|---|---|---|
| `unevaluatedProperties` | Field in `ObjectSchema`, error code in `EvalError` | No compile logic, no validation logic |
| `unevaluatedItems` | Field in `ObjectSchema`, error code in `EvalError` | No compile logic, no validation logic |
| `minContains` / `maxContains` | Fields in `ObjectSchema` | No compile logic, no validation logic |
| `$dynamicRef` / `$dynamicAnchor` | `dynamicAnchors` map in `CompiledSchema` | No compile logic, no resolve logic |
| `contentEncoding` | Error code in `EvalError` | No compile logic, no validation logic |
| `contentMediaType` | — | Not represented at all |
| `contentSchema` | — | Not represented at all |

### Infrastructure Notes

- The IETF compliance submodule (`compliance/ietf-json-schema-draft-2020-12/`) is checked in but **empty** — needs `git submodule update --init`.
- No TODOs or FIXMEs exist in schema source or tests.
- Test suite: 4 unit test files + 1 parameterized IETF compliance runner.

---

## Milestone Plan

### Milestone 1 — `minContains` / `maxContains`

**Effort:** Small — straightforward integer keywords extending existing `contains` logic.

**Tasks:**

1. **Compile**: In `compileArrayKeywords()` (`CompileDispatch.hpp`), parse `minContains` and `maxContains` via `parseIntegerKeyword()` and assign to `node.minContains` / `node.maxContains`.
2. **Validate**: In `validateArray()` (`ValidateArray.hpp`), change the `contains` block from a simple boolean `found` to counting matches, then check the count against `minContains` (default 1) and `maxContains`.
3. **Test**: Add unit tests covering:
   - `contains` + `minContains` (require ≥ N matches)
   - `contains` + `maxContains` (require ≤ N matches)
   - `minContains: 0` (contains is satisfied even with zero matches)
   - Edge case: `minContains` without `contains`

---

### Milestone 2 — `unevaluatedProperties`

**Effort:** Medium — requires tracking which properties were "evaluated" across `properties`, `patternProperties`, `additionalProperties`, and all combinators/conditionals.

**Tasks:**

1. **Design evaluation tracking**: Add an `EvaluatedTracker` (or extend `ValidateContext`) with a `std::unordered_set<QString> evaluatedProperties` that records property names touched during validation.
2. **Propagate tracking**: Update `validateSingleProperty()` to record each property that is matched by `properties`, `patternProperties`, or `additionalProperties`. Update combinator validators (`allOf`, `anyOf`, `oneOf`, `if/then/else`) to merge their evaluated sets into the parent.
3. **Compile**: In `compileObjectKeywords()`, compile `unevaluatedProperties` sub-schema via `compile(ctx, ...)` and assign to `node.unevaluatedProperties`.
4. **Validate**: After all other object validation in `validateObject()`, iterate remaining (unevaluated) properties and validate them against the `unevaluatedProperties` schema.
5. **Test**: Add unit tests covering:
   - `unevaluatedProperties: false` with `properties` only
   - `unevaluatedProperties` with `patternProperties`
   - `unevaluatedProperties` interacting with `allOf`, `anyOf`, `oneOf`
   - `unevaluatedProperties` interacting with `if/then/else`
   - Nested `unevaluatedProperties`

---

### Milestone 3 — `unevaluatedItems`

**Effort:** Medium — analogous to `unevaluatedProperties` but for arrays. Requires tracking which array indices were evaluated by `prefixItems`, `items`, and `contains`.

**Tasks:**

1. **Extend evaluation tracking**: Add `std::unordered_set<std::size_t> evaluatedIndices` to the tracker.
2. **Propagate tracking**: Update `validateArray()` to record indices validated by `prefixItems`, `items`, and `contains`. Merge evaluated indices from combinators.
3. **Compile**: In `compileArrayKeywords()`, compile `unevaluatedItems` sub-schema and assign to `node.unevaluatedItems`.
4. **Validate**: After all other array validation, validate remaining indices against `unevaluatedItems`.
5. **Test**: Add unit tests covering:
   - `unevaluatedItems: false` with `prefixItems`
   - `unevaluatedItems` with `items`
   - `unevaluatedItems` interacting with `contains`
   - `unevaluatedItems` interacting with combinators

---

### Milestone 4 — `$dynamicRef` / `$dynamicAnchor`

**Effort:** Medium-High — requires a runtime scope chain for dynamic anchor resolution, which is fundamentally different from static `$ref`.

**Tasks:**

1. **Compile `$dynamicAnchor`**: During schema compilation, when a `$dynamicAnchor` keyword is encountered, register it in `CompiledSchema::dynamicAnchors` (mapping anchor name → node index).
2. **Compile `$dynamicRef`**: Create a new node variant or extend `RefSchema` with a `isDynamic` flag and the dynamic anchor name.
3. **Runtime resolution**: During validation, maintain a stack/scope of schemas being evaluated. When a `$dynamicRef` is encountered, walk the scope chain outward to find the nearest `$dynamicAnchor` with that name.
4. **Extend `ValidateContext`**: Add a `std::vector<const CompiledSchema*>` or similar scope stack for dynamic resolution.
5. **Test**: Add unit tests covering:
   - Basic `$dynamicRef` / `$dynamicAnchor` resolution
   - Dynamic anchor overriding in nested schemas
   - `$dynamicRef` falling back to `$ref` behavior when no dynamic anchor is found
   - Recursive schemas using `$dynamicRef`

---

### Milestone 5 — `content*` Keywords (Optional)

**Effort:** Low — these are annotation-only by default in Draft 2020-12. Full validation is optional per the spec.

**Tasks:**

1. **Compile**: Parse `contentEncoding`, `contentMediaType`, `contentSchema` as optional string/schema fields in `ObjectSchema`.
2. **Validate** (optional): If opted in, decode `contentEncoding` (e.g., base64), check `contentMediaType`, and validate decoded content against `contentSchema`.
3. **Test**: Add basic compile/store tests. If validation is implemented, test base64 decoding + JSON media type.

---

### Milestone 6 — Compliance Hardening

**Effort:** Variable — depends on gaps discovered.

**Tasks:**

1. Initialize the IETF compliance submodule (`git submodule update --init`).
2. Run the full compliance suite and capture the failure list.
3. Triage failures into the above milestones or as standalone bug fixes.
4. Target **100%** of the required (non-optional) test suite passing.

---

## Suggested Priority Order

| Priority | Milestone | Rationale |
|---|---|---|
| 1 | M1: `minContains`/`maxContains` | Quick win, extends existing code |
| 2 | M6: Compliance hardening | Establishes baseline, may reveal other gaps |
| 3 | M2: `unevaluatedProperties` | High-value keyword for real-world schemas |
| 4 | M3: `unevaluatedItems` | Reuses tracking from M2 |
| 5 | M4: `$dynamicRef`/`$dynamicAnchor` | Niche but needed for full spec compliance |
| 6 | M5: `content*` keywords | Optional per spec, lowest priority |
