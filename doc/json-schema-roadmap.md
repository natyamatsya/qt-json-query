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

## Phase 5b: Optional Format Dependencies ✅ (+33 tests → 1965 with deps)

Two optional dependencies enabled via CMake options:

```cmake
cmake -DJSON_QUERY_FORMAT_ECMA_REGEX=ON -DJSON_QUERY_FORMAT_IDN=ON ...
```

### SRELL (ECMAScript regex, header-only, BSD license)

**[akenotsuki.com/misc/srell](https://www.akenotsuki.com/misc/srell/en/)** — 3 header files

- `SchemaRegex` wrapper abstracts SRELL vs QRegularExpression
- Used for both `regex` format validation AND `pattern` keyword matching
- Compile definition: `JSON_QUERY_HAS_SRELL`
- **Fixes**: All 46 ecmascript-regex tests (44 from `pattern` keyword, 2 from `regex` format)

### ada-url/idna (IDNA 2008 / UTS #46, Apache-2.0/MIT)

**[github.com/ada-url/idna](https://github.com/ada-url/idna)** — v0.4.0

- `isIdnHostname` with ASCII fast path (delegates to strict RFC 1123 for ASCII inputs)
- `isIdnEmail` validates domain part with IDNA
- Compile definition: `JSON_QUERY_HAS_IDNA`
- **Fixes**: 18 idn-hostname + 2 idn-email tests

### Remaining 29 failures (hostname IDN edge cases)

- **hostname.json (30)**: Unicode hostname tests — our `hostname` format uses the strict
  ASCII RFC 1123 validator because ada-url/idna is too lenient for some invalid ASCII hostnames.
  These tests require a hybrid validator that's strict for ASCII but supports Unicode.
- **idn-hostname.json (28)**: IDNA 2008 contextual rules (MIDDLE DOT, KERAIA, GERESH,
  KATAKANA MIDDLE DOT, ZERO WIDTH JOINER/NON-JOINER) — ada-url/idna doesn't fully enforce
  all RFC 5892 contextual rules.

---

## Summary

| Phase    | Tests | Status    | Cumulative       |
| -------- | ----- | --------- | ---------------- |
| Phase 1  | +2    | ✅ Done   | 1909/1994        |
| Phase 2  | +2    | ✅ Done   | 1911/1994        |
| Phase 3  | +1    | ✅ Done   | 1912/1994        |
| Phase 4  | +1    | ✅ Done   | 1913/1994        |
| Phase 5a | +19   | ✅ Done   | 1932/1994 (base) |
| Phase 5b | +33   | ✅ Done   | 1965/1994 (opts) |
| Remain   | 29    | ⏳ Future | 1994/1994        |
