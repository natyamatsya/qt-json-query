# Production-Readiness Roadmap

Tracks the path from "pre-1.0, refactor freely" to production use, starting with
low-risk use cases in desktop applications. Based on the production-readiness
review of 2026-07-03.

**Adoption posture until v1.0:** consume as pinned source only
(`add_subdirectory` / FetchContent at a fixed commit). `find_package` install
flow is not usable yet (see M2). First production use cases: JSON Schema
validation of trusted app/config JSON, and JSONPointer/JSONPath extraction from
app-internal documents.

---

## M0 — Blockers (before first production use)

- [x] **Root-cause the ASan suppression on JSON Pointer evaluation.**
      *Resolved 2026-07-03:* the suppression attribute was already absent from
      the code; full test corpus (2,665 tests) passes under ASan+UBSan with
      zero reports and zero functional failures (AppleClang, Qt 6.8.3). The
      original "wrapped array" symptom was identified as the `QJsonArray`
      brace-init footgun (ADR-001), reproducible without any sanitizer — the
      ASan correlation was coincidental. Removed the unused
      `SanitizerCompat.hpp` macro and stale comments; ADR-003 marked
      Superseded with the verification record and root-cause analysis.
- [x] **Remove leftover raw `qDebug()` from public header hot path.**
      *Resolved 2026-07-03.*
      `include/json-query/json-path/JSONPathCompile.hpp` (`embedFilter` /
      `evaluateEmbeddedFilter`) prints uncategorized `"DEBUG: ..."` output.
      Inline in a public header, so it fires from consumer TUs
      (`QT_NO_DEBUG_OUTPUT` is PRIVATE to the library target) and cannot be
      silenced via the `json_query.json_path` logging category. Also remove the
      `qDebug()` spam in `tests/rfc-compliance-suite/rfc-9535/RFC9535CTSComplianceGTest.cpp`.
- [x] **Pin all FetchContent dependencies.**
      *Resolved 2026-07-03: CTRE pinned to commit `6225211` (v3.10.0+2),
      function_ref to `v1.0.0`, SRELL download hash-pinned (SHA256).*
      CTRE is fetched at `GIT_TAG main` and function_ref at `master` (root
      `CMakeLists.txt`); README claims CTRE v3.9.0 — mismatch. CTRE is a
      *public* compile-time dependency baked into headers. Pin exact release
      tags/commits; add `URL_HASH` to the optional SRELL download.
- [x] **Fix data race in `JSONPath::create()`.**
      *Resolved 2026-07-03: both bracket-group ID counters are now
      `std::atomic<int>` with relaxed increments.*
      Plain `static int nextBracketGroupId` incremented during parsing
      (`src/json-query/json-path/JSONPathParsers.cpp`). Concurrent compilation
      is UB and can hand out colliding bracket-group IDs. Make it atomic or
      per-parse state, and document compile-side thread-safety.

## M1 — Correctness & robustness (shortly after first adoption)

- [ ] **Machine-readable known-failure list for compliance drivers.**
      The ~62 base-config IETF schema failures (optional regex/IDN features)
      are hard `EXPECT` failures with no xfail table — CI cannot distinguish a
      new regression from the documented backlog. Add an expected-failure
      table keyed by test name; green CI must mean "no regressions".
- [ ] **ASan/UBSan CI leg.** Sanitizers currently exist only bundled behind
      `ENABLE_FUZZ_TESTS`; no sanitizer runs in CI. Add a standalone
      ASan+UBSan option and a CI job (at least Linux, Debug).
- [ ] **Document and revisit recursive-descent limits.**
      `IterativeRecursiveDescent.hpp`: `kMaxStackDepth{100}` bounds the *work
      stack*, not nesting depth — any node with >100 children makes `$..`
      return `TooComplex`; `kMaxResults{10000}` caps totals. Failing loudly is
      right, but the limits are too low for real data and undocumented. Track
      true depth, raise/make configurable, document.
- [ ] **Unresolved remote `$ref` policy.** Currently validates as
      "accept all" silently (`JSONSchemaValidate.cpp`). Add an option (or
      default) to fail compilation/validation on unresolved refs; document.
      Verify cycle behavior for `$ref` chains that don't consume instance depth.
- [ ] **Specify JSONPath result semantics in headers.**
      `evaluate`/`evaluateSingle` have no doc comments; `evaluateSingle`
      returns an empty-array `QJsonValue` sentinel for missing definite nodes
      (`JSONPath.cpp`). Define and document found-null vs. not-found; consider
      returning an error or `std::nullopt`-like signal instead of a sentinel.
- [x] **Error-code stability policy.** *Resolved 2026-07-03 by ADR-004:*
      numeric error values are declared NOT part of the API (enumerators stay
      freely sortable); consumers must branch on enumerators, never persist
      `Error::numeric()` / `Error::code` / `toJson()` code fields. A stable
      wire format, if ever needed, will be an explicit symbolic mapping.
- [ ] **Schema fuzz target + fuzz hygiene.** No fuzzer covers the schema
      compiler/validator. Add one; fix `tests/fuzz/README.md` (documents two
      nonexistent targets); commit a seed corpus; add an on-demand CI fuzz job.

## M2 — Packaging & release engineering (before consuming as installed package)

- [x] **find_package-first dependency resolution (vcpkg support).**
      *Resolved 2026-07-03:* CTRE and function_ref declarations use
      `FIND_PACKAGE_ARGS` (vcpkg ports `ctre` / `tl-function-ref` win over the
      pinned FetchContent fallback); bridge targets forward to the imported
      targets; SRELL overridable via `JSON_QUERY_SRELL_INCLUDE_DIR`; consumer
      override mechanisms documented in README "Dependency resolution".
- [ ] **Working install/export.** Wire `configure_package_config_file()` +
      `write_basic_package_version_file()` (`cmake/cmake.config.in` is
      currently orphaned and stale — wrong targets file name, missing
      `Qt6::Network` dependency). CI must exercise
      `JSON_QUERY_ENABLE_INSTALL=ON` + a `find_package` consumer smoke test.
- [ ] **Fix installed-header dependencies.** Public headers include
      `<ctre.hpp>` / `<tl/function_ref.hpp>` but the deps are BUILD_INTERFACE
      only and never installed — installed headers cannot compile. Either
      remove third-party includes from public headers (preferred; CTRE usage
      in `JSONQueryUtils.hpp` / `JSONPathFilterComparison.hpp`) or install and
      export the dependencies properly.
- [ ] **Shrink the public surface / ABI.** `JSONPath`/`JSONPointer` expose
      concrete members and `Token`/filter internals via `JSONPathCompile.hpp`
      in public headers (unlike the pimpl'd `JSONSchema`). Move internals out
      of the public include tree; consider pimpl for JSONPath.
- [ ] **Symbol visibility / shared-lib story.** No export macros,
      no `CMAKE_CXX_VISIBILITY_PRESET`; shared builds are broken on Windows.
      Either declare static-only officially or add `generate_export_header`.
- [ ] **Versioning & releases.** Tag releases, add CHANGELOG, SOVERSION,
      and an API-stability statement (supersede the AGENTS.md "refactor
      freely" note when v1.0 is cut).

## M3 — Hardening & polish (ongoing)

- [ ] Widen CI: LLVM-clang leg, a second Qt version, Debug builds, coverage.
- [ ] `ArrayPool` global mutex serializes concurrent evaluation — measure and
      consider per-thread pools.
- [ ] `JSONPointer::create`/`evaluate` are `noexcept` but allocate
      (terminate on OOM) — drop `noexcept` or make allocation-free.
- [ ] Remove dead throwing code (`ArenaAllocator` is unreferenced) and the
      `utils::to_sv()` thread-local dangling-view trap from public headers.
- [ ] Repo hygiene: merge `doc/` and `docs/` (ADRs split across both), delete
      stale `src/Plan.md`, decide fate of `refactoring/` tooling.
- [ ] Toolchain files: remove hardcoded Homebrew rpath workaround
      (`llvm-clang.cmake`) once libc++ mismatch is resolved; fix
      `apple-clang.cmake` hardcoded `x86_64`; never use these for
      distributable builds.

---

## Usage rules for early adopters (until M1 lands)

- Pin this repo (and its deps) to exact commits; clone `--recursive`.
- Share compiled `JSONPath`/`JSONPointer`/`JSONSchema` objects across threads
  for *evaluation* only; do not call `create()` concurrently (see M0 race).
- Avoid `$..` recursive descent on documents where any node may exceed ~100
  children or results exceed 10,000 (returns `TooComplex`).
- Set `FormatValidation` explicitly if you expect `format` to be enforced
  (2020-12 default is annotation-only).
- Keep `JSON_QUERY_FORMAT_IDN` off, or use the `ada` backend — libidn2 is
  LGPL-3.0 (relevant for statically-linked proprietary apps).
- Never persist or transmit `Error::numeric()` / `Error::code` values —
  they are not stable across library versions (ADR-004). Branch on the
  enumerators instead.
