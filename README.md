# JSON Query for Qt (C++23 Edition)

A high-performance, modern C++ implementation of JSONPointer (RFC 6901), JSONPath (RFC 9535), and JSON Schema validation for Qt.

**Features:**

- Full **RFC 6901 JSONPointer** compliance.
- Support for common **JSONPath** features for querying JSON structures.
- **JSON Schema (Draft 2020-12)** validation with comprehensive format support.
- Utilizes **Compile-Time Regular Expressions (CTRE)** for efficient parsing.
- Robust error handling using **`std::expected`** (C++23) for object creation/parsing.
- Modern C++23 design, prioritizing standard library types (`std::vector`, `std::string`, etc.) internally.
- Clean integration with Qt's JSON types (`QJsonValue`, `QJsonDocument`, etc.) at the API boundary.

## Detailed Features

### JSONPointer (RFC 6901)

- Direct access to JSON elements using pointer notation (e.g., `/foo/0/bar`).
- Support for nested objects and arrays.
- Correct handling of escape sequences (`~0` for `~`, `~1` for `/`).
- **Robust Error Handling:** Uses `std::expected` for:
  - **Creation/Parsing:** Indicates syntax errors via `JsonPointerParseError`.
  - **Evaluation:** Indicates errors like key-not-found or invalid-index via `JsonPointerError`.

### JSONPath

- Standard JSONPath query features:
  - Root object access (`$`).
  - Direct property access (dot `.` and bracket `['...']` notation).
  - Array access by index (`[index]`, including negative indices).
  - Array slices (`[start:end:step]`, including defaults and negative indices/steps).
  - Wildcards for properties (`.*`) and array elements (`[*]`).
  - Recursive descent (`..`) to search deeply.
  - Basic filter expressions (`[?(@.property == value)]`, `[?(@.age > 30)]`, `[?(@.name)]`).
- **Robust Parsing:** Uses `std::expected` to report syntax errors during creation via `JsonPathParseError`.
- **Evaluation Results:** Returns a `QJsonArray` containing all matched values (empty array if no matches found).

### JSON Schema (Draft 2020-12)

- **Full Draft 2020-12 compliance** (1994/1994 test suite passing).
- Supported keywords:
  - **Type validation:** `type`, `enum`, `const`
  - **String:** `minLength`, `maxLength`, `pattern`, `format`
  - **Numeric:** `minimum`, `maximum`, `exclusiveMinimum`, `exclusiveMaximum`, `multipleOf`
  - **Array:** `minItems`, `maxItems`, `uniqueItems`, `prefixItems`, `items`, `contains`
  - **Object:** `properties`, `patternProperties`, `additionalProperties`, `required`, `propertyNames`, `minProperties`, `maxProperties`, `dependentRequired`, `dependentSchemas`
  - **Combinators:** `allOf`, `anyOf`, `oneOf`, `not`, `if`/`then`/`else`
  - **References:** `$ref`, `$defs`, `$anchor`, `$id`
- **Format validation** with CTRE + Qt semantic checks:
  - Date/time: `date-time`, `date`, `time`
  - Network: `email`, `hostname`, `ipv4`, `ipv6`, `uri`, `uri-reference`, `uri-template`
  - Other: `uuid`, `json-pointer`, `relative-json-pointer`, `regex`
- **Detailed error reporting:** Instance path, schema path, error code, and message for each violation.
- **Two validation modes:** Collect all errors or stop on first error.

### Performance & Design

- Uses **compile-time regular expressions (CTRE)** via Hana Dusíková's library for fast path parsing where applicable.
- Internal implementation prioritizes standard C++ types (`std::string`, `std::vector`, `std::variant`, `std::optional`, `std::tuple`) for efficiency and portability.
- Minimized allocations where possible during parsing and evaluation.
- Consistent C++23 features (e.g., brace initialization, `const` correctness).

## Requirements

- **C++23 compatible compiler:**
  - GCC 13 or newer
  - Clang 16 or newer
  - MSVC VS 2022 v17.8 or newer (ensure `/std:c++latest` is enabled)
- **Qt 6.7 or newer** (Recommended for better C++23 integration, though core features might work with 6.5+ depending on compiler).
- **CTRE library:** Included as a submodule. Ensure you initialize and update submodules.

## Installation

1. Clone the repository:

    ```bash
    git clone [https://github.com/yourusername/json-query-qt.git](https://github.com/yourusername/json-query-qt.git)
    cd json-query-qt
    # Initialize and fetch the CTRE submodule
    git submodule update --init --recursive
    ```

2. Include in your CMake project:

    ```cmake
    # Add the subdirectory containing json-query-qt's CMakeLists.txt
    add_subdirectory(path/to/json-query-qt)

    # Link the library to your target
    target_link_libraries(your_target PRIVATE json-query-qt)
    ```

    *(A basic `CMakeLists.txt` would need to be provided in the library for this to work).*

    **Alternatively (Manual):**
    Add the `.h` and `.cpp` files directly to your existing build system, ensuring the CTRE include path is correctly configured and C++23 is enabled.

## Usage

### JSONPointer (C++23 with `std::expected`)

```cpp
#include "JSONPointer.hpp"
#include "JSONQueryUtils.hpp" // For conversions if needed elsewhere
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <expected>

// --- Create Document ---
QJsonObject obj{{"foo", QJsonObject{{"bar", 42}, {"baz", "hello"}}}};
QJsonDocument doc(obj);

// --- Create Pointer (Handles Parsing Errors) ---
auto pointer_exp = JSONPointer::create("/foo/bar");

if (!pointer_exp) {
    // Handle parsing error
    qWarning() << "Failed to parse JSON Pointer:"
               << utils::std_string_to_qstring(pointer_exp.error().message);
    return;
}
const JSONPointer pointer = pointer_exp.value(); // or *pointer_exp

// --- Evaluate Pointer (Handles Evaluation Errors) ---
auto eval_result = pointer.evaluate(doc);

if (eval_result) {
    // Evaluation successful
    QJsonValue value = eval_result.value(); // or *eval_result
    qDebug() << "Pointer result:" << value; // Output: QJsonValue(double, 42)
} else {
    // Handle evaluation error
    JsonPointerError error_code = eval_result.error();
    qWarning() << "Failed to evaluate JSON Pointer, error code:" << static_cast<int>(error_code);
    // Example: KeyNotFound, IndexOutOfBounds, TypeMismatch
}

// --- Example: Non-existent path ---
auto pointer_nonexist_exp = JSONPointer::create("/foo/qux");
if (pointer_nonexist_exp) {
    auto eval_nonexist = pointer_nonexist_exp->evaluate(doc);
    if (!eval_nonexist) {
         qDebug() << "Evaluation failed as expected for non-existent path. Error:"
                  << static_cast<int>(eval_nonexist.error()); // Likely KeyNotFound
    }
}

// --- Example: Invalid Syntax ---
auto pointer_invalid_exp = JSONPointer::create("foo/bar"); // Missing leading '/'
if (!pointer_invalid_exp) {
     qDebug() << "Pointer creation failed as expected for invalid syntax. Error:"
              << utils::std_string_to_qstring(pointer_invalid_exp.error().message);
}
JSONPath (C++23 with std::expected for creation)

#include "JSONPath.h"
#include "JSONQueryUtils.hpp" // For conversions if needed elsewhere
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <expected>

// --- Create Document ---
QJsonArray books{};
books.append(QJsonObject{{"title", "Book 1"}, {"author", "Author 1"}, {"price", 10.0}});
books.append(QJsonObject{{"title", "Book 2"}, {"author", "Author 2"}, {"price", 25.5}});
books.append(QJsonObject{{"title", "Book 3"}, {"author", "Author 1"}, {"price", 15.0}});
QJsonObject store{{"books", books}};
QJsonDocument doc(store);

// --- Create Path (Handles Parsing Errors) ---
auto path_exp = JSONPath::create("$.books[*].title");

if (!path_exp) {
    // Handle parsing error
    qWarning() << "Failed to parse JSON Path:"
               << utils::std_string_to_qstring(path_exp.error().message);
    return;
}
const JSONPath path = path_exp.value(); // or *path_exp

// --- Evaluate Path ---
// Evaluate returns QJsonArray directly (empty if no matches)
QJsonArray titles = path.evaluate(doc);
qDebug() << "All book titles:" << titles; // Output: QJsonArray(["Book 1", "Book 2", "Book 3"])

// --- Example: Filter Path ---
auto filter_path_exp = JSONPath::create("$.books[?(@.price > 12)].author");
if (!filter_path_exp) {
     qWarning() << "Failed to parse filter path:"
                << utils::std_string_to_qstring(filter_path_exp.error().message);
     return;
}
const JSONPath filter_path = *filter_path_exp; // Alternate access via operator*

QJsonArray expensive_authors = filter_path.evaluate(doc);
qDebug() << "Authors of expensive books:" << expensive_authors; // Output: QJsonArray(["Author 2", "Author 1"])

// --- Example: Invalid Syntax ---
auto invalid_path_exp = JSONPath::create("$[1:2:0]"); // Invalid step
if (!invalid_path_exp) {
    qDebug() << "Path creation failed as expected for invalid syntax. Error:"
             << utils::std_string_to_qstring(invalid_path_exp.error().message);
}
```

### JSON Schema Validation

```cpp
#include "json-query/json-schema/JSONSchema.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

using namespace json_query::json_schema;

// --- Define Schema ---
const QJsonObject schemaObj{
    {"type", "object"},
    {"properties", QJsonObject{
        {"name", QJsonObject{{"type", "string"}, {"minLength", 1}}},
        {"age", QJsonObject{{"type", "integer"}, {"minimum", 0}}},
        {"email", QJsonObject{{"type", "string"}, {"format", "email"}}}
    }},
    {"required", QJsonArray{"name", "age"}}
};

// --- Monadic Style: Compile and Validate in One Expression ---
const QJsonObject instance{{"name", "Alice"}, {"age", 30}, {"email", "alice@example.com"}};

const auto result{JSONSchema::create(schemaObj).transform([&](const JSONSchema& schema) {
    return schema.validate(instance);
})};

if (result && result->isValid()) {
    qDebug() << "Instance is valid!";
}

// --- Monadic Chain with Error Handling ---
JSONSchema::create(schemaObj)
    .transform([&](const JSONSchema& schema) { return schema.validate(instance); })
    .transform([&](const ValidationResult& r) {
        for (const auto& err : r.errors()) {
            qDebug() << err.instanceLocation << "->" << err.message;
            
            // Navigate directly to the failing value using JSONPointer
            if (auto failingValue = err.navigateTo(instance))
                qDebug() << "  Value was:" << *failingValue;
        }
        return r.isValid();
    });

// --- Quick Validation (stops on first error) ---
const auto isValid{JSONSchema::create(schemaObj).transform([&](const JSONSchema& s) {
    return s.isValid(instance);
})};

if (isValid.value_or(false))
    qDebug() << "Valid!";
```

## Performance Comparison

This implementation leverages compile-time regular expressions (CTRE) for parsing, which generally offers significantly better performance compared to runtime regex engines like QRegularExpression for the patterns used in path segmentation. Actual performance gains depend on the complexity of the paths and the specific operations. Benchmarking against other libraries or approaches is recommended for specific use cases.
