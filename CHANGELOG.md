# Changelog

Pre-1.0: minor versions may contain breaking changes (see `ROADMAP.md`).

## Unreleased

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
