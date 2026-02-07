# ADR-002: Two-Layer Format Validation (CTRE + Qt)

- **Status:** Accepted
- **Date:** 2026-02-08
- **Context:** Design of `format` keyword validation in the JSON Schema module

## Problem

JSON Schema's `format` keyword (e.g., `"date-time"`, `"email"`, `"ipv4"`)
requires validating string values against format-specific rules defined by
various RFCs. These rules have two distinct aspects:

1. **Structural** — Does the string match the expected syntax?
   (e.g., `YYYY-MM-DD` for dates, `user@host` for email)
2. **Semantic** — Is the structurally valid string actually meaningful?
   (e.g., `2024-02-30` matches the date pattern but February 30th doesn't exist)

A single validation layer cannot cleanly handle both: regex alone cannot
check calendar validity or IP address ranges, and Qt's parsers sometimes
accept formats that are too permissive for RFC compliance.

## Decision

Format validators use a **two-layer architecture** where each layer has a
distinct responsibility:

### Layer 1: CTRE Pattern Matching (Structural)

[CTRE](https://github.com/hanickadot/compile-time-regular-expressions)
(compile-time regular expressions) validates the syntactic structure of the
value. Patterns are defined as `constexpr ctll::fixed_string` values in
`src/json-query/json-schema/internal/FormatValidators.cpp` and are compiled
at C++ compile time — zero runtime regex compilation cost.

This layer catches:

- Wrong separators (`2024/01/01` instead of `2024-01-01`)
- Missing required components (`12:00` without seconds for time format)
- Invalid characters (`user@@host` for email)
- Structural violations (hostname labels starting with hyphens)

Failure at this layer produces `EvalError::FormatInvalid`.

### Layer 2: Qt Semantic Validation

Qt types (`QDateTime`, `QDate`, `QHostAddress`, `QUrl`,
`QRegularExpression`) validate the semantic correctness of values that
passed the structural check.

This layer catches:

- Invalid calendar dates (`2024-02-30`)
- Out-of-range IP octets (`192.168.1.999`)
- Malformed URIs that pass basic pattern matching
- Invalid regex syntax

Failure at this layer produces `EvalError::FormatSemanticInvalid`.

### Layer Applicability Per Format

Not all formats use both layers. The choice depends on what each
technology handles well:

| Format                  | Layer 1 (CTRE)              | Layer 2 (Qt)            | Rationale                                                                    |
| ----------------------- | --------------------------- | ----------------------- | ---------------------------------------------------------------------------- |
| `date-time`             | `dateTimePattern`           | `QDateTime::fromString` | Pattern ensures RFC 3339 syntax; Qt validates calendar semantics             |
| `date`                  | `datePattern`               | `QDate::fromString`     | Same split as date-time                                                      |
| `time`                  | `timePattern`               | Manual range check      | CTRE validates structure; hand-rolled hour/minute/second range check          |
| `email`                 | `emailPattern`              | —                       | Simplified RFC 5322 pattern is sufficient                                    |
| `hostname`              | `hostnamePattern`           | —                       | RFC 1123 is fully expressible as regex                                       |
| `ipv4`                  | `ipv4Pattern`               | `QHostAddress`          | Pattern validates `N.N.N.N` format; Qt validates 0-255 ranges                |
| `ipv6`                  | —                           | `QHostAddress`          | IPv6 compression/notation is too complex for regex; Qt handles it entirely   |
| `uri`                   | —                           | `QUrl`                  | Qt's URI parser is RFC 3986 compliant; additional scheme-present check       |
| `uri-reference`         | —                           | `QUrl`                  | Qt handles relative and absolute URI references                              |
| `uri-template`          | `uriTemplatePattern`        | —                       | RFC 6570 structure is regex-friendly                                         |
| `uuid`                  | `uuidPattern`               | —                       | Fixed hexadecimal format is fully regex-expressible                           |
| `json-pointer`          | `jsonPointerPattern`        | —                       | RFC 6901 escape rules (`~0`, `~1`) are regex-friendly                        |
| `relative-json-pointer` | `relativeJsonPointerPattern`| —                       | Extension of JSON Pointer pattern                                            |
| `regex`                 | —                           | `QRegularExpression`    | Qt validates ECMA-262 regex syntax                                           |
| `idn-email`             | Reuses `email`              | —                       | Delegated to basic email validator                                           |
| `idn-hostname`          | Reuses `hostname`           | —                       | Delegated to basic hostname validator                                        |

### Error Code Distinction

The two-layer design is reflected in the error model:

```cpp
enum class EvalError : std::uint8_t {
    // ...
    FormatInvalid,         ///< Layer 1: structural pattern mismatch
    FormatSemanticInvalid, ///< Layer 2: Qt semantic check failed
};
```

This distinction allows consumers to differentiate between "not even the
right shape" and "right shape, wrong value" — useful for error messages
and debugging.

### Dispatch Table

Format validators are registered in a compile-time dispatch table
(`kFormatTable`) that maps format name strings to validator functions.
Unknown formats pass validation silently, per the JSON Schema specification
(formats are annotations by default; this library treats them as assertions
for stricter validation).

```cpp
static constexpr std::array kFormatTable{
    FormatEntry{u"date-time", isDateTime},
    FormatEntry{u"date",      isDate},
    // ...
};
```

## Consequences

### Benefits

- **Compile-time pattern validation** — CTRE rejects invalid regex at
  compile time; no runtime surprises from bad patterns.
- **Zero-cost structural checks** — CTRE patterns compile to optimized
  state machines, faster than `QRegularExpression` for fixed patterns.
- **Semantic correctness** — Qt's battle-tested parsers handle edge cases
  (leap years, IPv6 compression, URI encoding) that regex cannot.
- **Granular error reporting** — Two error codes let consumers distinguish
  structural from semantic failures.
- **Extensibility** — New formats only need a function and a dispatch table
  entry.

### Trade-offs

- **Duplicate validation** — For formats using both layers, the string is
  parsed twice (once by CTRE, once by Qt). The CTRE check is fast enough
  that this is negligible in practice.
- **idn-email/idn-hostname are incomplete** — These delegate to basic
  ASCII validators; proper internationalized domain name support would
  require ICU or a dedicated IDN library.
- **Qt version coupling** — Semantic validation accuracy depends on Qt's
  parser quality. Tested against Qt 6.7+.

## Alternatives Considered

1. **Qt-only validation** — Simpler, but `QDateTime::fromString` accepts
   formats that don't conform to RFC 3339 strictly (e.g., missing timezone).
   CTRE provides the strict structural gate.

2. **CTRE-only validation** — Cannot validate semantics (calendar
   correctness, IP ranges). Would need hand-rolled logic for every format.

3. **Runtime `QRegularExpression`** — Works but pays regex compilation cost
   at runtime. CTRE eliminates this entirely.

4. **Single error code for all format failures** — Simpler but loses the
   structural/semantic distinction that aids debugging.

## References

- [JSON Schema Validation §7](https://json-schema.org/draft/2020-12/json-schema-validation#section-7) — Format vocabulary
- [CTRE documentation](https://compile-time-regular-expressions.readthedocs.io/)
- [RFC 3339](https://www.rfc-editor.org/rfc/rfc3339) — Date/Time format
- [RFC 5321](https://www.rfc-editor.org/rfc/rfc5321) — Email format
- [RFC 3986](https://www.rfc-editor.org/rfc/rfc3986) — URI format
- [RFC 4291](https://www.rfc-editor.org/rfc/rfc4291) — IPv6 addressing
- [RFC 1123](https://www.rfc-editor.org/rfc/rfc1123) — Hostname format
