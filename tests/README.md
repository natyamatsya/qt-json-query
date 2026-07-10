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
| `json_query_core_tests` | Core | 39 | JSONPath and JSONPointer basic functionality (incl. pointer write ops) |
| `json_query_internal_tests` | Internal | 74 | Internal components (cursors, filter storage) |
| `rfc9535_compliance_tests` | RFC 9535 | 443 | JSONPath compliance (official CTS) |
| `rfc6901_compliance_tests` | RFC 6901 | 83 | JSON Pointer compliance (read + write suites) |
| `rfc6902_compliance_tests` | RFC 6902 | 122 | JSON Patch compliance (community suite) + JSONPatch unit tests |
| `rfc7386_compliance_tests` | RFC 7386 | 20 | JSON Merge Patch (complete Appendix A table + edge cases) |
| `json_schema_tests` | Schema | 122 | JSON Schema validation unit tests |
| `ietf_json_schema_draft_2020_12_compliance_tests` | IETF Draft 2020-12 | 1932 (+62 tracked xfail skips) | JSON Schema compliance (IETF test suite) |

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

# JSON Patch / Merge Patch tests
ctest --test-dir build -R RFC6902
ctest --test-dir build -R RFC7386
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

Runs a comprehensive test suite covering RFC 6901 §5 examples, escape sequences (`~0`, `~1`), special characters, container-relative token semantics, and error cases — plus a write-operation suite (`rfc6901-write-tests.json`, seeded from the RFC 6902 Appendix A examples) exercising `add`/`replace`/`remove`/`set` against both the `QJsonDocument` and `QJsonValue` overloads with strong-guarantee and COW-isolation checks.

```bash
cmake --build build --target rfc6901_compliance_tests
./build/tests/rfc6901_compliance_tests
```

### JSON Patch — RFC 6902

Runs the community [json-patch-tests](https://github.com/json-patch/json-patch-tests) suite from the `compliance/json-patch-tests` submodule (`spec_tests.json` + `tests.json`), plus the JSONPatch unit tests. Expected failures would be tracked in `tests/rfc-compliance-suite/rfc-6902/KnownFailures.hpp` with the same self-cleaning contract as the IETF schema suite (a stale entry fails the build) — the table is currently empty: all enabled cases pass.

```bash
cmake --build build --target rfc6902_compliance_tests
./build/tests/rfc6902_compliance_tests
```

### JSON Merge Patch — RFC 7386

Data-driven suite containing the complete RFC 7386 Appendix A example table plus edge cases.

```bash
cmake --build build --target rfc7386_compliance_tests
./build/tests/rfc7386_compliance_tests
```

### JSON Schema — IETF Draft 2020-12

Runs the official [JSON Schema Test Suite](https://github.com/json-schema-org/JSON-Schema-Test-Suite) for Draft 2020-12.

```bash
cmake --build build --target ietf_json_schema_draft_2020_12_compliance_tests
./build/tests/ietf_json_schema_draft_2020_12_compliance_tests
```

## CMake Options

The test tree as a whole is gated by `JSON_QUERY_BUILD_TESTS` (default: `ON`
for top-level builds when `BUILD_TESTING` is on, `OFF` when the project is
embedded via `add_subdirectory`). Within it, all suites are enabled by
default (except fuzz tests):

| Option | Default | Description |
|---|---|---|
| `JSON_QUERY_ENABLE_CORE_TESTS` | `ON` | Core JSONPath/JSONPointer tests |
| `JSON_QUERY_ENABLE_INTERNAL_TESTS` | `ON` | Internal component tests |
| `JSON_QUERY_ENABLE_RFC9535_TESTS` | `ON` | RFC 9535 JSONPath compliance |
| `JSON_QUERY_ENABLE_RFC6901_TESTS` | `ON` | RFC 6901 JSON Pointer compliance |
| `JSON_QUERY_ENABLE_RFC6902_TESTS` | `ON` | RFC 6902 JSON Patch compliance + unit tests |
| `JSON_QUERY_ENABLE_RFC7386_TESTS` | `ON` | RFC 7386 JSON Merge Patch |
| `JSON_QUERY_ENABLE_SCHEMA_TESTS` | `ON` | JSON Schema unit tests |
| `JSON_QUERY_ENABLE_IETF_JSON_SCHEMA_DRAFT_2020_12_TESTS` | `ON` | IETF Draft 2020-12 compliance |
| `JSON_QUERY_ENABLE_FUZZ_TESTS` | `OFF` | LibFuzzer-based fuzz testing |

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
│   ├── json-patch/                  # JSONPatch unit tests
│   │   └── JSONPatchGTest.cpp
│   ├── json-pointer/                # Core JSONPointer tests
│   │   ├── JSONPointerGTest.cpp
│   │   └── JSONPointerWriteGTest.cpp
│   └── json-schema/                 # JSON Schema unit tests
│       ├── JSONSchemaBasicGTest.cpp
│       ├── JSONSchemaFormatGTest.cpp
│       ├── JSONSchemaKeywordGTest.cpp
│       ├── JSONSchemaRefGTest.cpp
│       └── JSONSchemaRegistryGTest.cpp
├── rfc-compliance-suite/            # Compliance suites (data-driven)
│   ├── rfc-6901/
│   ├── rfc-6902/
│   ├── rfc-7386/
│   ├── rfc-9535/
│   └── ietf-json-schema-draft-2020-12/
└── fuzz/                            # Fuzz tests (optional)
```

**Naming convention**: All test files use the `*GTest.cpp` suffix.

## Fuzz Testing

Fuzz tests use [LibFuzzer](https://llvm.org/docs/LibFuzzer.html) and require Clang. They are disabled by default.

| Target | What it fuzzes |
| --- | --- |
| `fuzz_jsonpath_parsing` | Random bytes → JSONPath parsing + evaluation |
| `fuzz_jsonpointer_parsing` | Random bytes → JSONPointer parsing + evaluation |
| `fuzz_jsonpointer_write` | Fuzzed pointer + document + value → write ops (round-trip and untouched-on-failure invariants) |
| `fuzz_jsonpatch_apply` | Fuzzed patch + document → create + apply (atomicity invariants) |
| `fuzz_combined_evaluation` | Fuzzed JSONPath expression + fuzzed JSON document |
| `fuzz_jsonschema` | Fuzzed schema (compile, lenient + strict options) + fuzzed instance (validate) |

### Building

`JSON_QUERY_ENABLE_FUZZ_TESTS=ON` adds ASan/UBSan instrumentation globally (library + fuzz targets):

```bash
cmake -B build-fuzz -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH=/path/to/qt/6.x/platform \
    -DJSON_QUERY_ENABLE_FUZZ_TESTS=ON \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_C_COMPILER=clang
ninja -C build-fuzz fuzz_jsonpath_parsing fuzz_jsonpointer_parsing fuzz_combined_evaluation fuzz_jsonschema
```

### Running

```bash
# Single fuzzer (5 minutes)
./build-fuzz/fuzz/fuzz_jsonpath_parsing build-fuzz/fuzz/corpus/fuzz_jsonpath_parsing -max_total_time=300

# All fuzzers (5 minutes each)
ninja -C build-fuzz run_all_fuzzers

# Quick run (30 seconds each)
ninja -C build-fuzz fuzz_quick
```

### macOS notes

On macOS with homebrew LLVM, you may also need to set `CMAKE_AR` and `CMAKE_RANLIB` to the LLVM versions to avoid archive format mismatches:

```bash
-DCMAKE_AR=/opt/homebrew/opt/llvm/bin/llvm-ar \
-DCMAKE_RANLIB=/opt/homebrew/opt/llvm/bin/llvm-ranlib
```

**Resolved (2026-07-03)**: Homebrew LLVM 21 shipped a `libclang_rt.fuzzer_osx.a` compiled against a different libc++ ABI, causing `__hash_memory` linker errors. The `cmake/toolchains/llvm-clang.cmake` toolchain now injects Homebrew LLVM's own libc++ into the link (`-L`/`-rpath`), which resolves this — fuzz targets build and run on macOS with Homebrew LLVM 22 via `-DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/llvm-clang.cmake -DJSON_QUERY_ENABLE_FUZZ_TESTS=ON`. Do not ship binaries produced with this toolchain (developer-machine rpath).
