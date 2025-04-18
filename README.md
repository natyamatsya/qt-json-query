# JSON Query for Qt

A high-performance implementation of JSONPointer and JSONPath for Qt, featuring:

- Full RFC 6901 JSONPointer compliance
- Standard-compliant JSONPath implementation
- Compile-time regular expressions (CTRE) for maximum performance
- Seamless integration between JSONPointer and JSONPath

## Features

### JSONPointer (RFC 6901)

- Direct access to JSON elements using path notation
- Support for nested objects and arrays
- Proper escape sequence handling
- Robust error handling

### JSONPath

- All standard JSONPath features:
  - Direct property access (dot and bracket notation)
  - Array access by index
  - Array slices with start, end, and step
  - Wildcards for properties and arrays
  - Recursive descent (`..`)
  - Filter expressions

### Performance Optimizations

- Uses compile-time regular expressions (CTRE) for parsing
- Integration with JSONPointer for efficient direct path evaluation
- Zero allocation for common path patterns
- Optimized internal data structures

## Requirements

- Qt 6.5.7 or newer
- C++17 compatible compiler
- CTRE library (included as submodule)

## Installation

1. Clone the repository:

```bash
git clone https://github.com/yourusername/json-query-qt.git
cd json-query-qt
git submodule update --init --recursive  # To fetch CTRE
```

2. Include in your project:

```qmake
include(path/to/json-query-qt/json-query.pri)
```

Or add the source files directly to your project.

## Usage

### JSONPointer Basic Usage

```cpp
#include "JSONPointer.h"

// Create a JSON document
QJsonObject obj{{"foo", QJsonObject{{"bar", 42}}}};
QJsonDocument doc(obj);

// Create and evaluate a JSONPointer
JSONPointer pointer("/foo/bar");
QJsonValue result = pointer.evaluate(doc);  // Returns 42
```

### JSONPath Basic Usage

```cpp
#include "JSONPath.h"

// Create a JSON document with an array
QJsonArray books = QJsonArray::fromVariantList({
    QJsonObject{{"title", "Book 1"}, {"author", "Author 1"}},
    QJsonObject{{"title", "Book 2"}, {"author", "Author 2"}}
});
QJsonObject store{{"books", books}};
QJsonDocument doc(store);

// Get all book titles
JSONPath path("$.books[*].title");
QJsonArray titles = path.evaluate(doc);  // Returns ["Book 1", "Book 2"]

// Get books with complex criteria
JSONPath filter("$.books[?(@.author == 'Author 1')]");
QJsonArray filteredBooks = filter.evaluate(doc);
```

## Performance Comparison

The implementation uses compile-time regular expressions (CTRE) for significant performance improvements:

| Operation | Processing Time (10,000 iterations) |
|-----------|-------------------------------------|
| JSONPointer (with CTRE) | ~XX ms |
| JSONPath (with CTRE) | ~XX ms |
| Standard Qt regex (for comparison) | ~XX ms |

## License

This project is licensed under the MIT License - see the LICENSE file for details.
