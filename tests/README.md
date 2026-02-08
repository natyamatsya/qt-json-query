# Test Suites

This project uses [GoogleTest](https://github.com/google/googletest) with modular test targets. Each suite can be built and run independently.

## Quick Start

```bash
# Configure (adjust CMAKE_PREFIX_PATH to your Qt installation)
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH=/path/to/qt/6.x/platform

# Build all test targets
cmake --build build

# Run all tests via CTest
ctest --test-dir build
```

## Test Targets

| Target | Suite | Tests | Description |
|---|---|---|---|
| `json_query_core_tests` | Core | 15 | JSONPath and JSONPointer basic functionality |
| `json_query_internal_tests` | Internal | 74 | Internal components (cursors, filter storage) |
| `rfc9535_compliance_tests` | RFC 9535 | 443 | JSONPath compliance (official CTS) |
| `rfc6901_compliance_tests` | RFC 6901 | 33 | JSON Pointer compliance |
| `json_schema_tests` | Schema | 116 | JSON Schema validation unit tests |
| `ietf_json_schema_draft_2020_12_compliance_tests` | IETF Draft 2020-12 | 1932+ | JSON Schema compliance (IETF test suite) |

### Build a specific target

```bash
cmake --build build --target rfc9535_compliance_tests
```

### Run a specific target

```bash
./build/tests/rfc9535_compliance_tests
```

### Run with CTest filters

```bash
# All RFC 9535 tests
ctest --test-dir build -R RFC9535

# All JSON Schema tests
ctest --test-dir build -R JSONSchema

# All RFC 6901 tests
ctest --test-dir build -R RFC6901
```

## Compliance Test Suites

### JSONPath — RFC 9535

Runs the official [JSONPath Compliance Test Suite](https://github.com/jsonpath-standard/jsonpath-compliance-test-suite) from `compliance/rfc-9535-jsonpath-test-suite/tests/*.json`.

```bash
cmake --build build --target rfc9535_compliance_tests
./build/tests/rfc9535_compliance_tests
```

The CTS JSON files are loaded at runtime. The `JSON_QUERY_SOURCE_DIR` compile definition points the test binary to the project root so it can locate the compliance data.

### JSON Pointer — RFC 6901

Runs a comprehensive test suite covering RFC 6901 §5 examples, escape sequences (`~0`, `~1`), special characters, and error cases.

```bash
cmake --build build --target rfc6901_compliance_tests
./build/tests/rfc6901_compliance_tests
```

### JSON Schema — IETF Draft 2020-12

Runs the official [JSON Schema Test Suite](https://github.com/json-schema-org/JSON-Schema-Test-Suite) for Draft 2020-12.

```bash
cmake --build build --target ietf_json_schema_draft_2020_12_compliance_tests
./build/tests/ietf_json_schema_draft_2020_12_compliance_tests
```

## CMake Options

All test suites are enabled by default (except fuzz tests):

| Option | Default | Description |
|---|---|---|
| `JSON_QUERY_ENABLE_CORE_TESTS` | `ON` | Core JSONPath/JSONPointer tests |
| `JSON_QUERY_ENABLE_INTERNAL_TESTS` | `ON` | Internal component tests |
| `JSON_QUERY_ENABLE_RFC9535_TESTS` | `ON` | RFC 9535 JSONPath compliance |
| `JSON_QUERY_ENABLE_RFC6901_TESTS` | `ON` | RFC 6901 JSON Pointer compliance |
| `JSON_QUERY_ENABLE_SCHEMA_TESTS` | `ON` | JSON Schema unit tests |
| `JSON_QUERY_ENABLE_IETF_JSON_SCHEMA_DRAFT_2020_12_TESTS` | `ON` | IETF Draft 2020-12 compliance |
| `ENABLE_FUZZ_TESTS` | `OFF` | LibFuzzer-based fuzz testing |

To disable a suite:

```bash
cmake -B build -S . -G Ninja -DJSON_QUERY_ENABLE_RFC9535_TESTS=OFF ...
```

## File Organization

```
tests/
├── cmake/                           # CMake modules (one per suite)
├── include/framework/               # Shared test utilities
│   └── JSONMatchersGTest.hpp
├── json-query/
│   ├── json-path/                   # Core JSONPath tests
│   │   ├── JSONPathGTest.cpp
│   │   ├── JSONPathLogicalOrGTest.cpp
│   │   └── internal/               # Internal component tests
│   ├── json-pointer/                # Core JSONPointer tests
│   │   └── JSONPointerGTest.cpp
│   └── json-schema/                 # JSON Schema unit tests
│       ├── JSONSchemaBasicGTest.cpp
│       ├── JSONSchemaFormatGTest.cpp
│       ├── JSONSchemaKeywordGTest.cpp
│       ├── JSONSchemaRefGTest.cpp
│       └── JSONSchemaRegistryGTest.cpp
├── rfc-compliance-suite/            # Compliance suites (data-driven)
│   ├── rfc-6901/
│   ├── rfc-9535/
│   └── ietf-json-schema-draft-2020-12/
└── fuzz/                            # Fuzz tests (optional)
```

**Naming convention**: All test files use the `*GTest.cpp` suffix.
