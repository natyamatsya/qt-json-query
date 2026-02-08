# JSON Schema Validation Integration Plan for qt-json-query

## Executive Summary

This document outlines how to extend the **qt-json-query** library with JSON Schema validation capabilities, following the architectural principles established in the design document and adapting them to the existing codebase patterns.

The qt-json-query library currently provides:

- **JSONPointer** (RFC 6901) - Direct access to JSON elements
- **JSONPath** (RFC 9535) - Query-based access with filters and wildcards

Adding **JSON Schema Validation** (Draft 2020-12) creates a natural trilogy of JSON tooling, sharing the same Qt-native API design, error handling patterns, and performance characteristics.

---

## 1. Architectural Alignment

### 1.1 Existing Patterns to Follow

The codebase establishes clear patterns that the schema validation module must adhere to:

| Pattern | Current Implementation | Schema Validation Equivalent |
|---------|----------------------|------------------------------|
| **Factory creation** | `JSONPath::create()` → `std::expected<JSONPath, Error>` | `JSONSchema::create()` → `std::expected<JSONSchema, Error>` |
| **Evaluation** | `path.evaluate(doc)` → `std::expected<QJsonValue, Error>` | `schema.validate(doc)` → `std::expected<ValidationResult, Error>` |
| **Error domains** | `ErrorDomain::PathParse`, `PathEval`, `PointerParse`, `PointerEval` | `ErrorDomain::SchemaParse`, `SchemaEval` |
| **Compact errors** | `Error` (2 bytes: domain + code) | Extend with schema-specific codes |
| **Namespace** | `json_query::json_path`, `json_query::json_pointer` | `json_query::json_schema` |
| **Header structure** | `include/json-query/json-path/*.hpp` | `include/json-query/json-schema/*.hpp` |

### 1.2 Proposed Namespace & Directory Structure

```
include/json-query/
├── json-schema/
│   ├── JSONSchema.hpp              # Main public API
│   ├── JSONSchemaError.hpp         # Parse/Eval error enums
│   ├── JSONSchemaCompile.hpp       # Schema compilation internals
│   ├── JSONSchemaValidate.hpp      # Validation engine
│   ├── JSONSchemaResult.hpp        # Validation result + error list
│   ├── JSONSchemaKeywords.hpp      # Keyword handlers (type, properties, etc.)
│   ├── JSONSchemaRefResolver.hpp   # $ref and $dynamicRef resolution
│   └── internal/
│       ├── SchemaNode.hpp          # Compiled schema node types
│       ├── KeywordRegistry.hpp     # Keyword → handler mapping
│       └── FormatValidators.hpp    # format keyword validators

src/json-query/
├── json-schema/
│   ├── JSONSchema.cpp
│   ├── JSONSchemaCompile.cpp
│   ├── JSONSchemaValidate.cpp
│   ├── JSONSchemaKeywords.cpp
│   ├── JSONSchemaRefResolver.cpp
│   └── internal/
│       ├── SchemaNode.cpp
│       └── FormatValidators.cpp

tests/
├── json-query/
│   └── json-schema/
│       ├── JSONSchemaBasicTests.cpp
│       ├── JSONSchemaKeywordTests.cpp
│       └── JSONSchemaRefTests.cpp
├── rfc-compliance-suite/
│   └── json-schema-test-suite/     # Official test suite submodule
└── cmake/
    └── JSONSchemaTests.cmake
```

---

## 2. Core API Design

### 2.1 Primary Classes

```cpp
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <QJsonDocument>
#include <QJsonValue>
#include <expected>
#include "json-query/utils/JSONError.hpp"
#include "json-query/json-schema/JSONSchemaResult.hpp"

namespace json_query::json_schema
{

class JSONSchema
{
  public:
    // Factory (mirrors JSONPath::create and JSONPointer::create)
    using ParseResult = std::expected<JSONSchema, json_query::Error>;
    
    static ParseResult create(const QJsonObject& schemaObject);
    static ParseResult create(const QJsonDocument& schemaDoc);
    static ParseResult fromFile(const QString& filePath);
    
    // Validation API
    [[nodiscard]] ValidationResult validate(const QJsonValue& instance) const;
    [[nodiscard]] ValidationResult validate(const QJsonDocument& doc) const;
    
    // Quick validation (returns bool, discards detailed errors)
    [[nodiscard]] bool isValid(const QJsonValue& instance) const;
    
    // Schema introspection
    [[nodiscard]] QString schemaId() const;      // $id if present
    [[nodiscard]] QString schemaVersion() const; // $schema dialect
    [[nodiscard]] bool isCompiled() const;
    
    // Move/copy (schema is immutable after creation, safe to share)
    JSONSchema(JSONSchema&&) noexcept = default;
    JSONSchema(const JSONSchema&) = default;
    JSONSchema& operator=(JSONSchema&&) noexcept = default;
    JSONSchema& operator=(const JSONSchema&) = default;
    
  private:
    JSONSchema() = default;
    std::shared_ptr<const internal::CompiledSchema> m_compiled;
};

} // namespace json_query::json_schema
```

### 2.2 Validation Result

```cpp
namespace json_query::json_schema
{

struct ValidationError
{
    QString instanceLocation;  // JSON Pointer to failing data (e.g., "/address/zip")
    QString schemaLocation;    // JSON Pointer within schema (e.g., "#/properties/address")
    QString message;           // Human-readable description
    EvalError code;            // Machine-readable error code
    
    // For nested errors (e.g., from allOf/anyOf)
    std::vector<ValidationError> nested;
};

class ValidationResult
{
  public:
    [[nodiscard]] bool isValid() const noexcept { return m_errors.empty(); }
    [[nodiscard]] explicit operator bool() const noexcept { return isValid(); }
    
    [[nodiscard]] std::size_t errorCount() const noexcept { return m_errors.size(); }
    [[nodiscard]] const std::vector<ValidationError>& errors() const noexcept { return m_errors; }
    
    // Convenience: first error message (empty if valid)
    [[nodiscard]] QString firstError() const;
    
    // Machine-readable output (JSON Schema output format)
    [[nodiscard]] QJsonObject toJson() const;
    
    // Human-readable summary
    [[nodiscard]] QString toString() const;
    
  private:
    friend class JSONSchema;
    std::vector<ValidationError> m_errors;
};

} // namespace json_query::json_schema
```

### 2.3 Error Integration

Extend the existing `Error` system in `JSONError.hpp`:

```cpp
// Add to ErrorDomain enum
enum class ErrorDomain : std::uint8_t
{
    PathParse,
    PointerParse,
    PathEval,
    PointerEval,
    Convert,
    SchemaParse,   // NEW: Schema compilation errors
    SchemaEval     // NEW: Schema validation errors
};

// In json-schema/JSONSchemaError.hpp
namespace json_query::json_schema
{

enum class ParseError : std::uint8_t
{
    InvalidSchemaStructure,      // Schema must be object or boolean
    UnknownKeyword,              // Unrecognized keyword (warning in strict mode)
    InvalidKeywordValue,         // e.g., "type": 123 instead of string
    InvalidRegexPattern,         // pattern keyword has invalid regex
    CircularReference,           // $ref creates infinite loop
    UnresolvedReference,         // $ref target not found
    InvalidJsonPointer,          // $ref fragment is invalid JSON Pointer
    UnsupportedDialect,          // $schema specifies unsupported draft
    DuplicateAnchor,             // Same $anchor defined twice
};

enum class EvalError : std::uint8_t
{
    TypeMismatch,                // "type" constraint failed
    RequiredMissing,             // Required property missing
    AdditionalPropertiesInvalid, // additionalProperties: false violated
    PatternMismatch,             // "pattern" regex didn't match
    MinLengthViolation,
    MaxLengthViolation,
    MinimumViolation,
    MaximumViolation,
    ExclusiveMinimumViolation,
    ExclusiveMaximumViolation,
    MultipleOfViolation,
    MinItemsViolation,
    MaxItemsViolation,
    UniqueItemsViolation,
    MinPropertiesViolation,
    MaxPropertiesViolation,
    EnumMismatch,
    ConstMismatch,
    AllOfFailed,
    AnyOfFailed,
    OneOfFailed,
    NotFailed,
    IfThenElseFailed,
    FormatInvalid,
    ContentEncodingInvalid,
    UnevaluatedPropertiesInvalid,
    UnevaluatedItemsInvalid,
    DependentRequiredMissing,
    DependentSchemasFailed,
};

} // namespace json_query::json_schema
```

---

## 3. Internal Architecture

### 3.1 Schema Compilation

Following the design document's emphasis on schema compilation for performance:

```cpp
namespace json_query::json_schema::internal
{

// Variant-based schema node (compile-time type safety)
using SchemaNode = std::variant<
    BooleanSchema,          // true/false schemas
    ObjectSchema,           // Full schema with keywords
    RefSchema               // $ref placeholder (resolved at compile time)
>;

struct ObjectSchema
{
    // Type constraints
    std::optional<TypeConstraint> type;
    
    // Object keywords
    std::unordered_map<QString, std::size_t> properties;  // name → node index
    std::vector<std::pair<QRegularExpression, std::size_t>> patternProperties;
    std::optional<std::size_t> additionalProperties;
    std::vector<QString> required;
    std::optional<std::size_t> minProperties;
    std::optional<std::size_t> maxProperties;
    
    // Array keywords
    std::optional<std::size_t> items;         // node index for items schema
    std::vector<std::size_t> prefixItems;     // node indices for prefix items
    std::optional<std::size_t> contains;
    std::optional<std::size_t> minContains;
    std::optional<std::size_t> maxContains;
    std::optional<std::size_t> minItems;
    std::optional<std::size_t> maxItems;
    bool uniqueItems = false;
    
    // String keywords
    std::optional<QRegularExpression> pattern;
    std::optional<std::size_t> minLength;
    std::optional<std::size_t> maxLength;
    std::optional<QString> format;
    
    // Numeric keywords
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::optional<double> exclusiveMinimum;
    std::optional<double> exclusiveMaximum;
    std::optional<double> multipleOf;
    
    // Combinators (store node indices)
    std::vector<std::size_t> allOf;
    std::vector<std::size_t> anyOf;
    std::vector<std::size_t> oneOf;
    std::optional<std::size_t> notSchema;
    
    // Conditional
    std::optional<std::size_t> ifSchema;
    std::optional<std::size_t> thenSchema;
    std::optional<std::size_t> elseSchema;
    
    // Value constraints
    std::optional<QJsonArray> enumValues;
    std::optional<QJsonValue> constValue;
    
    // Unevaluated (2019-09+)
    std::optional<std::size_t> unevaluatedProperties;
    std::optional<std::size_t> unevaluatedItems;
};

// Flat array of nodes (cache-friendly, index-based references)
struct CompiledSchema
{
    std::vector<SchemaNode> nodes;
    std::size_t rootIndex = 0;
    
    // Anchor registry for $ref resolution
    std::unordered_map<QString, std::size_t> anchors;      // $anchor → node index
    std::unordered_map<QString, std::size_t> dynamicAnchors; // $dynamicAnchor
    
    // Metadata
    QString schemaId;
    QString dialect;
};

} // namespace json_query::json_schema::internal
```

### 3.2 Leveraging Existing JSONPointer

The schema validator will **reuse JSONPointer** for:

- Resolving `$ref` fragments (e.g., `#/definitions/Address`)
- Reporting error locations in instances
- Navigating schema structure during compilation

```cpp
// Example: resolving a $ref using existing JSONPointer
auto resolveRef(const QString& ref, const QJsonObject& rootSchema) 
    -> std::expected<QJsonValue, Error>
{
    if (!ref.startsWith(u'#')) {
        return std::unexpected(Error(ParseError::UnresolvedReference));
    }
    
    QString fragment = ref.mid(1); // Remove '#'
    if (fragment.isEmpty()) {
        return QJsonValue(rootSchema); // Reference to root
    }
    
    // Use existing JSONPointer implementation
    auto pointer = json_pointer::JSONPointer::create(fragment);
    if (!pointer) {
        return std::unexpected(pointer.error());
    }
    
    return pointer->evaluate(QJsonValue(rootSchema));
}
```

### 3.3 Regex Handling with QRegularExpression

Per the design document, use `QRegularExpression` (PCRE2-based) for `pattern` keywords:

```cpp
// During schema compilation - compile patterns once
auto compilePattern(const QString& pattern) 
    -> std::expected<QRegularExpression, Error>
{
    QRegularExpression regex(pattern);
    
    if (!regex.isValid()) {
        // Return parse error with pattern error details
        return std::unexpected(Error(ParseError::InvalidRegexPattern));
    }
    
    // Enable JIT optimization for repeated use
    regex.optimize();
    
    return regex;
}

// During validation - reuse compiled regex
bool matchesPattern(const QRegularExpression& pattern, QStringView value)
{
    return pattern.match(value).hasMatch();
}
```

---

## 4. CMake Integration

### 4.1 Source File Updates

Add to `src/json-query/CMakeLists.txt`:

```cmake
# Add JSON Schema sources
set(JSON_SCHEMA_SOURCES
    json-schema/JSONSchema.cpp
    json-schema/JSONSchemaCompile.cpp
    json-schema/JSONSchemaValidate.cpp
    json-schema/JSONSchemaKeywords.cpp
    json-schema/JSONSchemaRefResolver.cpp
    json-schema/internal/SchemaNode.cpp
    json-schema/internal/FormatValidators.cpp)

set(JSON_QUERY_SOURCES
    ${JSON_QUERY_SOURCES}
    ${JSON_SCHEMA_SOURCES})
```

### 4.2 Test Configuration

Add to `tests/cmake/JSONSchemaTests.cmake`:

```cmake
function(add_json_schema_tests)
    # Unit tests
    add_executable(json_schema_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-schema/JSONSchemaBasicTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-schema/JSONSchemaKeywordTests.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/json-query/json-schema/JSONSchemaRefTests.cpp)
    
    target_link_libraries(json_schema_tests PRIVATE json_query GTest::gtest_main)
    gtest_discover_tests(json_schema_tests)
    
    # Official JSON Schema Test Suite compliance
    add_executable(json_schema_compliance_tests
        ${CMAKE_CURRENT_SOURCE_DIR}/rfc-compliance-suite/JSONSchemaTestSuiteRunner.cpp)
    
    target_link_libraries(json_schema_compliance_tests PRIVATE json_query GTest::gtest_main)
    
    # Copy test suite JSON files
    target_compile_definitions(json_schema_compliance_tests PRIVATE
        JSON_SCHEMA_TEST_SUITE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/rfc-compliance-suite/json-schema-test-suite")
    
    gtest_discover_tests(json_schema_compliance_tests)
endfunction()
```

Add option to `tests/CMakeLists.txt`:

```cmake
option(JSON_QUERY_ENABLE_SCHEMA_TESTS
       "Build JSON Schema validation tests" ON)

if(JSON_QUERY_ENABLE_SCHEMA_TESTS)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/JSONSchemaTests.cmake)
    add_json_schema_tests()
endif()
```

### 4.3 Test Suite Submodule

Add official JSON Schema Test Suite:

```bash
git submodule add https://github.com/json-schema-org/JSON-Schema-Test-Suite.git \
    tests/rfc-compliance-suite/json-schema-test-suite
```

Update `.gitmodules`:

```ini
[submodule "tests/rfc-compliance-suite/json-schema-test-suite"]
    path = tests/rfc-compliance-suite/json-schema-test-suite
    url = https://github.com/json-schema-org/JSON-Schema-Test-Suite.git
    branch = main
```

---

## 5. Usage Examples

### 5.1 Basic Validation

```cpp
#include <json-query/JSONQuery>  // Unified header
#include <QJsonDocument>
#include <QDebug>

using namespace json_query::json_schema;

int main()
{
    // Define schema
    QJsonObject schemaObj = QJsonDocument::fromJson(R"({
        "type": "object",
        "properties": {
            "name": { "type": "string", "minLength": 1 },
            "age": { "type": "integer", "minimum": 0 }
        },
        "required": ["name"]
    })").object();
    
    // Compile schema (once)
    auto schemaResult = JSONSchema::create(schemaObj);
    if (!schemaResult) {
        qWarning() << "Schema error:" << to_qt_sv(schemaResult.error());
        return 1;
    }
    const JSONSchema& schema = *schemaResult;
    
    // Validate instance
    QJsonObject instance = {{"name", "Alice"}, {"age", 30}};
    
    ValidationResult result = schema.validate(instance);
    if (result) {
        qDebug() << "Valid!";
    } else {
        for (const auto& error : result.errors()) {
            qDebug() << "Error at" << error.instanceLocation 
                     << ":" << error.message;
        }
    }
    
    return 0;
}
```

### 5.2 Combined with JSONPath Queries

```cpp
#include <json-query/JSONQuery>

using namespace json_query;

// Validate a specific portion of a document
void validateSubDocument(const QJsonDocument& doc)
{
    // Extract nested data with JSONPath
    auto pathResult = json_path::JSONPath::create("$.users[*]");
    if (!pathResult) return;
    
    auto usersResult = pathResult->evaluateAll(doc);
    if (!usersResult) return;
    
    // Validate each user against schema
    auto schemaResult = json_schema::JSONSchema::create(userSchemaObj);
    if (!schemaResult) return;
    
    for (const QJsonValue& user : *usersResult) {
        auto validation = schemaResult->validate(user);
        if (!validation) {
            qWarning() << "Invalid user:" << validation.toString();
        }
    }
}
```

### 5.3 Error Location with JSONPointer

```cpp
// When validation fails, use JSONPointer to access the problematic value
void handleValidationError(const QJsonDocument& doc, 
                           const json_schema::ValidationError& error)
{
    // error.instanceLocation is a JSON Pointer string
    auto pointer = json_pointer::JSONPointer::create(error.instanceLocation);
    if (pointer) {
        auto valueResult = pointer->evaluate(doc);
        if (valueResult) {
            qDebug() << "Invalid value:" << *valueResult;
            qDebug() << "Error:" << error.message;
        }
    }
}
```

---

## 6. Implementation Phases

### Phase 1: Core Infrastructure (2-3 weeks)

- [ ] Create directory structure and CMake integration
- [ ] Implement `JSONSchemaError.hpp` with error enums
- [ ] Extend `Error` with schema domains
- [ ] Implement basic `JSONSchema` class with factory pattern
- [ ] Schema parsing for primitive keywords: `type`, `enum`, `const`

### Phase 2: Object & Array Keywords (2-3 weeks)

- [ ] Object keywords: `properties`, `required`, `additionalProperties`
- [ ] Array keywords: `items`, `prefixItems`, `minItems`, `maxItems`
- [ ] String keywords: `minLength`, `maxLength`, `pattern`
- [ ] Numeric keywords: `minimum`, `maximum`, `multipleOf`

### Phase 3: Combinators & References (2-3 weeks)

- [ ] Combinators: `allOf`, `anyOf`, `oneOf`, `not`
- [ ] Conditional: `if`/`then`/`else`
- [ ] `$ref` resolution using JSONPointer
- [ ] `$defs`/`definitions` support

### Phase 4: Advanced Features (2-3 weeks)

- [ ] `unevaluatedProperties`, `unevaluatedItems`
- [ ] `$dynamicRef`, `$dynamicAnchor`
- [ ] `format` keyword with common validators
- [ ] `dependentRequired`, `dependentSchemas`

### Phase 5: Compliance & Performance (2-3 weeks)

- [ ] Integrate official JSON Schema Test Suite
- [ ] Achieve 100% pass rate for Draft 2020-12
- [ ] Performance profiling and optimization
- [ ] Documentation and examples

---

## 7. Performance Considerations

### 7.1 Compile-Once, Validate-Many

The compiled schema (`CompiledSchema`) is:

- **Immutable** after creation
- **Thread-safe** for concurrent validation
- **Cache-friendly** with flat node array (indices instead of pointers)

### 7.2 Optimizations

| Optimization | Implementation |
|--------------|----------------|
| **Regex caching** | Pre-compile all `pattern` regexes during schema compilation |
| **Property lookup** | Use `std::unordered_map` for O(1) property name lookup |
| **Short-circuit evaluation** | Stop `allOf` on first failure (configurable) |
| **Static unevaluated analysis** | Pre-compute which properties are always evaluated |
| **Implicit sharing** | Qt's JSON types avoid deep copies during traversal |

### 7.3 Memory Layout

```
CompiledSchema
├── nodes: vector<SchemaNode>     // Contiguous memory, cache-friendly
├── anchors: unordered_map        // Fast $ref lookup
└── metadata: QString             // Schema ID, dialect
```

---

## 8. Testing Strategy

### 8.1 Test Categories

| Category | Description | Location |
|----------|-------------|----------|
| **Unit tests** | Individual keyword validation | `tests/json-query/json-schema/` |
| **Integration tests** | Combined with JSONPath/Pointer | `tests/json-query/json-schema/` |
| **Compliance tests** | Official JSON Schema Test Suite | `tests/rfc-compliance-suite/` |
| **Fuzz tests** | Random schema/instance generation | `tests/fuzz/` |
| **Performance tests** | Benchmarks for large documents | `perf/` |

### 8.2 Compliance Test Runner

```cpp
// Parameterized test using official test suite JSON files
class JSONSchemaComplianceTest : public ::testing::TestWithParam<std::filesystem::path> {};

TEST_P(JSONSchemaComplianceTest, OfficialTestCase)
{
    auto testFile = GetParam();
    QFile file(QString::fromStdString(testFile.string()));
    file.open(QIODevice::ReadOnly);
    
    QJsonArray testGroups = QJsonDocument::fromJson(file.readAll()).array();
    
    for (const QJsonValue& group : testGroups) {
        QJsonObject schema = group["schema"].toObject();
        
        auto schemaResult = json_schema::JSONSchema::create(schema);
        ASSERT_TRUE(schemaResult.has_value()) << "Schema compilation failed";
        
        for (const QJsonValue& test : group["tests"].toArray()) {
            QJsonValue data = test["data"];
            bool expected = test["valid"].toBool();
            
            auto result = schemaResult->validate(data);
            EXPECT_EQ(result.isValid(), expected) 
                << "Test: " << test["description"].toString().toStdString();
        }
    }
}
```

---

## 9. Public API Summary

### Headers

```cpp
// Main public header (add to include/json-query/JSONQuery)
#include <json-query/json-schema/JSONSchema.hpp>
```

### Classes

| Class | Purpose |
|-------|---------|
| `JSONSchema` | Compiled schema, main entry point |
| `ValidationResult` | Validation outcome with error details |
| `ValidationError` | Single validation error with location |

### Factory Methods

```cpp
JSONSchema::create(const QJsonObject&)    → std::expected<JSONSchema, Error>
JSONSchema::create(const QJsonDocument&)  → std::expected<JSONSchema, Error>
JSONSchema::fromFile(const QString&)      → std::expected<JSONSchema, Error>
```

### Validation Methods

```cpp
schema.validate(const QJsonValue&)    → ValidationResult
schema.validate(const QJsonDocument&) → ValidationResult
schema.isValid(const QJsonValue&)     → bool  // Quick check, no error details
```

---

## 10. Conclusion

This integration plan extends qt-json-query with JSON Schema validation while maintaining consistency with the existing architecture:

- **Same patterns**: Factory creation with `std::expected`, unified error handling
- **Same quality**: RFC compliance testing, performance focus
- **Natural fit**: Reuses JSONPointer for `$ref` resolution and error locations
- **Incremental**: Phased implementation allows early testing and feedback

The result is a cohesive Qt JSON toolkit covering querying (JSONPath), direct access (JSONPointer), and validation (JSON Schema) — all with modern C++23 ergonomics and Qt-native APIs.
