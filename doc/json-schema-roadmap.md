# JSON Schema Compliance Roadmap

## Current Score: 1907/1994 IETF (95.6%)

| Category     | Passing | Total | Status           |
| ------------ | ------- | ----- | ---------------- |
| Unit tests   | 100     | 100   | ✅ Complete      |
| dynamicRef   | 46      | 46    | ✅ Complete      |
| refRemote    | 31      | 31    | ✅ Complete      |
| unevaluated* | 196     | 196   | ✅ Complete      |
| IETF overall | 1907    | 1994  | 🔧 87 remaining |

### Remaining Failures Breakdown

- **81 optional format** — idn-hostname (24), ecmascript-regex (23), format (19), hostname (15)
- **6 core** — detailed below

---

## Phase 1: Quick Wins (+2 tests)

### 1.1 float-overflow (optional, +1)

**Test**: `float-overflow.json` — `1e308` with `{"type": "integer", "multipleOf": 0.5}`

**Problem**: `1e308 / 0.5 = 2e308 = inf`, triggering our overflow guard. But all integers ARE multiples of 0.5.

**Fix**: Use `std::fmod` as a fallback when the quotient overflows:

```cpp
if (!std::isfinite(quotient))
{
    // Fallback: fmod handles large values without overflow
    if (std::fmod(value, divisor) != 0.0)
        // report error
}
```

**Effort**: ~5 min, single function change in `ValidateNumeric.hpp`

### 1.2 URI format edge case (optional, +1)

**Test**: `uri.json` — "a valid URL with many special characters"

**Problem**: Our URI format validator rejects a valid URI with special characters.

**Fix**: Audit and fix the URI regex/parser in `FormatValidators.cpp`.

**Effort**: ~30 min, format validator fix

---

## Phase 2: Bundle Meta-Schema (+2 tests)

### 2.1 Embed the 2020-12 meta-schema

**Tests fixed**: `ref.json/"remote ref, containing refs itself"` + `defs.json/"validate definition against metaschema"`

Both reference `$ref: "https://json-schema.org/draft/2020-12/schema"`. Without the meta-schema available, these fail.

**Approach** (following nlohmann's pattern):

1. Download the official 2020-12 meta-schema JSON
2. Embed it as a `constexpr` string literal or `QByteArray` in a generated `.cpp` file
3. In `fetchAndCompileRemoteSchema`, check for the meta-schema URI before calling the user's fetcher
4. Register the built-in meta-schema as a fallback

**Effort**: ~2 hours

**Files**:

- New: `src/json-query/json-schema/meta-schema-2020-12.cpp` (embedded JSON)
- Modified: `JSONSchemaCompile.cpp` (built-in fetcher fallback)

---

## Phase 3: Vocabulary Support (+1 test)

### 3.1 Parse `$vocabulary` from meta-schemas

**Test**: `vocabulary.json` — custom metaschema with no validation vocabulary

**Problem**: When a schema's `$schema` points to a metaschema that declares `$vocabulary` without the validation vocabulary, validation keywords (type, minimum, etc.) should be ignored.

**Approach**:

1. When compiling a schema, fetch and parse its `$schema` metaschema
2. Extract `$vocabulary` to determine which keyword groups are active
3. Skip compilation of keywords from disabled vocabularies
4. Define vocabulary → keyword group mapping

**Effort**: ~1 day (new feature, touches compilation pipeline)

**Files**:

- Modified: `CompileContext.hpp` (active vocabularies state)
- Modified: `JSONSchemaCompile.cpp` (vocabulary-aware compilation)
- Modified: `CompileDispatch.hpp` (conditional keyword dispatch)

---

## Phase 4: Cross-Draft Support (+1 test, optional)

### 4.1 Historic draft detection and processing

**Test**: `cross-draft.json` — `$ref` to a draft-2019-09 schema

**Problem**: When `$ref` targets a schema with `$schema` pointing to an older draft, that schema should be processed under the rules of that draft (e.g., `items` instead of `prefixItems`).

**Approach**:

1. Detect `$schema` version on fetched remote schemas
2. Apply draft-specific keyword aliases (e.g., `items` → `prefixItems` in 2019-09)
3. Potentially maintain per-draft compilation rules

**Effort**: ~2-3 days (architectural, requires draft-version abstraction)

**Recommendation**: Defer to post-1.0 unless full compliance is a hard requirement.

---

## Phase 5: Optional Format Validators (+81 tests)

### 5.1 IDN hostname validation (+24)

Requires Unicode IDNA (RFC 5891) — typically via ICU or a dedicated library.

### 5.2 ECMA-262 regex validation (+23)

Requires an ECMA-262 regex engine (different from PCRE/Qt regex).

### 5.3 Hostname validation improvements (+15)

Fix edge cases in hostname format checking.

### 5.4 Format validator fixes (+19)

IRI-reference, URI-reference, and other format edge cases.

**Recommendation**: These are all *optional* per the spec (format is annotation-only by default in 2020-12). Prioritize based on user demand.

---

## Summary

| Phase   | Tests | Effort    | Priority  | Cumulative |
| ------- | ----- | --------- | --------- | ---------- |
| Phase 1 | +2    | ~1 hour   | Do now    | 1909/1994  |
| Phase 2 | +2    | ~2 hours  | Do now    | 1911/1994  |
| Phase 3 | +1    | ~1 day    | Pre-1.0   | 1912/1994  |
| Phase 4 | +1    | ~2-3 days | Post-1.0  | 1913/1994  |
| Phase 5 | +81   | ~1-2 wks  | On demand | 1994/1994  |
