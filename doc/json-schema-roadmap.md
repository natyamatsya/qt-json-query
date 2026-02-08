# JSON Schema Compliance Roadmap

## Current Score

| Configuration      | Passing | Total | Percentage |
| ------------------ | ------- | ----- | ---------- |
| Base (no opt deps) | 1932    | 1994  | 96.9%      |
| With optional deps | 1967    | 1994  | 98.6%      |
| Unit tests         | 100     | 100   | 100%       |

### Remaining 27 failures (with optional deps)

- **hostname** (30) — Unicode hostname tests; ASCII validator can't handle IDN
- **idn-hostname** (24) — libidn2 contextual rule gaps (MIDDLE DOT, KERAIA, bidi)

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

## Phase 5b: Optional Format Dependencies ✅ (+35 tests → 1967 with deps)

Two optional dependencies enabled via CMake options:

```cmake
cmake -DJSON_QUERY_FORMAT_ECMA_REGEX=ON -DJSON_QUERY_FORMAT_IDN=ON ...
```

### SRELL (ECMAScript regex, header-only, BSD license)

**[akenotsuki.com/misc/srell](https://www.akenotsuki.com/misc/srell/en/)** — v4.140, 3 header files

- `SchemaRegex` wrapper abstracts SRELL vs QRegularExpression
- Used for both `regex` format validation AND `pattern` keyword matching
- Compile definition: `JSON_QUERY_HAS_SRELL`
- **Fixes**: All 46 ecmascript-regex tests (44 from `pattern` keyword, 2 from `regex` format)

### libidn2 (IDNA 2008 / RFC 5891+5892, LGPL-3.0)

**[gnu.org/software/libidn](https://www.gnu.org/software/libidn/)** — system library

Install: `brew install libidn2` (macOS) or `apt install libidn2-dev` (Linux)

- Full IDNA 2008 validation via `idn2_lookup_u8` with NFC normalization
- Pre-processes Unicode dot variants (U+3002, U+FF0E, U+FF61) to ASCII periods
- `isIdnHostname` with ASCII fast path + empty label rejection
- Compile definition: `JSON_QUERY_HAS_IDNA`
- **Fixes**: 22 idn-hostname + 2 idn-email tests

### Remaining 27 failures (hostname IDN edge cases)

- **hostname.json (30)**: Unicode hostname tests; `hostname` format uses strict ASCII
  RFC 1123 validator. Routing through libidn2 causes regressions for some edge cases.
- **idn-hostname.json (24)**: libidn2 doesn't fully enforce all RFC 5892 contextual rules
  (MIDDLE DOT, KERAIA, GERESH, KATAKANA MIDDLE DOT, Arabic digit mixing).

---

## Summary

| Phase    | Tests | Status    | Cumulative       |
| -------- | ----- | --------- | ---------------- |
| Phase 1  | +2    | ✅ Done   | 1909/1994        |
| Phase 2  | +2    | ✅ Done   | 1911/1994        |
| Phase 3  | +1    | ✅ Done   | 1912/1994        |
| Phase 4  | +1    | ✅ Done   | 1913/1994        |
| Phase 5a | +19   | ✅ Done   | 1932/1994 (base) |
| Phase 5b | +35   | ✅ Done   | 1967/1994 (opts) |
| Remain   | 27    | ⏳ Future | 1994/1994        |
