# AGENTS.md

Guidelines for AI agents working on this codebase.

## Project Overview

**qt-json-query** is a C++23 library providing JSON Pointer (RFC 6901), JSONPath (RFC 9535), and JSON Schema (Draft 2020-12) for Qt, validated against the official compliance suites.

Key documents: `ROADMAP.md` (production-readiness state), `CHANGELOG.md`, `doc/adr/` (architecture decisions).

## API Design Philosophy

**This library is pre-1.0 (tagged 0.x releases exist). Backwards compatibility is not yet a hard constraint, but breaking changes must now be recorded in `CHANGELOG.md`, and minor-version bumps require bumping `JSON_QUERY_ABI_NS` in `include/json-query/config/AbiNamespace.hpp` (see `doc/adr/005`).**

The goal is to design a great, well-thought-out API for the initial public release. This means:

- ✅ **Breaking changes are acceptable** - Focus on getting the API right, not maintaining compatibility
- ✅ **Refactor freely** - Improve code structure, naming, and interfaces without hesitation
- ✅ **Experiment with better designs** - Try `std::expected`, dispatch tables, and modern C++ patterns
- ✅ **Optimize without constraints** - Performance improvements that change APIs are encouraged
- ✅ **Learn from mistakes** - If an API design isn't working well, change it now before release

**Once the library reaches v1.0 and goes public, we will maintain backwards compatibility. Until then, the focus is on quality over compatibility.**

## Build System

- **CMake** with Ninja generator
- **C++23** standard required
- **Qt 6.8+** dependency
- **CTRE** (compile-time regular expressions) for performance

### Build Commands

```pwsh
# Windows/MSVC: dot-source first for a build-ready shell (VS dev env + Qt on
# PATH + CMAKE_PREFIX_PATH). Respects a preconfigured environment (superbuild);
# qt.user.json can be generated via: python scripts/init_qt_config.py
. .\Init-DevEnv.ps1
cmake --preset debug-msvc
```

```bash
# Configure (MSVC, manual alternative)
cmake -B build-debug-msvc -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=<Qt6_DIR>

# Build library
cmake --build build-debug-msvc --target json_query

# Build and run tests (see tests/README.md for the full target list)
cmake --build build-debug-msvc --target json_query_core_tests
./build-debug-msvc/tests/json_query_core_tests
```

## Code Style Requirements

### 1. Brace-Initialization (Mandatory — with one exception)

Always use brace-initialization `{}` instead of `=` assignment.

**Exception (ADR-001, doc/adr/001):** never brace-init a Qt JSON container
from another container — `QJsonArray arr{someArray}` selects the
`initializer_list` constructor and produces an array *containing* the array.
Use copy-init there: `const QJsonArray arr = value.toArray();`

```cpp
// ✅ Correct
const auto index{nodes.size()};
auto result{parseKeyword(value)};
TypeConstraint constraint{};
ValidationResult result{};

// ❌ Incorrect
const auto index = nodes.size();
auto result = parseKeyword(value);
TypeConstraint constraint;
ValidationResult result;
```

### 2. const by Default

Use `const` for all variables that are not modified:

```cpp
// ✅ Correct
const auto arr{value.toArray()};
const auto size{obj.size()};

// ❌ Incorrect (if not modified)
auto arr{value.toArray()};
```

### 3. Prefer auto with Brace-Init

Use `auto` with brace-initialization for type deduction:

```cpp
// ✅ Correct
const auto schemaObj{schemaValue.toObject()};
auto typeResult{parseTypeKeyword(value)};

// ❌ Incorrect
const QJsonObject schemaObj{schemaValue.toObject()};
```

### 4. constexpr Where Applicable

Use `constexpr` for compile-time constants:

```cpp
// ✅ Correct
static constexpr auto slicePattern = ctre::match<"...">;
inline constexpr auto error_map = utils::detail::ErrorMap<...>{...};
```

### 5. Qt String Literals

Use `u"..."_qt_s` for Qt string literals in concatenation:

```cpp
// ✅ Correct
schemaPath + u"/type"_qt_s
instancePath + u"/"_qt_s + propName

// ❌ Incorrect
schemaPath + u"/type"
schemaPath + "/type"
```

### 6. Error Handling with std::expected

Return `std::expected<T, Error>` for fallible operations:

```cpp
// ✅ Correct
std::expected<JSONSchema, Error> JSONSchema::create(const QJsonValue& schema);

// Usage
auto result{JSONSchema::create(schemaJson)};
if (!result)
    return std::unexpected(result.error());
```

### 7. Default Member Initializers

Use brace-init for struct member defaults:

```cpp
struct ObjectSchema {
    std::vector<QString> required{};
    std::optional<std::size_t> minLength{};
    bool uniqueItems{false};
};
```

### 8. Avoid Raw Index-Based For Loops

Prefer range-based for loops, algorithms, or `std::views` over raw index-based iteration:

```cpp
// ✅ Correct - range-based for
for (const auto& item : container)
    process(item);

// ✅ Correct - algorithm
std::ranges::any_of(container, predicate);

// ✅ Correct - indexed iteration with enumerate (C++23)
for (auto [i, item] : std::views::enumerate(container))
    process(i, item);

// ❌ Avoid - raw index loop
for (std::size_t i = 0; i < container.size(); ++i)
    process(container[i]);

// ❌ Avoid - C-style index loop
for (int i = 0; i < arr.size(); ++i)
    process(arr[i]);
```

When index access is truly needed (e.g., comparing adjacent elements), prefer `std::views::enumerate` or document why the raw loop is necessary.

## Formatting

Run clang-format before committing:

```bash
clang-format -i <file.cpp>
```

The project uses a `.clang-format` configuration file in the root.

## Code Style Automation

A Python script is available to apply style transformations:

```bash
# Dry run (preview changes)
python refactoring/scripts/code_style_modernizer.py --dry-run

# Apply changes and verify build
python refactoring/scripts/code_style_modernizer.py --verify

# Process all files in codebase
python refactoring/scripts/code_style_modernizer.py --all --verify
```

## Project Structure

```
include/json-query/
├── json-path/          # JSONPath (RFC 9535) implementation
├── json-pointer/       # JSON Pointer (RFC 6901) implementation
├── json-schema/        # JSON Schema validation
│   ├── internal/       # Internal implementation details
│   ├── JSONSchema.hpp  # Public API
│   └── ...
└── utils/              # Shared utilities

src/json-query/
├── json-path/
├── json-pointer/
└── json-schema/

tests/
├── json-query/
│   ├── json-path/
│   ├── json-pointer/
│   └── json-schema/
└── cmake/              # Test configuration
```

## Testing

- **GoogleTest** framework
- Tests are organized by component
- Run specific test suites with CTest:

```bash
ctest --test-dir build-debug-msvc -R JSONSchema
ctest --test-dir build-debug-msvc -R RFC9535
```

## MSVC-Specific Notes

- Template depth may need to be increased for complex CTRE patterns (cl
  19.51+ exceeds the default of 1000 with error C2999)
- This is handled in `src/json-query/CMakeLists.txt` (compiler-ID guard, not
  `if(MSVC)`, so clang-cl keeps taking the Clang `-ftemplate-depth` branch):

```cmake
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  set_source_files_properties(
    json-path/JSONPathParseUtils.cpp PROPERTIES COMPILE_OPTIONS
                                                "/templateDepth:2000")
endif()
```

## Key Patterns

### Factory Pattern for Schema/Path Creation

```cpp
auto schema{JSONSchema::create(schemaJson)};
if (!schema)
    // Handle error

auto result{schema->validate(instance)};
```

### Validation Result Pattern

```cpp
ValidationResult result{schema.validate(instance)};
if (!result.isValid()) {
    for (const auto& error : result.errors()) {
        // error.instanceLocation, error.message, error.code
    }
}
```

## Dependencies

| Dependency | Purpose | Source |
|------------|---------|--------|
| Qt 6.8+ (Core, Network) | JSON types, regex, QHostAddress | External |
| CTRE v3.10.0 (pinned commit) | Compile-time regex | find_package, FetchContent fallback |
| tl::function_ref v1.0.0 | Non-owning callable refs | find_package, FetchContent fallback |
| GoogleTest | Unit testing | FetchContent |
| SRELL (optional) | ECMA-262 regex formats | FetchContent (SHA256-pinned) |
| libidn2/ada (optional) | IDN formats | System / FetchContent |

## Additional Ground Rules

- **Exception policy**: the library never throws; all recoverable errors go
  through `std::expected`. `noexcept` means "OOM is fatal" (documented on the
  `create()` factories).
- **Error enums** (ADR-004): numeric values are not API — keep enumerators
  sortable, never rely on or persist their values.
- **Compliance xfails**: expected IETF suite failures live in
  `tests/rfc-compliance-suite/ietf-json-schema-draft-2020-12/KnownFailures.hpp`;
  a fixed test must be removed from the table (the driver enforces this).
- **License headers**: `// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT`
  on every new file.
