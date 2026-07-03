# Production-Readiness Roadmap

Tracks the path from "pre-1.0, refactor freely" to production use, starting with
low-risk use cases in desktop applications. Based on the production-readiness
review of 2026-07-03.

**Adoption posture until v1.0:** consume as pinned source
(`add_subdirectory` / FetchContent at a fixed tag) or as an installed package
via `find_package(json_query)` (working since 0.3.0). API may still break
between minor versions. First production use cases: JSON Schema validation of
trusted app/config JSON, and JSONPointer/JSONPath extraction from app-internal
documents.

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

- [x] **Machine-readable known-failure list for compliance drivers.**
      *Resolved 2026-07-03:* `KnownFailures.hpp` next to the IETF driver holds
      exact (file, group, test) xfail triples in three buckets (no-SRELL
      regex, no-IDN, strict-ASCII hostname), selected by build features.
      Known mismatches → GTEST_SKIP; unknown mismatches → failure; stale
      entries that pass → failure. Suite is now green in base config
      (1932 passed / 62 skipped / 0 failed).
- [x] **ASan/UBSan CI leg.** *Resolved 2026-07-03:* new
      `JSON_QUERY_ENABLE_SANITIZERS` option (ASan+UBSan, independent of
      `ENABLE_FUZZ_TESTS`) and a `sanitize` CI job (Linux, Debug, full ctest
      with `detect_stack_use_after_return`).
- [x] **Recursive-descent limits removed; container-dedup RFC bug fixed.**
      *Resolved 2026-07-03:* the `kMaxStackDepth{100}`/`kMaxResults{10000}`
      caps were removed — the traversal is iterative (no call-stack risk) and
      output is O(document node count), so the caps only broke `$..` on nodes
      with >100 children. Removing them exposed a real RFC 9535 violation the
      caps had been masking: `deduplicateJsonValues` dropped equal-valued
      containers at distinct locations after recursive descent. Dedup removed
      (CTS still 443/443); scale + duplicate-preservation regression tests
      added in `JSONPathGTest.cpp`.
- [x] **Unresolved remote `$ref` policy + `$ref` cycle crash fixed.**
      *Resolved 2026-07-03:* verification found that `$ref` cycles consuming
      no instance input (e.g. `{"$ref": "#"}`) crashed validation with a
      stack overflow. Validation now tracks active (target, instance-location)
      ref expansions and reports `EvalError::RefCycleDetected` instead;
      productive recursion (tree schemas) is unaffected (IETF still
      1932/1994). Added `SchemaOptions::unresolvedRefPolicy`
      (`AcceptAll` default / `Fail` = create() fails with
      `ParseError::UnresolvedReference`). Note: the convenience `create()`
      overloads force `FormatValidation::Assertion` while `SchemaOptions{}`
      defaults to `Auto` — documented in the header; align at v1.0.
- [x] **Specify JSONPath result semantics in headers.** *Resolved 2026-07-03:*
      `evaluate`/`evaluateSingle` and JSONPointer methods carry full doc
      comments (nodelist semantics, squash rules and their ambiguity,
      missing-node behavior, thread-safety). Replacing the empty-array
      sentinel with a distinct signal remains a candidate API change for v1.0.
- [x] **Error-code stability policy.** *Resolved 2026-07-03 by ADR-004:*
      numeric error values are declared NOT part of the API (enumerators stay
      freely sortable); consumers must branch on enumerators, never persist
      `Error::numeric()` / `Error::code` / `toJson()` code fields. A stable
      wire format, if ever needed, will be an explicit symbolic mapping.
- [x] **Schema fuzz target + fuzz hygiene.** *Resolved 2026-07-03:*
      `fuzz_jsonschema` compiles fuzzed schemas (lenient + strict options) and
      validates fuzzed instances; schema seed corpus added; fuzz README fixed
      (removed two nonexistent targets, documented the new one); smoke-tested
      on macOS (11.9M execs/45s under ASan+UBSan, zero crashes — Homebrew
      LLVM 22 + llvm-clang toolchain now links fuzzers). Remaining: an
      on-demand CI fuzz job (Linux clang) — tracked in M3.

## M2 — Packaging & release engineering (before consuming as installed package)

- [x] **find_package-first dependency resolution (vcpkg support).**
      *Resolved 2026-07-03:* CTRE and function_ref declarations use
      `FIND_PACKAGE_ARGS` (vcpkg ports `ctre` / `tl-function-ref` win over the
      pinned FetchContent fallback); bridge targets forward to the imported
      targets; SRELL overridable via `JSON_QUERY_SRELL_INCLUDE_DIR`; consumer
      override mechanisms documented in README "Dependency resolution".
- [x] **SBOM generation.** *Resolved 2026-07-03:* `JSON_QUERY_ENABLE_SBOM`
      installs an SPDX 3.0.1 JSON-LD SBOM via CMake 4.3's native
      (experimental) `install(SBOM)`, derived from the `json_queryTargets`
      export set. Also fixed a duplicate-export bug (`json_query` was added
      to the export set twice). The experimental UUID is pinned for the
      CMake 4.3 series; revisit when the feature stabilizes.
- [x] **Working install/export.** *Resolved 2026-07-03 (0.3.0):* generated
      `json_queryConfig.cmake` (+version file, SameMinorVersion) with
      `find_dependency(Qt6 Core Network)` and conditional libidn2 handling;
      stale `cmake.config.in` deleted. `tests/consumer-smoke/` builds and
      runs against an installed prefix in the new `install-package` CI job.
- [x] **Fix installed-header dependencies.** *Resolved 2026-07-03 (0.3.0):*
      after the pimpl, no header reachable from the public umbrella includes
      any third-party header (verified by include-closure scan: 6 headers,
      zero third-party). CTRE/function_ref/SRELL are PRIVATE build deps,
      excluded from the export via `$<BUILD_INTERFACE:...>`.
- [x] **Shrink the public surface / ABI.** *Resolved 2026-07-03 (0.3.0):*
      `JSONPath`/`JSONPointer` are pimpl'd (`std::unique_ptr<const Impl>`);
      the umbrella's include closure shrank to 6 first-party headers.
      Benchmarked: evaluation unchanged (±0%), `create()` +7–9% (one
      allocation), copies clone the compiled state (pre-pimpl semantics).
- [x] **Symbol visibility / shared-lib story.** *Resolved 2026-07-03
      (0.3.0):* declared static-only officially (`add_library(json_query
      STATIC ...)`, documented in README); export-macro machinery deferred
      to v1.0 if shared builds are ever wanted.
- [x] **Versioning & releases.** *Resolved 2026-07-03:* version 0.3.0,
      `CHANGELOG.md` added, `v0.3.0` tag; version file enforces
      SameMinorVersion compatibility (pre-1.0 semantics). SOVERSION is moot
      (static-only). The AGENTS.md "refactor freely" note stands until v1.0.

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
- Set `FormatValidation` explicitly if you expect `format` to be enforced
  (2020-12 default is annotation-only).
- Keep `JSON_QUERY_FORMAT_IDN` off, or use the `ada` backend — libidn2 is
  LGPL-3.0 (relevant for statically-linked proprietary apps).
- Never persist or transmit `Error::numeric()` / `Error::code` values —
  they are not stable across library versions (ADR-004). Branch on the
  enumerators instead.
