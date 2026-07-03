# Fuzz Testing for qt-json-query

This directory contains LibFuzzer-based fuzz tests for comprehensive robustness testing of the qt-json-query library components.

## Overview

Fuzz testing helps discover edge cases, crashes, and security vulnerabilities by feeding randomly generated or mutated inputs to the library. Our fuzz tests target the most critical components of the qt-json-query library:

- **JSONPath expression parsing** (`fuzz_jsonpath_parsing`)
- **JSON Pointer parsing** (`fuzz_jsonpointer_parsing`)
- **Combined evaluation pipeline** (`fuzz_combined_evaluation`)
- **JSON Schema compile + validate** (`fuzz_jsonschema`)

## Requirements

- **Clang compiler** with LibFuzzer support (Clang 15+)
- **AddressSanitizer and UndefinedBehaviorSanitizer** support
- **CMake 3.21+** with preset support
- **Python 3.6+** for corpus setup (optional but recommended)

## Quick Start

### 1. Enable Fuzz Testing

```bash
# Configure with fuzz testing enabled
cmake --preset debug-qt -DENABLE_FUZZ_TESTS=ON

# Build the fuzz targets
cmake --build --preset debug-qt
```

### 2. Set Up Seed Corpus (Recommended)

```bash
# Run the corpus setup script
cd tests/fuzz
python3 setup_corpus.py --build-dir ../../build
```

### 3. Run Fuzz Tests

```bash
# Quick test (30 seconds each fuzzer)
make fuzz_quick

# Full test (5 minutes each fuzzer)  
make run_all_fuzzers

# Run individual fuzzers
make run_fuzz_jsonpath_parsing
make run_fuzz_jsonpointer_parsing
make run_fuzz_combined_evaluation
make run_fuzz_jsonschema
```

## Fuzz Targets

### 1. JSONPath Parsing (`fuzz_jsonpath_parsing`)

**Purpose**: Tests robustness of `JSONPath::create()` with malformed expressions.

**Focus Areas**:
- Parser error handling with invalid syntax
- Edge cases in filter expressions
- Unicode and special character handling
- Memory safety during parsing

**Example Issues It May Find**:
- Crashes on deeply nested filter expressions
- Buffer overflows in string parsing
- Infinite loops in recursive descent parsing

### 2. JSON Pointer Parsing (`fuzz_jsonpointer_parsing`)

**Purpose**: Tests robustness of `JSONPointer::create()` according to RFC 6901.

**Focus Areas**:
- RFC 6901 compliance with edge cases
- Escape sequence handling (`~0`, `~1`)
- Unicode path components
- Boundary conditions in array indices

**Example Issues It May Find**:
- Incorrect escape sequence processing
- Integer overflow in array index parsing
- Memory corruption with malformed pointers

### 3. Combined Evaluation (`fuzz_combined_evaluation`)

**Purpose**: Tests the complete evaluation pipeline with both fuzzed JSONPath expressions and JSON documents.

**Focus Areas**:
- Type mismatches between path and document
- Deep nesting scenarios
- Large array/object handling
- Filter evaluation edge cases

**Example Issues It May Find**:
- Stack overflow in recursive evaluation
- Type confusion in filter predicates
- Performance issues with pathological inputs

### 4. JSON Schema (`fuzz_jsonschema`)

**Purpose**: Tests schema compilation and instance validation with fuzzed
schema documents and fuzzed instances, under both lenient and strict
(`FormatValidation::Assertion` + `UnresolvedRefPolicy::Fail`) options.

**Focus Areas**:
- Keyword compilation with malformed schema values
- `$ref`/`$dynamicRef` resolution, anchors, and the ref-cycle guard
- Regex compilation in `pattern`/`patternProperties`
- All keyword validators and error-reporting paths

**Example Issues It May Find**:
- Stack overflow via reference cycles
- Crashes on malformed keyword values
- Memory errors in validation error collection

## Seed Corpus

The fuzz tests use carefully crafted seed inputs to improve fuzzing effectiveness:

### JSONPath Seeds (`corpus/jsonpath_seeds.txt`)
- Basic expressions (`$.store.book[0]`)
- Complex filters (`$.store.book[?(@.price > 10)]`)
- Edge cases and malformed expressions
- Unicode and special characters

### JSON Pointer Seeds (`corpus/jsonpointer_seeds.txt`)
- RFC 6901 compliant pointers (`/store/book/0`)
- Escape sequences (`/~0`, `/~1`)
- Edge cases and boundary conditions
- Unicode path components

### JSON Document Seeds (`corpus/json_document_seeds.txt`)
- Various JSON structures (objects, arrays, primitives)
- Edge cases (empty keys, unicode, deep nesting)
- Real-world-like documents
- Malformed JSON for parser testing

### JSON Schema Seeds (`corpus/jsonschema_seeds.txt`)
- Schemas exercising each keyword family (type, numeric, string, array,
  object, combinators, conditionals)
- `$ref`/`$defs`/`$anchor`/`$dynamicRef` shapes including cycles
- Format validators and pattern regexes

## Configuration

### CMake Options

```cmake
# Enable fuzz testing
-DENABLE_FUZZ_TESTS=ON

# Fuzz tests are automatically configured with:
# - LibFuzzer instrumentation (-fsanitize=fuzzer)
# - AddressSanitizer (-fsanitize=address)
# - UndefinedBehaviorSanitizer (-fsanitize=undefined)
# - Light optimization (-O1) for better fuzzing performance
```

### Runtime Options

Each fuzzer accepts LibFuzzer command-line options:

```bash
# Run for specific time
./fuzz_jsonpath_parsing -max_total_time=300

# Limit memory usage
./fuzz_jsonpath_parsing -rss_limit_mb=1024

# Use specific corpus directory
./fuzz_jsonpath_parsing /path/to/corpus

# Generate coverage report
./fuzz_jsonpath_parsing -print_coverage=1

# Minimize test cases
./fuzz_jsonpath_parsing -minimize_crash=1
```

## Interpreting Results

### Successful Run
```
INFO: Seed: 1234567890
INFO: Loaded 1 modules   (12345 inline 8-bit counters): 12345 [0x..., 0x...)
INFO: Loaded 1 PC tables (12345 PCs): 12345 [0x...,0x...)
INFO: -max_total_time=30 seconds
INFO: A corpus is not provided, starting from an empty corpus
#1      INITED cov: 123 ft: 456 corp: 1/1b exec/s: 0 rss: 45Mb
#100    NEW    cov: 234 ft: 567 corp: 2/10b lim: 4 exec/s: 0 rss: 46Mb
...
Done 12345 runs in 30 second(s)
```

### Crash Found
```
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x...
    #0 0x... in JSONPath::parse() src/JSONPath.cpp:123
    #1 0x... in LLVMFuzzerTestOneInput fuzz_jsonpath_parsing.cpp:45
    
artifact_prefix='./'; Test unit written to ./crash-...
```

When a crash is found:
1. The crashing input is saved as `crash-<hash>`
2. Review the stack trace to identify the issue
3. Create a minimal test case to reproduce the bug
4. Fix the underlying issue in the library

## Integration with CI/CD

### GitHub Actions Example

```yaml
- name: Fuzz Testing
  if: matrix.compiler == 'clang'
  run: |
    cmake --preset debug-qt -DENABLE_FUZZ_TESTS=ON
    cmake --build --preset debug-qt
    
    # Set up corpus
    cd tests/fuzz
    python3 setup_corpus.py --build-dir ../../build
    cd ../../
    
    # Run quick fuzz tests
    make fuzz_quick
    
    # Check for crashes
    if find build/fuzz -name "crash-*" | grep -q .; then
      echo "Fuzzing found crashes!"
      exit 1
    fi
```

## Best Practices

1. **Run Regularly**: Include fuzz testing in CI/CD pipeline
2. **Monitor Coverage**: Use `-print_coverage=1` to track code coverage
3. **Minimize Crashes**: Use `-minimize_crash=1` to create minimal test cases
4. **Update Seeds**: Add new seed cases when adding features
5. **Long Runs**: Run overnight fuzzing sessions for thorough testing
6. **Multiple Sanitizers**: Test with different sanitizer combinations

## Troubleshooting

### Build Issues

**Problem**: `fatal error: 'fuzzer/FuzzedDataProvider.h' not found`
**Solution**: Ensure you're using Clang with LibFuzzer support

**Problem**: `undefined reference to '__sanitizer_cov_trace_pc_guard'`
**Solution**: Make sure `-fsanitize=fuzzer` is in both compile and link flags

### Runtime Issues

**Problem**: Fuzzer exits immediately with no iterations
**Solution**: Check that the target binary was built with fuzzing instrumentation

**Problem**: Very slow fuzzing performance
**Solution**: Use `-O1` optimization and ensure sanitizers are properly configured

**Problem**: Out of memory errors
**Solution**: Use `-rss_limit_mb=<limit>` to constrain memory usage

## Contributing

When adding new features to qt-json-query:

1. **Update Fuzz Targets**: Modify existing fuzzers to test new code paths
2. **Add Seed Cases**: Include relevant seed inputs for new functionality  
3. **Create New Fuzzers**: Add specialized fuzzers for complex new features
4. **Document Changes**: Update this README with new fuzzing strategies

## References

- [LibFuzzer Documentation](https://llvm.org/docs/LibFuzzer.html)
- [AddressSanitizer](https://clang.llvm.org/docs/AddressSanitizer.html)
- [UndefinedBehaviorSanitizer](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)
- [RFC 9535 JSONPath](https://tools.ietf.org/rfc/rfc9535.txt)
- [RFC 6901 JSON Pointer](https://tools.ietf.org/rfc/rfc6901.txt)
