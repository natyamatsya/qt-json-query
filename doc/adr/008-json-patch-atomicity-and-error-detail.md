# ADR-008: JSON Patch Atomicity via Functional Apply; Error Detail Carries the Op Index

- **Status:** Accepted
- **Date:** 2026-07-10
- **Context:** RFC 6902 §5 requires all-or-nothing patch application; the
  4-byte `Error` struct (ADR-004 context) has a single 16-bit `detail` field
  that the pointer layer already uses for the failing token index.

## Problem

1. RFC 6902 patches are multi-operation. If `apply` mutated the caller's
   document as it went, a mid-patch failure would have to undo prior
   operations (journal/rollback machinery) to honor §5.
2. When operation *k* fails, callers need to know **which operation** failed
   and **why**. The underlying "why" is usually a pointer-evaluation error
   whose `detail` is a *token index* — but a token index without its
   operation is useless to a patch caller, and `Error::detail` cannot carry
   both indices.

## Decision

### Functional apply

`JSONPatch::apply(document)` is functional: operations run in order against
a working `QJsonValue` copy (cheap COW handle; the pointer primitives'
strong guarantee — ADR-007 — keeps each step all-or-nothing too), and the
patched document is *returned*, never written into the input. Atomicity
falls out by construction: on failure the caller's document was never
touched. `applyInPlace(document)` is a two-line wrapper that assigns the
result back on success only.

Operations compose the pointer primitives: `add`/`replace`/`remove` call
them directly (never `set`-with-create — RFC 6902 `add` must not create
intermediates); `copy` = `evaluate(from)` + `add(path)`; `move` =
descendant-prefix check, then `remove(from)` + `add(path)`; `test` =
`evaluate(path)` + deep equality. The `test` equality follows §4.6: numbers
compare numerically (Qt's CBOR backend distinguishes `1` and `1.0`
representations, so a normalizing `jsonDeepEquals` is used, not
`QJsonValue::operator==`). The move descendant check compares canonical
`to_string()` forms with a `/` boundary, so `/a` → `/ab` (shared name
prefix, different member) is correctly allowed.

### Error detail = operation index

Errors surfaced by `create()` and `apply()` carry the **operation index**
in `Error::detail`:

- Patch-specific failures (`TestFailed`, `MoveIntoOwnDescendant`) use the
  new `ErrorDomain::PatchEval`; structural rejections use
  `ErrorDomain::PatchParse`.
- Failures of the underlying pointer operations keep their `PointerEval`
  domain **and code** (callers can still distinguish `KeyNotFound` from
  `IndexOutOfRange`), but `apply()` rewrites `detail` from the token index
  to the op index — the op index is the actionable coordinate at this layer,
  and the 4-byte `Error` cannot carry both.
- `Error::formatted_message()` renders `(at operation N)` for the patch
  domains. For propagated pointer errors it still says `(at token N)`;
  `apply()`'s documentation states that its errors' `detail` is always the
  op index.

The new `PatchParse`/`PatchEval` domains add a seventh and eighth arm to the
`ErrorDomain` wiring in `utils/JSONError.hpp`; the ROADMAP item about
collapsing that duplication now covers them too (deliberately not refactored
in the same change).

## Consequences

- `apply` can be called on shared/const documents without defensive copies.
- A failed `applyInPlace` provably leaves the target untouched (unit- and
  fuzz-asserted).
- The community json-patch-tests suite passes completely (empty
  KnownFailures table); the atomicity property is additionally covered by a
  dedicated mid-patch-failure unit test.
- Trade-off accepted: the token index of a propagated pointer error is not
  recoverable from a patch error. Callers who need it can re-run the single
  failing op's pointer directly.
