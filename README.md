# qt-json-query

> **Disclaimer:** This project is agentically engineered — an experiment in guiding agentic
> development with compliance acceptance test suites (the engineering record lives in
> [ROADMAP.md](ROADMAP.md), [CHANGELOG.md](CHANGELOG.md), and [doc/adr/](doc/adr/)). The
> covered JSON specs seem useful to have in a Qt context. **Pre-1.0:** APIs can change
> between minor versions until v1.0 — breaking changes are recorded in CHANGELOG.md, and
> the versioned ABI namespace (doc/adr/005) keeps differently-versioned binaries from
> colliding.

A modern C++23 library providing [JSON Pointer (RFC 6901)](https://www.rfc-editor.org/rfc/rfc6901) with write support, [JSON Patch (RFC 6902)](https://www.rfc-editor.org/rfc/rfc6902), [JSON Merge Patch (RFC 7386)](https://www.rfc-editor.org/rfc/rfc7386), [JSONPath (RFC 9535)](https://www.rfc-editor.org/rfc/rfc9535), and [JSON Schema (Draft 2020-12)](https://json-schema.org/draft/2020-12/json-schema-core) for Qt.

- Full **JSON Pointer (RFC 6901)** compliance (83/83 tests) — read **and write**
  (`add`/`replace`/`remove`/`set`, strong error guarantee)
- Full **JSON Patch (RFC 6902)** compliance (community json-patch-tests suite,
  108/108 enabled cases) — atomic multi-operation patches
- **JSON Merge Patch (RFC 7386)** — complete Appendix A table
- Full **JSONPath (RFC 9535)** compliance (443/444 tests, 1 skipped)
- **JSON Schema (Draft 2020-12)** validation — 1932/1994 IETF tests (96.9% base config; the rest are tracked optional-feature gaps)
- Unified **`Error`** type with `std::expected` across all modules
- Compile-time regular expressions (**CTRE**) for fast parsing
- Clean integration with Qt JSON types (`QJsonValue`, `QJsonDocument`, etc.)

## Requirements

| Dependency | Version |
|---|---|
| **C++23 compiler** | Clang 16+, GCC 13+, or MSVC 17.8+ |
| **Qt** | 6.8+ |
| **CTRE** | v3.10.0+2 (pinned commit, fetched automatically via CMake FetchContent) |
| **GoogleTest** | v1.14.0 (fetched automatically for tests) |
| **Google Benchmark** | v1.8.3 (fetched automatically for benchmarks, opt-in) |

### Optional Dependencies

| Dependency | CMake Option | Purpose |
|---|---|---|
| **SRELL** | `JSON_QUERY_FORMAT_ECMA_REGEX=ON` | ECMA-262 regex for `pattern` + `regex` format |
| **ada-url/idna** | `JSON_QUERY_FORMAT_IDN=ON` | IDN hostname/email validation (UTS #46, Apache-2.0/MIT, fetched) |
| **libidn2** | `JSON_QUERY_FORMAT_IDN=ON` + `JSON_QUERY_IDN_BACKEND=libidn2` | Fuller IDNA 2008 / RFC 5892 coverage — **LGPL-3.0**, explicit opt-in |

All default dependencies are permissively licensed. Because the library is
static-only, an LGPL dependency would impose relink obligations on every
consumer binary, so the LGPL-3.0 libidn2 backend is never selected by
default — consumers must opt in via `JSON_QUERY_IDN_BACKEND=libidn2`.

## Building

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH=/path/to/qt/6.x/platform
cmake --build build
```

### Windows (MSVC) quick start

Dot-source the init script in PowerShell to get a build-ready shell (imports
the Visual Studio developer environment, resolves a Qt MSVC kit, and exports
`CMAKE_PREFIX_PATH`):

```pwsh
. .\Init-DevEnv.ps1
cmake --preset debug-msvc
cmake --build --preset debug-msvc
ctest --test-dir build-debug-msvc
```

Qt is resolved from (in order): the `-QtDir` parameter, an already-configured
environment (`Qt6_DIR` / `CMAKE_PREFIX_PATH` — e.g. set by a superbuild that
consumes this repo — is respected, never overridden), the git-ignored
`qt.user.json`, or auto-discovery. To generate `qt.user.json` from the Qt
installations found on your machine:

```pwsh
python scripts/init_qt_config.py         # see --print / --force
```

Run tests:

```bash
ctest --test-dir build                                     # all tests
cmake --build build --target rfc9535_compliance_tests      # specific suite
```

See [`tests/README.md`](tests/README.md) for the full list of test targets and CMake options.

### Benchmarks and performance tools

Google-Benchmark microbenchmarks are opt-in (`JSON_QUERY_BUILD_BENCHMARKS`, default
`OFF`) and should be run on a Release build:

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release -DJSON_QUERY_BUILD_BENCHMARKS=ON ...
cmake --build build --target json_benchmark
./build/json_benchmark
```

CI builds and runs the benchmarks on every platform leg (best-effort) and
uploads the JSON results as per-platform artifacts.

`perf/` additionally contains profiling/allocation-analysis tools
(`JSON_QUERY_BUILD_PERF`, default `ON` for top-level builds, `OFF` when
embedded) and the measurement record:
`perf/performance_baseline.md` (re-baselined 2026-07-03 on Windows/MSVC and
macOS/AppleClang, raw JSON archived in `perf/results/`) and
`perf/PERFORMANCE_ROADMAP.md` (current optimization priorities). Highlights:
pre-compiled evaluation runs at ≈1–3x plain-Qt traversal speed depending on
platform (`evaluateSingle`: at or below a hand-written traversal); recursive
descent (`$..`) is the known optimization target.

## Integration

As a subdirectory (FetchContent or git submodule):

```cmake
add_subdirectory(path/to/qt-json-query)
target_link_libraries(your_target PRIVATE json_query::json_query)
```

When embedded this way, only the library target is configured — examples,
perf tools, tests, and install/export rules all default to OFF (each can be
re-enabled, e.g. `-DJSON_QUERY_BUILD_TESTS=ON`).

Or installed, via `find_package`:

```cmake
find_package(json_query 0.7 REQUIRED)
target_link_libraries(your_target PRIVATE json_query::json_query)
```

(install with `-DJSON_QUERY_ENABLE_INSTALL=ON` + `cmake --install`; see
`tests/consumer-smoke/` for a minimal consumer). The library is built as a
static library only — shared builds are unsupported until a symbol-visibility
story exists (planned for v1.0).

Safe to embed in multiple libraries within one application: all symbols live
in a versioned inline ABI namespace (`json_query::v0_7::…`, transparent to
source code — you still write `json_query::JSONPath`) and the archive is
built with hidden visibility, so different embedded versions cannot collide
or interpose each other (see `doc/adr/005`).

### Dependency resolution (vcpkg-friendly)

Internal dependencies are resolved with `find_package` first and fetched via
FetchContent only as a fallback, so package managers control the versions:

| Dependency | `find_package` name | vcpkg port | FetchContent fallback |
|---|---|---|---|
| **CTRE** | `ctre` | `ctre` | pinned commit `6225211` (v3.10.0+2) |
| **function_ref** | `tl-function-ref` | `tl-function-ref` | `v1.0.0` |
| **SRELL** (optional) | — | — (set `JSON_QUERY_SRELL_INCLUDE_DIR`) | `srell4_140.zip` (SHA256-pinned) |

Override mechanisms, in order of preference:

- **vcpkg / system package:** just make the package findable (vcpkg toolchain
  file, `CMAKE_PREFIX_PATH`); it wins over the fetch automatically.
- **Different source version:** call `FetchContent_Declare(ctre ...)` (or
  `function_ref`) with your own tag *before* `add_subdirectory` of this
  project — the first declaration wins.
- **Local checkout:** set `FETCHCONTENT_SOURCE_DIR_CTRE` /
  `FETCHCONTENT_SOURCE_DIR_FUNCTION_REF` to a source directory.
- **Force the pinned fetch** (ignore installed packages): configure with
  `-DFETCHCONTENT_TRY_FIND_PACKAGE_MODE=NEVER`.

The same applies to the test/benchmark dependencies (GoogleTest via
`find_package(GTest)`, Google Benchmark via `find_package(benchmark)`).
A `vcpkg.json` manifest is provided for vcpkg users: `ctre` and
`tl-function-ref` are direct dependencies, GoogleTest/Benchmark sit behind
the `tests`/`benchmarks` features (`-DVCPKG_MANIFEST_FEATURES=tests`).
Qt itself is deliberately **not** in the manifest — supply it via
`CMAKE_PREFIX_PATH` (vcpkg's qtbase is a heavyweight build). CI exercises
this path (`vcpkg-deps` job), the installed-package path
(`install-package`), and the `add_subdirectory` embedding path
(`tests/embed-smoke`).

### SBOM generation (CMake ≥ 4.3)

The build can install an SPDX 3.0.1 (JSON-LD) Software Bill of Materials
generated by CMake's native — currently experimental — `install(SBOM)`:

```bash
cmake -B build -DJSON_QUERY_ENABLE_INSTALL=ON -DJSON_QUERY_ENABLE_SBOM=ON ...
cmake --build build
cmake --install build --prefix <prefix>
# → <prefix>/lib/sbom/json_query/json_query.spdx.json
```

The SBOM is derived from the `json_queryTargets` export set and records the
library, its header-only interface targets, and external packages resolved
via `find_package` (Qt, and CTRE/function_ref when provided by vcpkg) with
their versions. Requires CMake 4.3+; the experimental feature UUID pinned in
`CMakeLists.txt` matches the CMake 4.3 series and must be bumped when the
feature changes or stabilizes.

## Usage

Include the umbrella header:

```cpp
#include "json-query/JSONQuery"
using namespace json_query;
```

This provides `JSONPointer`, `JSONPatch`, `JSONPath`, and `JSONSchema` in the `json_query` namespace.

### JSON Pointer

```cpp
#include "json-query/JSONQuery"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

using namespace json_query;

const QJsonObject obj{{"foo", QJsonObject{{"bar", 42}, {"baz", "hello"}}}};
const QJsonDocument doc{obj};

// Create a pointer (returns std::expected<JSONPointer, Error>)
auto pointer{JSONPointer::create("/foo/bar")};
if (!pointer)
{
    qWarning() << "Parse error:" << pointer.error().formatted_message();
    return;
}

// Evaluate against a document (returns std::expected<QJsonValue, Error>)
auto result{pointer->evaluate(doc)};
if (result)
    qDebug() << "Result:" << *result; // QJsonValue(double, 42)
else
    qWarning() << "Eval error:" << result.error().formatted_message();
```

### Writing with JSON Pointers

All write methods mutate the passed document in place and give the **strong
guarantee**: on any error the document is left untouched (no half-applied
writes). `add`/`replace`/`remove` follow RFC 6902 §4 operation semantics;
`set` is an upsert that can optionally create missing intermediate containers
(the settings-backend shape).

```cpp
#include "json-query/JSONQuery"
#include <QJsonDocument>

using namespace json_query;

QJsonDocument doc; // may start empty

// Upsert a deep value, creating {"ui":{"theme":{...}}} on the way
auto accent{JSONPointer::create("/ui/theme/accent")};
if (auto r{accent->set(doc, "teal", {.createIntermediates = true})}; !r)
    qWarning() << r.error().formatted_message();

// RFC 6902 add: "-" appends to an array
auto tags{JSONPointer::create("/ui/tags/-")};
if (auto r{tags->set(doc, "dark", {.createIntermediates = true})}; !r)
    qWarning() << r.error().formatted_message();

// replace: the target must already exist
auto theme{JSONPointer::create("/ui/theme/accent")};
if (auto r{theme->replace(doc, "crimson")}; !r)
    qWarning() << r.error().formatted_message();

// remove returns the removed value ("take")
if (auto removed{theme->remove(doc)})
    qDebug() << "was:" << *removed;

// Typed roots: consumers holding a QJsonObject/QJsonArray write in place
// (a root-replacing write that would change the kind fails, root untouched)
QJsonObject settingsObj = doc.object();
if (auto r{accent->replace(settingsObj, "ocean")}; !r)
    qWarning() << r.error().formatted_message();

// Composition: keys enter as data (never through the parser — no escaping,
// no injection surface), keeping the compile-once pattern in indexed loops
const auto interfaces{JSONPointer::create("/adp_information/interfaces").value()};
for (qsizetype i = 0; i < macs.size(); ++i)
    if (auto r{(interfaces / i / u"mac_address").replace(doc, macs[i])}; !r)
        qWarning() << r.error().formatted_message();

// as<qint64>: Qt's native JSON integer width, exact beyond 2^53
const auto id{JSONPointer::create("/data/id")
                  .and_then([&](const JSONPointer& p) { return p.evaluate(doc); })
                  .and_then(as<qint64>)
                  .value_or(-1)};
```

### JSON Patch (RFC 6902)

Compiled patches apply **atomically** (all-or-nothing): `apply()` works on a
copy and returns the result only if every operation succeeded.

```cpp
#include "json-query/JSONQuery"
#include <QJsonArray>
#include <QJsonObject>

using namespace json_query;

const QJsonArray patchJson{
    QJsonObject{{"op", "test"},    {"path", "/name"},  {"value", "old"}},
    QJsonObject{{"op", "replace"}, {"path", "/name"},  {"value", "new"}},
    QJsonObject{{"op", "add"},     {"path", "/tags/-"}, {"value", "renamed"}},
};

auto patch{JSONPatch::create(patchJson)}; // eager validation
if (!patch)
    qWarning() << patch.error().formatted_message(); // "(at operation N)"

if (auto result{patch->apply(doc)})
    doc = *result;               // or: patch->applyInPlace(doc)
else
    qWarning() << result.error().formatted_message();
```

Or build patches fluently — validated by the same rules as parsed ones, and
serializable back to the wire format via `toJson()`:

```cpp
auto patch{JSONPatchBuilder{}
               .test(u"/version", 3)
               .replace(u"/name", "new")
               .add(mappings / u"-", mapping) // composed pointer
               .build()};
if (patch)
    patch->applyInPlace(doc); // QJsonDocument&, QJsonObject&, or QJsonArray&
```

RFC 7386 Merge Patch is available as a free function (total — no error path):

```cpp
#include "json-query/json-patch/JSONMergePatch.hpp"
// {"a":{"b":1,"c":2}} + {"a":{"c":null},"d":3} -> {"a":{"b":1},"d":3}
doc = json_patch::merge_patch(doc, patchDoc);
```

### JSONPath

```cpp
#include "json-query/JSONQuery"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

using namespace json_query;

const QJsonObject store{{"books", QJsonArray{
    QJsonObject{{"title", "Book 1"}, {"price", 10.0}},
    QJsonObject{{"title", "Book 2"}, {"price", 25.5}},
    QJsonObject{{"title", "Book 3"}, {"price", 15.0}},
}}};
const QJsonDocument doc{store};

// Create a path (returns std::expected<JSONPath, Error>)
auto path{JSONPath::create("$.books[*].title")};
if (!path)
{
    qWarning() << "Parse error:" << path.error().formatted_message();
    return;
}

// evaluate returns the full nodelist: std::expected<QJsonArray, Error>
auto titles{path->evaluate(doc)};
if (titles)
    qDebug() << "Titles:" << *titles; // ["Book 1", "Book 2", "Book 3"]

// Filter expressions
auto expensive{JSONPath::create("$.books[?(@.price > 12)].title")};
if (expensive)
{
    auto result{expensive->evaluate(doc)};
    if (result)
        qDebug() << "Expensive:" << *result; // ["Book 2", "Book 3"]
}
```

### JSON Schema

```cpp
#include "json-query/JSONQuery"
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

using namespace json_query;
using namespace json_query::json_schema;

// Define a schema
const auto schemaJson{QJsonObject{
    {"type", "object"},
    {"required", QJsonArray{"name", "age"}},
    {"properties", QJsonObject{
        {"name", QJsonObject{{"type", "string"}, {"minLength", 1}}},
        {"age", QJsonObject{{"type", "integer"}, {"minimum", 0}}},
        {"email", QJsonObject{{"type", "string"}, {"format", "email"}}},
    }},
}};

// Compile once, validate many times
auto schema{JSONSchema::create(schemaJson)};
if (!schema)
{
    qCritical() << "Schema error:" << schema.error().formatted_message();
    return;
}

// Validate an instance
const QJsonObject person{{"name", "Alice"}, {"age", 30}, {"email", "alice@example.com"}};
auto result{schema->validate(QJsonValue{person})};

if (result)
{
    qDebug() << "Valid!";
}
else
{
    qDebug() << result.errorCount() << "errors:";
    for (const auto& err : result.errors())
        qDebug().noquote() << " " << err.instanceLocation << "—" << err.message;
}

// Quick bool check (no error details collected)
if (schema->isValid(QJsonValue{person}))
    qDebug() << "Still valid!";
```

### Unified Error Handling

All modules return `std::expected<T, Error>`. The `Error` type is a compact 4-byte struct:

```cpp
auto result{pointer->evaluate(doc)};
if (!result)
{
    const auto& err{result.error()};
    err.message();            // constexpr std::string_view — zero-cost
    err.message_qt();         // constexpr QStringView — zero-cost
    err.formatted_message();  // QString — includes "at token N" context
    err.domain;               // ErrorDomain enum
    err.code;                 // uint8_t error code
    err.detail;               // uint16_t token index (pointer/path eval) or
                              // operation index (patch errors)
}
```

Pick the accessor by context: `message()`/`message_qt()` are zero-cost views
(good for streaming/logging), but QTest macros and `QString` APIs want an
owning string — use `formatted_message()` there (it also appends the
token/operation context).

### Examples

Runnable example programs live in [`examples/`](examples/) (built by default
in top-level builds; each is a standalone executable):

| Example | Shows |
|---|---|
| [`simple_pointer.cpp`](examples/simple_pointer.cpp) | JSON Pointer basics: evaluation, escaping (`~0`/`~1`), error cases |
| [`pointer_writes_and_patch.cpp`](examples/pointer_writes_and_patch.cpp) | **Document mutation**: pointer `add`/`replace`/`remove`, settings-style `set` with `createIntermediates`, atomic RFC 6902 patches, RFC 7386 merge patch |
| [`simple_path.cpp`](examples/simple_path.cpp) | JSONPath basics: selectors, filters, nodelists |
| [`simple_schema.cpp`](examples/simple_schema.cpp) | JSON Schema validation basics |
| [`as_function_demo.cpp`](examples/as_function_demo.cpp) | Typed extraction with the `as<T>()` conversion helpers |
| [`compile_once_reuse.cpp`](examples/compile_once_reuse.cpp) | Compile paths/schemas once, apply to many documents |
| [`flagship_pipeline.cpp`](examples/flagship_pipeline.cpp) | End-to-end pipeline: validate → query → extract |
| [`refactor_potential.cpp`](examples/refactor_potential.cpp) | Plain Qt JSON code vs. the library, side by side |

## Test Status

*Last verified: 2026-07-10 — MSVC (cl 19.51, x64), Windows 11, Qt 6.8.5,
C++23 (previous full verification 2026-07-03 additionally covered AppleClang,
macOS (arm64), Qt 6.8.3; CI exercises Linux (GCC + clang, incl. ASan/UBSan),
macOS, and Windows (MSVC), Qt 6.8.3)*

| Test Suite | Passed | Skipped | Failed |
|---|---|---|---|
| **Core unit tests** | 56 | — | 0 |
| **Internal unit tests** | 74 | — | 0 |
| **RFC 6901 — JSON Pointer (read + write)** | 89 | — | 0 |
| **RFC 6902 — JSON Patch (json-patch-tests + unit)** | 131 | — | 0 |
| **RFC 7386 — JSON Merge Patch** | 21 | — | 0 |
| **RFC 9535 — JSONPath CTS** | 444 | 1¹ | 0 |
| **JSON Schema unit tests** | 122 | — | 0 |
| **IETF JSON Schema Draft 2020-12** | 1932 | 62² | 0 |

**Totals: 2,869 passing, 0 failing.**

¹ One CTS case skipped due to a documented upstream test-suite bug.
² Known optional-feature gaps, tracked as an exact expected-failure table
(`tests/rfc-compliance-suite/ietf-json-schema-draft-2020-12/KnownFailures.hpp`):
ECMA-262 regex semantics (closed by `JSON_QUERY_FORMAT_ECMA_REGEX`/SRELL),
IDN hostnames/emails (closed by `JSON_QUERY_FORMAT_IDN`), and strict-ASCII
hostname validation. A tracked entry that unexpectedly passes fails the
suite, so the table cannot go stale — green CI means no regressions.

## License

Dual-licensed: **Apache-2.0 WITH LLVM-exception OR MIT** — use either
license, at your option. See [`LICENSE-APACHE.txt`](LICENSE-APACHE.txt) and
[`LICENSE-MIT.txt`](LICENSE-MIT.txt). Contributions are accepted under the
same dual license.
