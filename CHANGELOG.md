# Changelog

Pre-1.0: minor versions may contain breaking changes (see `ROADMAP.md`).

## Unreleased (0.10.0)

### Added
- **`as_or<T>(fallback)` terminal adapter** (AC-3033 follow-up, report §5):
  ends a monadic chain with a plain `T`, falling back on *any* failure —
  evaluation error, missing value, or conversion failure:
  `"/data/name"_jptr.evaluate(doc) | as_or<QString>()`. The default
  fallback is a value-initialized `T`. Retires the consumers' local
  `StringAt`/`ArrayAt` one-liner helpers. Chosen over a
  `JSONPointer::value_or` member deliberately: it lives in the conversion
  layer next to `as<T>` (no new coupling into the pointer header) and
  terminates pointer, JSONPath, and plain-value chains alike.

## 0.9.0 — 2026-07-10

### Added
- **`_jpath` compile-once JSONPath literals** (AC-3033 follow-up, their
  report §5): `"$[?(@.type=='user')]"_jpath` — no `std::expected`
  unwrapping, each distinct literal compiled once (function-local static).
  Deliberately *compile-once, fail-fast* rather than compile-time-validated:
  full compile-time validation would require a second JSONPath grammar
  outside the real parser (the drift class ADR-007 forbids), so only the
  immutable root-identifier rule is a compile error; anything else in an
  invalid literal is a fatal error at first use. Retires the last
  consumer-side assert-and-unwrap helper (`CompiledPath`).

## 0.8.0 — 2026-07-10

### Added
- **Compile-time-validated pointer literals** (AC-3033 #4e, the last item
  of the ergonomics track): opt in with `using namespace
  json_query::literals;` and write `"/a/b"_jptr` — the literal's RFC 6901
  syntax is checked in a consteval context (invalid literal = compile
  error), so no `std::expected` unwrapping and no assert-and-unwrap
  helpers for known-good pointers. Each distinct literal compiles once
  (function-local static). char (UTF-8) and char16_t literal forms.

### Changed
- `parsePointer` now runs the complete syntax check up front through the
  same `validatePointerSyntax` the literals use (single source of truth —
  the rule that prevented the parseDot/parseBare guard divergence), which
  also simplified its tokenization loop.

## 0.7.0 — 2026-07-10

The 0.7.0 milestone is the AC-3033 first-consumer ergonomics track
(API surface over unchanged spec semantics).

### Added
- **Typed-root write overloads**: `JSONPointer::add/replace/remove/set` and
  `JSONPatch::applyInPlace` for `QJsonObject&`/`QJsonArray&` — no more
  QJsonValue round-trip for typed roots. New
  `EvalError::RootTypeMismatch` when a root-replacing write would change
  the fixed container kind (strong guarantee holds).
- **Pointer composition**: `JSONPointer::appended(key|index)` and
  `operator/` build compiled pointers programmatically (no re-parsing;
  keys enter as data and re-encode canonically — no escaping at call
  sites, no injection surface). Classification shares `parseArrayIndex`
  with the parser, so composed and parsed pointers behave identically.
- **`JSONPatchBuilder`** (fluent, both compiled-pointer and string
  overloads; `build()` validates via `JSONPatch::create`) and
  **`JSONPatch::toJson()`** for RFC 6902 wire-format round-trips.
- **`as<qint64>`**: exact for integer-backed values across the full
  qint64 range (no double round-trip); integral doubles convert,
  fractional/overflow report `NumericNotIntegral`/`NumericOutOfRange`.

### Changed
- **ADR-001 is now machine-enforced**: a pre-commit pygrep hook rejects
  brace-initializing a QJsonArray from an array-producing expression (and
  any Qt JSON container from a bare same-type variable) — the pattern
  copies on MSVC but wraps on GCC/clang, so it passes local Windows
  builds and fails only on CI. The handful of benign `auto x{...toObject()}`
  sites the sweep touched were converted to copy-init.
- **Conversion errors unified** (tracked ROADMAP item): the parallel
  `ConvErrorCode`/`ConvError` system in `JSONValueUtils.hpp` is gone;
  `as<T>` produces unified `Error`s (Convert domain) with the
  expected/actual `JsonKind` pair packed into `Error::detail` and
  rendered by `formatted_message()` ("... (expected array, got
  string)"). `JsonKind`/`kind_name` moved to `utils/JSONError.hpp`.

## 0.6.0 — 2026-07-10

### Added
- **Writable JSON Pointers.** `JSONPointer` gains `add()`, `replace()`,
  `remove()` (RFC 6902 §4 primitive semantics: `-`/index==size append,
  replace/remove require the target, remove returns the removed value) and a
  `set()` upsert with `WriteOptions::createIntermediates` (creates missing
  containers — type chosen by the next token, JSON null counts as empty,
  scalars are never overwritten, arrays never null-padded). All write methods
  give the strong guarantee: on any error the document is untouched. The
  engine is a validate-then-rebuild walk over Qt's COW containers in its own
  translation unit, so non-writing consumers link zero write code
  (ADR-007). New `EvalError::CannotRemoveRoot` and
  `EvalError::DocumentRootNotContainer`. New data-driven write compliance
  suite (seeded from RFC 6902 Appendix A), unit tests, libFuzzer target, and
  benchmarks.
- **RFC 6902 JSON Patch module** (`json-patch/`, `json_query::JSONPatch`):
  eagerly validated `create(QJsonArray|QJsonDocument)`, functional + atomic
  `apply(QJsonDocument|QJsonValue)` (all-or-nothing per RFC 6902 §5), and an
  `applyInPlace()` convenience. `test` compares numbers numerically per
  §4.6. Errors carry the failing operation index in `Error::detail`
  (ADR-008); new `ErrorDomain::PatchParse`/`PatchEval`. Validated against
  the community json-patch-tests suite (new pinned submodule
  `compliance/json-patch-tests`): all 108 enabled cases pass, KnownFailures
  table empty.
- **RFC 7386 JSON Merge Patch**: `json_query::json_patch::merge_patch()`
  free function (total, no error path) with a `QJsonDocument` overload;
  data-driven test suite containing the complete RFC 7386 Appendix A table.

### Fixed
- **RFC 6901 container-relative token semantics.** Tokens are now
  interpreted relative to the container they meet at evaluation time
  (RFC 6901 §4): numeric tokens (`/foo/5`) resolve object members, and
  leading-zero/overflowing-digit tokens are valid pointers that only fail
  against arrays (previously rejected at parse time). Escape-sequence
  validation now runs for every token.

### Removed
- `json_pointer::ParseError::ArrayIndexOverflow`, `::NonDecimalArrayIndex`,
  and `::EmptyNonTerminalToken` — unreachable after the container-relative
  fix (the latter two were already unused). Pre-1.0 breaking change; the
  numeric values were never API (ADR-004).

### Changed (internal, no API impact)
- Post-review consolidation: single `utils/detail/DocumentRoot.hpp` for the
  QJsonDocument root unwrap/commit used by all document overloads; RFC 6902
  test-op equality exposed as `json_patch::detail::jsonDeepEquals` (the
  compliance driver now uses it too); JSONPath's shorthand-name scan unified
  between `parseDot`/`parseBare`; shared compliance-suite test plumbing.
- `utils/JSONError.hpp`: the error-domain wiring (trait, concept, nine
  explicit constructors, two dispatch switches) is now generated from one
  `JSON_QUERY_ERROR_DOMAIN_LIST` table. The per-enum `Error` constructors
  are subsumed by one constrained constructor, which is implicit like the
  previous fallback (was: explicit for the nine listed enums; call sites
  needing no change were verified by the full suite).

### Changed (CMake integration)
- **Option renames** (no compatibility shims, pre-1.0):
  `BUILD_BENCHMARKING` → `JSON_QUERY_BUILD_BENCHMARKS`,
  `ENABLE_FUZZ_TESTS` → `JSON_QUERY_ENABLE_FUZZ_TESTS`,
  `WITH_REFACTORING_TOOLS` → `JSON_QUERY_BUILD_REFACTORING_TOOLS`. Every
  user-facing cache variable now carries the `JSON_QUERY_` prefix, so a
  superbuild cannot collide with them. `BUILD_TESTING` keeps its standard
  CTest name and is honored for top-level builds via the new
  `JSON_QUERY_BUILD_TESTS` gate.
- **Superbuild/`add_subdirectory` hardening:** `include(CTest)` and the
  examples/perf/tests/install defaults now key off `PROJECT_IS_TOP_LEVEL` —
  embedding projects get only the library target (each piece can be re-enabled
  explicitly, e.g. `-DJSON_QUERY_BUILD_TESTS=ON`). New
  `json_query::json_query` ALIAS so embedded consumers link the same
  namespaced name the installed package exports (all in-tree consumers and
  a new CI embed smoke test use it). Fuzz-test paths are superbuild-safe
  (`PROJECT_SOURCE_DIR`/`PROJECT_BINARY_DIR`). `JSON_QUERY_ENABLE_INSTALL`
  is now a declared option (default: top-level only).
- **Install layout:** CMake package files moved from
  `share/json_query/cmake/` to `lib/cmake/json_query/` (the conventional
  location for an arch-dependent static library; override with
  `JSON_QUERY_INSTALL_CMAKEDIR`).
- **Package-manager friendliness:** the GoogleTest and Google Benchmark
  fetches now use `FIND_PACKAGE_ARGS` (vcpkg/system copies win over the
  pinned FetchContent fallback), matching the existing CTRE/function_ref
  behavior. New `vcpkg.json` manifest (`ctre`, `tl-function-ref`; `tests` and
  `benchmarks` features) plus a CI job that builds and tests with
  vcpkg-resolved dependencies.

### Changed (library)
- Error enums are now grouped by category with banner comments, alphabetical
  within each group (`json_schema::ParseError`/`EvalError`, `ErrorDomain`,
  `ConvertError`, and `json_path::EvalError` reordered). Numeric error-code
  values shift accordingly; per ADR-004 those values are not API. ADR-004's
  maintenance policy updated to "grouped by category, alphabetical within
  group".

## 0.5.0 — 2026-07-03

### Changed
- The IDN backend (`JSON_QUERY_IDN_BACKEND`) now defaults to `ada`
  (ada-url/idna, Apache-2.0/MIT, fetched) instead of `libidn2` (LGPL-3.0,
  system). The library is static-only, so the default build must not impose
  LGPL relink obligations on consumer binaries; libidn2 remains available as
  an explicit opt-in for its fuller RFC 5892 coverage. A new
  `kKnownFailuresIdnAda` xfail bucket covers the 14 CONTEXTO-rule suite cases
  UTS #46 cannot enforce (full suite stays green on both backends), and
  ada-idna's leaked `INTERFACE /WX /W3 /sdl` MSVC options are stripped so the
  dependency cannot force warnings-as-errors onto consumers.

- `ArrayPool` is now a lock-free `thread_local` pool (was a mutex-guarded
  global singleton with negative scaling — measured 4.6× faster 8-thread
  concurrent evaluation).
- `JSONPointer` no longer uses a pimpl: its internals are first-party, so the
  inline `std::vector<Token>` member restores the allocation-free-of-overhead
  `create()` while the umbrella stays free of third-party headers.
  `JSONPath` keeps its pimpl (the CTRE firewall).
- Exception policy documented on both `create()` factories: the library never
  throws (errors via `std::expected`); `noexcept` means OOM is fatal.
  `JSONPath::create` is now also `noexcept` for consistency.

### Removed
- Dead `ArenaAllocator` (unreferenced) and the broken
  `memory_optimization_test` perf tool (referenced removed APIs).

### Fixed
- Warning hygiene (all platforms now build warning-free): five
  signed/unsigned comparisons (`qsizetype` vs `std::vector::size()`, MSVC
  C4018 at /W3) now use `std::ssize`; the schema-registry test asserts every
  `add()`/`create()` result instead of discarding `[[nodiscard]]` values
  (MSVC C4834); the `alignas(32)` `ContainerCursor` is passed by reference
  to silence gcc's `-Wpsabi` note; GoogleTest's own build suppresses
  clang 21's `-Wcharacter-conversion` alongside the existing `-Wundef`,
  probed via `check_cxx_compiler_flag` so AppleClang does not raise
  `-Wunknown-warning-option`.

### Infrastructure
- CI is green again: the Linux legs now install the Qt `icu` archive (the
  Linux Qt binaries link the bundled ICU, which broke every Linux link), the
  clang leg uses clang 20 (clang < 19 cannot use libstdc++'s C++23
  `<expected>`), the Test Report workflow downloads the artifact names ci
  actually uploads, and the benchmark steps run the binary from its real
  location so benchmark artifacts are no longer silently empty.
- Actions bumped off deprecated Node 20 runtimes: checkout v7,
  upload-artifact v7, setup-python v6, gha-setup-ninja v6, dorny/test-reporter
  v3.
- CI: LLVM clang leg on Linux; on-demand (`workflow_dispatch`) fuzz job
  running all four LibFuzzer targets. `docs/adr` merged into `doc/adr`
  (thread-local cache ADR renumbered 006); `apple-clang.cmake` no longer
  hardcodes x86_64.
- Documentation refreshed for publication: README test-status table reflects
  the green xfail state (2,624 passing / 0 failing), stated Qt requirement is
  now 6.8+, AGENTS.md aligned with the tagged-release reality (including the
  ADR-001 brace-init exception), historical compliance/perf docs
  banner-marked, disclaimer reworded to the pre-1.0 API policy.

## 0.4.0 — 2026-07-03

### Changed
- **Versioned inline ABI namespace** (ADR-005, the nlohmann_json/fmt
  pattern): all symbols now live in `json_query::v0_4::...` via
  `namespace json_query::inline JSON_QUERY_ABI_NS`. Source-compatible
  (inline namespaces are transparent — `json_query::JSONPath` keeps
  working), but ABI-incompatible with 0.3.x: different embedded json_query
  versions can now coexist in one process without ODR violations or
  dynamic-linker interposition.
- The static library is built with hidden visibility
  (`CXX_VISIBILITY_PRESET hidden`, `VISIBILITY_INLINES_HIDDEN`) so shared
  libraries embedding it do not re-export its symbols.

## 0.3.0 — 2026-07-03

First tagged release, following the production-readiness hardening pass.

### Fixed
- **RFC 9535 violation:** recursive descent (`$..`) no longer drops
  equal-valued containers at distinct locations (removed value-based dedup);
  also removed the arbitrary `$..` limits that failed on nodes with more
  than 100 children (`TooComplex`).
- **Crash:** unproductive `$ref` cycles (e.g. `{"$ref": "#"}`) no longer
  stack-overflow validation; they report `EvalError::RefCycleDetected`.
- **Data race:** concurrent `JSONPath::create()` is now safe (atomic
  bracket-group counter).
- Removed leftover uncategorized `qDebug()` output that fired from consumer
  translation units.
- `install(EXPORT)` no longer adds `json_query` to the export set twice.

### Added
- Working `find_package(json_query)` flow: generated package config +
  version file, consumer smoke test in CI (`tests/consumer-smoke/`).
- `SchemaOptions::unresolvedRefPolicy` (`AcceptAll` default / `Fail`).
- SPDX 3.0.1 SBOM generation via CMake ≥ 4.3 `install(SBOM)`
  (`JSON_QUERY_ENABLE_SBOM`).
- `JSON_QUERY_ENABLE_SANITIZERS` option and an ASan+UBSan CI job.
- `fuzz_jsonschema` LibFuzzer target and schema seed corpus.
- Machine-readable known-failure (xfail) table for the IETF compliance
  driver — the suite is green in base config (regressions are visible).
- `find_package`-first dependency resolution (vcpkg `ctre` /
  `tl-function-ref` ports win over pinned FetchContent fallbacks).

### Changed
- **License:** dual `Apache-2.0 WITH LLVM-exception OR MIT` (previously
  Apache-only).
- **ABI/API surface:** `JSONPath` and `JSONPointer` are pimpl'd
  (`std::unique_ptr`); public headers no longer expose tokens/filters or any
  third-party headers (CTRE/function_ref are now private build
  dependencies). Copies clone the compiled state; evaluation performance is
  unchanged, `create()` pays one extra allocation (≈7–9%).
- The library is explicitly **static-only** (no symbol-visibility story yet).
- Dependencies pinned: CTRE (commit, v3.10.0+), function_ref `v1.0.0`,
  SRELL SHA256.
- Error-code numeric values declared not part of the API (ADR-004).
- Documented JSONPath/JSONPointer evaluation semantics (nodelist rules,
  `evaluateSingle` squash ambiguity, thread-safety guarantees).

## 0.2 — 2026-02/03 (untagged)

Initial development snapshots: RFC 6901 / RFC 9535 / JSON Schema 2020-12
implementation against the official compliance suites.
