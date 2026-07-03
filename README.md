# qt-json-query

!!! DISCLAIMER: This project is "vibe-engineered", unstable and currently not used in production.
    It is an experiment to see how agentic development can be guided by compliance acceptance test
    suites. The covered JSON specs seem useful to have in a Qt context. This library needs careful
    evaluation before serious use. !!!

A modern C++23 library providing [JSON Pointer (RFC 6901)](https://www.rfc-editor.org/rfc/rfc6901), [JSONPath (RFC 9535)](https://www.rfc-editor.org/rfc/rfc9535), and [JSON Schema (Draft 2020-12)](https://json-schema.org/draft/2020-12/json-schema-core) for Qt.

- Full **JSON Pointer (RFC 6901)** compliance (33/33 tests)
- Full **JSONPath (RFC 9535)** compliance (443/444 tests, 1 skipped)
- **JSON Schema (Draft 2020-12)** validation — 1932/1994 IETF tests (96.9%)
- Unified **`Error`** type with `std::expected` across all modules
- Compile-time regular expressions (**CTRE**) for fast parsing
- Clean integration with Qt JSON types (`QJsonValue`, `QJsonDocument`, etc.)

## Requirements

| Dependency | Version |
|---|---|
| **C++23 compiler** | Clang 16+, GCC 13+, or MSVC 17.8+ |
| **Qt** | 6.7+ |
| **CTRE** | v3.10.0 (pinned commit, fetched automatically via CMake FetchContent) |
| **GoogleTest** | (fetched automatically for tests) |

### Optional Dependencies

| Dependency | CMake Option | Purpose |
|---|---|---|
| **SRELL** | `JSON_QUERY_FORMAT_ECMA_REGEX=ON` | ECMA-262 regex for `pattern` + `regex` format |
| **libidn2** | `JSON_QUERY_FORMAT_IDN=ON` | IDN hostname/email validation |

## Building

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH=/path/to/qt/6.x/platform
cmake --build build
```

Run tests:

```bash
ctest --test-dir build                                     # all tests
cmake --build build --target rfc9535_compliance_tests      # specific suite
```

See [`tests/README.md`](tests/README.md) for the full list of test targets and CMake options.

## Integration

```cmake
add_subdirectory(path/to/qt-json-query)
target_link_libraries(your_target PRIVATE json_query)
```

### Dependency resolution (vcpkg-friendly)

Internal dependencies are resolved with `find_package` first and fetched via
FetchContent only as a fallback, so package managers control the versions:

| Dependency | `find_package` name | vcpkg port | FetchContent fallback |
|---|---|---|---|
| **CTRE** | `ctre` | `ctre` | pinned commit (v3.10.0+) |
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

Then include the umbrella header:

```cpp
#include "json-query/JSONQuery"
using namespace json_query;
```

This provides `JSONPointer`, `JSONPath`, and `JSONSchema` in the `json_query` namespace.

## Usage

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

All three modules return `std::expected<T, Error>`. The `Error` type is a compact 4-byte struct:

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
    err.detail;               // uint16_t token index (for eval errors)
}
```

## Test Status

*Last verified: 2026-02-08 — Homebrew clang 21.1.8 (LLVM), macOS, Qt 6.8.3, C++23*

| Test Suite | Passed | Total | Rate |
|---|---|---|---|
| **Core unit tests** | 18 | 18 | 100% |
| **Internal unit tests** | 60 | 60 | 100% |
| **RFC 6901 — JSON Pointer** | 33 | 33 | 100% |
| **RFC 9535 — JSONPath CTS** | 443 | 444 | 99.8% |
| **JSON Schema unit tests** | 116 | 116 | 100% |
| **IETF JSON Schema Draft 2020-12** | 1932 | 1994 | 96.9% |

**Totals: 2602 / 2665 tests passing (97.6%)**

Remaining IETF failures are in `ecmascript-regex.json` (ECMA-262 Unicode semantics — requires SRELL), `hostname.json` (Unicode hostnames), and `idn-hostname.json` / `idn-email.json` (IDN — requires libidn2).

## License

Apache-2.0 WITH LLVM-exception
