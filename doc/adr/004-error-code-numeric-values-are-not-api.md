# ADR-004: Error-Code Numeric Values Are Not API

- **Status:** Accepted
- **Date:** 2026-07-03
- **Context:** Production-readiness review flagged that `Error::numeric()`
  values depend on enumerator order

## Problem

All error enums (`json_path::ParseError`/`EvalError`,
`json_pointer::ParseError`/`EvalError`, `json_schema::ParseError`/`EvalError`,
`ConvertError`, `ErrorDomain`) use implicit, order-derived numeric values.
These values surface to consumers through:

- `Error::numeric()` / `Error::from_numeric()` (`utils/JSONError.hpp`)
- `Error::code` (public member)
- `ValidationError::toJson()` (`json-schema/JSONSchemaResult.hpp`), which
  embeds `code.numeric()` in machine-readable output

Inserting or reordering an enumerator shifts every subsequent value. Two
options were considered:

1. **Freeze the values** — assign explicit numerics and adopt an append-only
   policy. This stabilizes persisted values but forever forbids reordering:
   enumerators could no longer be kept alphanumerically sorted, and every
   future addition lands at the end regardless of logical grouping.
2. **Declare the values non-API** — keep enumerators freely sortable
   (alphanumeric order preferred) and document that the numeric values are an
   implementation detail.

## Decision

**Numeric error-code values are not part of the library's API contract.**

- Enumerators may be reordered, inserted, or renamed between library versions;
  maintainers keep them alphanumerically sorted where practical.
- Consumers MUST NOT persist, transmit, or hard-code numeric error values
  (`Error::numeric()`, `Error::code`, the `code` field of
  `ValidationError::toJson()` output). These values are only meaningful within
  a single library version.
- Consumers SHOULD branch on error identity using the enumerators themselves
  (e.g. `err == Error{EvalError::KeyNotFound}`) or the `ErrorDomain`
  predicates — these are stable as *names*, not as numbers.
- `Error::numeric()` / `from_numeric()` remain useful for compact in-process
  transport (logging, hashing, passing through non-typed layers) where writer
  and reader are the same build.

## Consequences

- Enum declarations stay clean and sortable; adding an error in its natural
  alphabetical position is a non-breaking change.
- Any future need for stable wire codes must be met by an explicit mapping
  layer (e.g. name-string output), not by the enum values. If
  `ValidationError::toJson()` output is ever consumed across versions, its
  `code` field should be migrated to a symbolic string.
- Documentation duty: `Error::numeric()` doc comment and README must not
  describe the values as stable (fixed alongside this ADR).
