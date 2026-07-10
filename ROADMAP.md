# Production-Readiness Roadmap

Tracks the path from "pre-1.0, refactor freely" to production use, starting with
low-risk use cases in desktop applications. Based on the production-readiness
review of 2026-07-03. **Status:** M0–M2 complete, M3 ongoing (as of 0.6.0);
remaining API-level work is consolidated in [Toward v1.0](#toward-v10-deferred-items-consolidated).

**Adoption posture until v1.0:** consume as pinned source
(`add_subdirectory` / FetchContent at a fixed tag) or as an installed package
via `find_package(json_query)` (working since 0.3.0). API may still break
between minor versions. First production use cases: JSON Schema validation of
trusted app/config JSON, JSONPointer/JSONPath extraction from app-internal
documents, and — since 0.6.0 — pointer-driven document *mutation*
(`JSONPointer::add/replace/remove/set`, `JSONPatch`, `merge_patch`; requested
by the first downstream consumer to replace hand-written write-back ladders
and a bespoke settings-backend pointer engine — see ADR-007/ADR-008).

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
      `JSON_QUERY_ENABLE_FUZZ_TESTS`) and a `sanitize` CI job (Linux, Debug, full ctest
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

- [x] Widen CI *(partially, 2026-07-03)*: LLVM clang leg on Linux; Debug is
      covered by the sanitize job; on-demand fuzz job added. A second Qt
      version was ruled out (only Qt 6.8 is relevant). Still open if ever
      wanted: code coverage reporting.
- [x] `ArrayPool`: *resolved 2026-07-03* — converted to lock-free
      `thread_local` pools; measured 4.6x faster 8-thread evaluation (the
      global mutex had produced negative scaling).
- [x] `noexcept`-on-allocating-paths: *resolved 2026-07-03 as policy, not
      defect* — the library never throws (errors via `std::expected`);
      `noexcept` documents that OOM is fatal (`std::terminate`), matching Qt.
      Policy documented on both `create()` factories; `JSONPath::create` is
      now also `noexcept` for consistency.
- [x] Dead code / header traps: *resolved 2026-07-03* — `ArenaAllocator`
      deleted (unreferenced), `to_sv()` lifetime warning strengthened (no
      longer reachable from the public umbrella since the pimpl).
- [x] Repo hygiene: *resolved 2026-07-03* — `docs/adr` merged into
      `doc/adr` (renumbered 006), stale `src/Plan.md` deleted earlier,
      `refactoring/` tooling kept (opt-in via `JSON_QUERY_BUILD_REFACTORING_TOOLS`),
      broken `memory_optimization_test` perf tool deleted.
- [x] Toolchain files: *resolved 2026-07-03* — `apple-clang.cmake` now uses
      the host architecture; the `llvm-clang.cmake` rpath workaround stays
      (it is what makes macOS fuzzing link, documented in `tests/README.md`)
      with the standing rule: never use these toolchains for distributables.
- [x] Windows developer experience: *resolved 2026-07-03* — `Init-DevEnv.ps1`
      bootstraps a build-ready MSVC shell (VS dev environment via vswhere,
      CMake PATH-order guard, Qt kit resolution honoring superbuild-provided
      `Qt6_DIR`/`CMAKE_PREFIX_PATH`); `scripts/init_qt_config.py` generates
      the git-ignored `qt.user.json`. Also fixed cl 19.51+ exceeding the
      default template-instantiation depth on the CTRE slice-selector pattern
      (`/templateDepth:2000` on `JSONPathParseUtils.cpp`, mirroring the Clang
      branch); verified 2687 tests green and superbuild consumption via
      `add_subdirectory` with parent-scope option overrides.
- [x] **Re-baseline performance documentation.** *Resolved 2026-07-03:*
      new baseline measured on Windows/MSVC (Release, 5-repetition medians,
      raw JSON archived in `perf/results/`); `performance_baseline.md` and
      `PERFORMANCE_ROADMAP.md` rewritten. Key finding: the historical
      "recursive descent 0.2x (faster than plain Qt)" figure was an artifact
      of the depth/result caps + dedup removed in M1 — the old benchmark
      measured a silently truncated traversal. Corrected cost: 11.5x
      (eval-only), making recursive+key token fusion the top perf priority
      (tracked in `perf/PERFORMANCE_ROADMAP.md` M3). Also added
      `perf/src/allocation_probe.cpp` (global operator-new counter):
      `JSONPointer::create` = 1 allocation (the token vector), `JSONPath::create`
      = 6, copies = 2 (filter tokens shared_ptr-shared; counts vary with
      the STL, see the baseline's per-platform table). macOS (arm64,
      AppleClang) re-measured the same day: overhead ratios are
      platform-dependent — eval-only ≈1x plain Qt on Windows vs 1.8–2.9x
      on macOS, `evaluateSingle` 0.3–0.4x vs ≈1.0x, recursive descent
      11.5x vs 20.0x (the top optimization target on both).

---

## Toward v1.0 (deferred items, consolidated)

Candidate API/infrastructure changes explicitly deferred while resolving
M0–M3, collected here so they are not lost in the resolved entries above:

- **JSONPath result signaling:** replace the empty-array sentinel from
  `evaluate()` with a distinct "no match" signal (see M1, result semantics).
- **Align `create()` format-validation defaults:** the convenience
  `JSONSchema::create()` overloads force `FormatValidation::Assertion` while
  `SchemaOptions{}` defaults to `Auto` (documented in the header; see M1).
- **Shared-library story:** export-macro machinery and symbol-visibility
  design if shared builds are ever wanted (currently static-only by design;
  see M2).
- **SBOM feature UUID:** revisit when CMake's `install(SBOM)` stabilizes
  (pinned to the CMake 4.3 series; see M2).
- **Code coverage reporting** in CI (optional; see M3).
- **Unify `ConvErrorCode` with `ConvertError`:** `utils/JSONValueUtils.hpp`
  carries a near-duplicate conversion enum (with its own message path and a
  `ConvError` struct holding expected/actual kinds) outside the unified
  `Error` system; folding them needs an API design pass.
- [x] **Collapse the three-way error-domain wiring.** *Resolved 2026-07-10
  (0.6.0):* `utils/JSONError.hpp` now derives everything from a single
  `JSON_QUERY_ERROR_DOMAIN_LIST` X-macro — `error_domain<E>` specializations,
  `is_domain_enum_v` (specialization-detection instead of a type chain), one
  constrained `Error(E, detail)` constructor replacing the nine explicit
  ones (overload resolution moved from explicit to implicit, matching the
  pre-existing implicit fallback), and both message-dispatch switches.
  Adding a module's error enum is now one list line plus the `ErrorDomain`
  enumerator.
- **Symbolic error codes in `ValidationError::toJson()`:** the `code` field
  is `Error::numeric()`, which ADR-004 declares unstable; migrate to (or add
  alongside) a symbolic string name if the JSON output is ever consumed
  across versions.
- **vcpkg port:** a real port/overlay recipe (the repo ships a consumer-side
  `vcpkg.json` manifest only).
- **API freeze:** on v1.0, breaking changes move to major versions only and
  the AGENTS.md "refactor freely" note is retired.

## Standing usage rules

- Pin this repo (and its deps) to exact commits or a release tag.
- Compiled `JSONPath`/`JSONPointer`/`JSONSchema` objects are immutable and
  safe to share across threads; `create()` is also safe to call concurrently
  (the former parser race was fixed in M0 — the old "evaluation only" rule
  is retired).
- Set `FormatValidation` explicitly if you expect `format` to be enforced
  (2020-12 default is annotation-only).
- The default build pulls only permissively licensed dependencies. The
  LGPL-3.0 libidn2 IDN backend requires an explicit
  `JSON_QUERY_IDN_BACKEND=libidn2` opt-in (the default `ada` backend is
  Apache-2.0/MIT); relevant for statically-linked proprietary apps.
- Never persist or transmit `Error::numeric()` / `Error::code` values —
  they are not stable across library versions (ADR-004). Branch on the
  enumerators instead.
