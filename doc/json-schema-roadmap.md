# JSON Schema Compliance Roadmap

## Current Score: 1932/1994 IETF (96.9%)

| Category     | Passing | Total | Status           |
| ------------ | ------- | ----- | ---------------- |
| Unit tests   | 100     | 100   | ✅ Complete      |
| dynamicRef   | 46      | 46    | ✅ Complete      |
| refRemote    | 31      | 31    | ✅ Complete      |
| unevaluated* | 196     | 196   | ✅ Complete      |
| vocabulary   | 5       | 5     | ✅ Complete      |
| IETF overall | 1932    | 1994  | 🔧 62 remaining  |

### Remaining Failures Breakdown

All 62 remaining failures are optional format validators requiring heavyweight features:

- **hostname + idn-hostname** (76) — Need IDNA 2008 Unicode processing (RFC 5891/5892)
- **ecmascript-regex** (46) — Need ECMA-262 regex syntax validator
- **idn-email** (2) — Need IDN-aware email validation

---

## Completed Phases

### Phase 1: Quick Wins ✅ (+2 tests → 1909)

- **1.1 float-overflow**: `std::fmod` fallback when multipleOf quotient overflows to inf
- **1.2 URI format**: Removed over-aggressive userinfo slash check (percent-encoded `%2f` is valid)

### Phase 2: Bundle Meta-Schema ✅ (+2 tests → 1911)

Embedded 8 official 2020-12 meta-schema files as `.inc` raw string literals under
`src/json-query/json-schema/meta-schemas/draft-2020-12/`. Served from `lookupBuiltinSchema()`
before the user's fetcher in `fetchAndCompileRemoteSchema`.

### Phase 3: Vocabulary Support ✅ (+1 test → 1912)

Extract `$vocabulary` from `$schema` metaschema in `compileSchema()`. The
`ctx.validationVocabActive` flag skips TypeConstraints, NumericKeywords, and
validation parts of StringKeywords when the validation vocabulary is absent.

### Phase 4: Cross-Draft Support ✅ (+1 test → 1913)

Detect `$schema` version on fetched remote schemas. Disable `prefixItems`
(2020-12-only keyword) when compiling schemas from older drafts via
`ctx.prefixItemsSupported` flag.

### Phase 5a: Format Annotation-Only ✅ (+19 tests → 1932)

Added `FormatValidation` enum and `SchemaOptions` to the public API:

```cpp
enum class FormatValidation { Auto, Assertion, Annotation };
struct SchemaOptions { FormatValidation formatValidation{FormatValidation::Auto}; };
```

- **Auto**: Derive from `$vocabulary` — annotation-only for standard 2020-12 meta-schema
- **Assertion**: Always validate format (library default for backward compatibility)
- **Annotation**: Never validate format

`CompiledSchema::formatAssertionEnabled` controls validation at runtime.

---

## Remaining: Phase 5b — Unicode Format Validators (+62 tests)

### Hostname / IDN-hostname (+76 tests)

All failures involve internationalized hostnames: Hangul, Punycode, PVALID exceptions,
ZERO WIDTH JOINER/NON-JOINER, Arabic-Indic digits, etc.

**Requires**: IDNA 2008 (RFC 5891/5892) — Unicode category tables, bidi rules, contextual rules.

**Options**: ICU library, or a standalone IDNA implementation.

### ECMA-262 regex validation (+46 tests)

Failures involve Unicode property escapes (`\p{Space_Separator}`), line terminators,
and other ECMA-262-specific regex features different from PCRE/Qt regex.

**Requires**: Dedicated ECMA-262 regex syntax validator (not a full engine, just syntax checking).

### IDN-email (+2 tests)

Hangul domain names in email addresses.

**Requires**: IDN-aware email validation (depends on hostname/IDN-hostname fix).

**Recommendation**: These are all *optional* per the spec (format is annotation-only by default
in 2020-12). Prioritize based on user demand. Consider ICU as a dependency for IDNA support.

---

## Summary

| Phase    | Tests | Status    | Cumulative |
| -------- | ----- | --------- | ---------- |
| Phase 1  | +2    | ✅ Done   | 1909/1994  |
| Phase 2  | +2    | ✅ Done   | 1911/1994  |
| Phase 3  | +1    | ✅ Done   | 1912/1994  |
| Phase 4  | +1    | ✅ Done   | 1913/1994  |
| Phase 5a | +19   | ✅ Done   | 1932/1994  |
| Phase 5b | +62   | ⏳ Future | 1994/1994  |
