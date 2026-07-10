# ADR-007: Container-Relative Tokens and In-Place Pointer Writes

- **Status:** Accepted
- **Date:** 2026-07-10
- **Context:** Downstream feedback requested writable JSON Pointers (a
  `set(document, pointer, value)` shape or RFC 6902) to replace hand-written
  extract→modify→write-back ladders and a bespoke settings-backend pointer
  engine. A design review of the read path surfaced a related RFC 6901
  compliance gap that write semantics would otherwise inherit.

## Problem

Three coupled decisions:

1. **Token semantics.** The parser classified all-digit tokens as
   `Kind::Index` with an empty `key`, and the evaluator rejected Index tokens
   against objects — so `/foo/5` could not resolve object member `"5"`,
   although RFC 6901 §4 interprets each token relative to the container it
   meets at evaluation time. Leading-zero (`01`) and out-of-range digit
   tokens were rejected at parse time, although the RFC only forbids them as
   *array indices*. Write support (RFC 6902's `-` append token, numeric
   member names) needs container-relative interpretation to be spec-correct.
2. **Write API shape.** Qt JSON types are copy-on-write value types with no
   deep-reference mechanism (`QJsonValueRef` only reaches one level), so a
   pointer write cannot mutate in place along a path; something must rebuild
   the spine. Where does that live, and does it mutate the caller's document
   or return a new one?
3. **Read-path cost.** The downstream consumer asked whether write support
   should sit behind a compile-time read/write mode switch to protect read
   performance.

## Decision

### Container-relative tokens (read and write)

`Token::key` is always populated, including for Index tokens (which
additionally carry the parsed numeric value). Evaluation interprets every
token against the container it meets: objects look up `key`
unconditionally; arrays require `Kind::Index`. Tokens that cannot be array
indices (leading zeros, overflow, non-digits) are valid pointers that fail
only when met by an array. The unreachable `ParseError` enumerators
(`ArrayIndexOverflow`, `NonDecimalArrayIndex`, `EmptyNonTerminalToken`) were
removed (ADR-004 makes the numerics non-API; removal is recorded as a
pre-1.0 breaking change).

`-` needs no new token kind: it parses as a Key token and is interpreted
container-relatively — object member `"-"` on objects; on arrays, the
position after the last element (writable by `add`/`set`, an
`IndexOutOfRange` error for reads, `replace`, and `remove`, per RFC 6902
§4.1).

### In-place writes with a validate-then-rebuild walk

`JSONPointer` gains `add`/`replace`/`remove` (RFC 6902 §4 primitive
semantics) plus a `set` upsert (`add` + optional
`WriteOptions::createIntermediates`). The methods mutate the passed
document/value in place — both downstream consumers hold a mutable working
document — and return `std::expected`, keeping the `evaluate()` error shape.

The engine (`detail::writePointer`) is an **iterative two-phase walk**:

1. *Descent/validate:* walk all non-terminal tokens, stacking each visited
   container (a COW handle — refcount bump only). Every failure is detected
   here, before any mutation.
2. *Leaf op + unwind:* apply the operation to a detached copy of the leaf
   container, then reassign the modified child into a detached copy of each
   stacked parent, bottom-up. Untouched siblings stay COW-shared.

Consequences of that shape:

- **Strong guarantee for free:** any error leaves the document bit-identical
  — this kills the "missed write-back silently drops edits" bug class that
  motivated the feature.
- **Atomicity for RFC 6902:** JSONPatch (ADR-008) inherits all-or-nothing
  application by working on a value copy.
- Iterative, not recursive: pointer depth is caller/fuzzer-controlled.
- Cost is O(Σ container sizes along the path) — one COW detach per level,
  the same work the hand-written ladders performed.

`createIntermediates` semantics (deliberately conservative): a created
intermediate's type is chosen by the *next* token (array for index `0` or
`-`, object otherwise); a JSON `null` counts as "nothing here yet" and is
replaced by a created container; an existing scalar in the path is never
overwritten; arrays are never null-padded (only the append position is
creatable). RFC 6902 `add` keeps its spec semantics — it never creates
intermediates; only `set` with the option does.

Error model: extend `json_pointer::EvalError` (`CannotRemoveRoot`,
`DocumentRootNotContainer`) instead of adding a write domain — write
failures *are* pointer-evaluation failures, `Error::detail` keeps carrying
the failing token index, and the seven-point `ErrorDomain` wiring in
`utils/JSONError.hpp` stays untouched. `QJsonDocument` cannot represent a
scalar root (a Qt limitation), so root-replacing writes that produce one
fail with `DocumentRootNotContainer` on the document overloads; the
`QJsonValue` overloads support them.

### No compile-time read/write gate

Write support is purely additive: no change to `Token`'s layout semantics
beyond always-populated keys (itself the compliance fix), no new members,
virtuals, or pimpl on `JSONPointer`, and the read walk is untouched. The
write engine lives in its own translation unit
(`JSONPointerWrite.cpp`), so a consumer that never calls a write method
never pulls the object file out of the static archive — zero code-size and
zero runtime cost for readers. The read benchmarks
(`BM_JSONPointer_{Simple,Nested,Array,Complex,Eval_*}`) were re-run against
a pre-change build to verify the deltas are noise (recorded in
`perf/performance_baseline.md`). Should evidence of a regression ever
appear, the minimal fallback is a CMake option excluding the write TU — not
added now.

## Consequences

- `/foo/5`, `/foo/01`, and `/-` resolve object members per spec; two former
  `invalid_pointer` compliance cases became evaluation-time behaviors.
- The write suite (`rfc6901-write-tests.json`) is seeded from RFC 6902
  Appendix A and asserts the strong guarantee and COW isolation on every
  case.
- Downstream: settings-backend engines map to `evaluate`/`set(create)`/
  `remove`; entity write-back ladders map to per-field `set`.
