#!/bin/bash

# Diagnostic Comprehensive Brace Modernization Script
# Systematically applies brace initialization modernization to all C++ files
# Validates build after each batch to ensure stability

set -e

PROJECT_ROOT="/Users/fischi/dev/github/natyamatsya/qt-json-query"
MODERNIZER="$PROJECT_ROOT/scripts/comprehensive_brace_modernizer"
BUILD_DIR="$PROJECT_ROOT/build-relwithdebinfo-apple-clang"

cd "$PROJECT_ROOT"

echo "=== Diagnostic Comprehensive Brace Modernization ==="
echo "Project: qt-json-query"
echo "Tool: $MODERNIZER"
echo "Build: $BUILD_DIR"
echo

# Function to build and test
build_and_test() {
    echo "Building project..."
    if ! cmake --build "$BUILD_DIR" --parallel; then
        echo "❌ Build failed!"
        return 1
    fi
    
    echo "Running RFC 9535 compliance tests..."
    if ! "$BUILD_DIR/tests/rfc9535_tests" > /dev/null 2>&1; then
        echo "❌ RFC 9535 tests failed!"
        return 1
    fi
    
    echo "✅ Build and tests successful"
    return 0
}

# Function to process files in a batch
process_batch() {
    local batch_name="$1"
    shift
    local files=("$@")
    
    echo "=== Processing batch: $batch_name ==="
    echo "Files in batch: ${#files[@]}"
    
    local modified_files=()
    
    for file in "${files[@]}"; do
        if [[ -f "$file" ]]; then
            echo "Processing: $file"
            if "$MODERNIZER" "$file"; then
                modified_files+=("$file")
                echo "✅ Modified: $file"
            else
                echo "ℹ️  No changes: $file"
            fi
        else
            echo "⚠️  File not found: $file"
        fi
    done
    
    if [[ ${#modified_files[@]} -eq 0 ]]; then
        echo "ℹ️  No files modified in batch: $batch_name"
        return 0
    fi
    
    echo
    echo "Modified files in batch $batch_name:"
    printf '  %s\n' "${modified_files[@]}"
    echo
    
    echo "Validating build after batch: $batch_name"
    if build_and_test; then
        echo "✅ Batch $batch_name completed successfully"
        return 0
    else
        echo "❌ Build failed after batch: $batch_name"
        echo "Modified files that may have caused issues:"
        printf '  %s\n' "${modified_files[@]}"
        echo
        echo "STOPPING: Manual intervention required"
        echo "The script will not revert changes automatically."
        echo "Please review and fix the build issues manually."
        return 1
    fi
}

# Initial build validation
echo "=== Initial Build Validation ==="
if ! build_and_test; then
    echo "❌ Initial build failed. Please fix build issues before running modernization."
    exit 1
fi

echo
echo "=== Starting Comprehensive Brace Modernization ==="

# Batch 1: Examples (safest to start with)
examples_files=(
    "examples/simple_path.cpp"
    "examples/simple_pointer.cpp"
    "examples/complex_query.cpp"
    "examples/function_ref_demo.cpp"
    "examples/monadic_error_handling_demo.cpp"
    "examples/refactor_potential.cpp"
)

if ! process_batch "Examples" "${examples_files[@]}"; then
    exit 1
fi

# Batch 2: Debug and compliance tools
debug_files=(
    "debug/src/debug_cts_root_test.cpp"
    "compliance/src/rfc9535_terminology_audit.cpp"
    "compliance/src/error_audit_script.cpp"
    "compliance/src/cts_error_cross_reference.cpp"
)

if ! process_batch "Debug and Compliance" "${debug_files[@]}"; then
    exit 1
fi

# Batch 3: Performance tools
perf_files=(
    "perf/src/memory_allocation_test.cpp"
    "perf/src/memory_optimization_test.cpp"
    "perf/src/qt_memory_tracker.cpp"
    "perf/src/allocation_hotspot_analyzer.cpp"
    "perf/src/profile_test.cpp"
    "perf/src/cache_analysis_tool.cpp"
)

if ! process_batch "Performance Tools" "${perf_files[@]}"; then
    exit 1
fi

# Batch 4: JSONPointer and utility files
utility_files=(
    "src/json-query/json-pointer/JSONPointer.cpp"
    "src/json-query/json-path/JSONPathLog.cpp"
    "src/json-query/json-path/internal/CacheOptimizedStructures.cpp"
    "src/json-query/json-path/internal/IterativeRecursiveDescent.cpp"
)

if ! process_batch "Utility Files" "${utility_files[@]}"; then
    exit 1
fi

# Batch 5: Core source files (JSONPath evaluation)
core_eval_files=(
    "src/json-query/json-path/JSONPathEvaluate.cpp"
    "src/json-query/json-path/JSONPathEvalHelpers.cpp"
    "src/json-query/json-path/JSONPathTokenEvaluators.cpp"
    "src/json-query/json-path/JSONPathWildcardRecursive.cpp"
    "src/json-query/json-path/JSONPathExpected.cpp"
)

if ! process_batch "Core Evaluation" "${core_eval_files[@]}"; then
    exit 1
fi

# Batch 6: Core source files (JSONPath parsing and filtering)
core_parse_files=(
    "src/json-query/json-path/JSONPathFilterCore.cpp"
    "src/json-query/json-path/JSONPathFilterParsers.cpp"
    "src/json-query/json-path/JSONPathFilterHelpers.cpp"
    "src/json-query/json-path/JSONPathFilterFunctions.cpp"
    "src/json-query/json-path/JSONPathFilterComparison.cpp"
    "src/json-query/json-path/JSONPathContextFilter.cpp"
)

if ! process_batch "Core Parsing and Filtering" "${core_parse_files[@]}"; then
    exit 1
fi

# Batch 7: Remaining core source files
remaining_core_files=(
    "src/json-query/json-path/JSONPath.cpp"
    "src/json-query/json-path/JSONPathCreate.cpp"
    "src/json-query/json-path/JSONPathCompile.cpp"
    "src/json-query/json-path/JSONPathParsers.cpp"
    "src/json-query/json-path/JSONPathParseUtils.cpp"
    "src/json-query/json-path/JSONPathBracketRules.cpp"
    "src/json-query/json-path/JSONPathTokenDispatch.cpp"
    "src/json-query/json-path/JSONPathPointerConversion.cpp"
)

if ! process_batch "Remaining Core Source" "${remaining_core_files[@]}"; then
    exit 1
fi

# Batch 8: Header files (most critical)
header_files=(
    "include/json-query/json-path/JSONPath.hpp"
    "include/json-query/json-path/JSONPathFilterParsers.hpp"
    "include/json-query/json-path/internal/ArrayPool.hpp"
    "include/json-query/json-path/internal/ArenaAllocator.hpp"
    "include/json-query/json-path/internal/CacheOptimizedStructures.hpp"
    "include/json-query/json-path/internal/IterativeRecursiveDescent.hpp"
)

if ! process_batch "Header Files" "${header_files[@]}"; then
    exit 1
fi

echo
echo "=== Final Validation ==="
if build_and_test; then
    echo "🎉 Comprehensive Brace Modernization completed successfully!"
    echo "All batches processed and validated."
    echo
    echo "Summary:"
    echo "- All C++ files processed for comprehensive brace initialization modernization"
    echo "- Build stability maintained throughout"
    echo "- RFC 9535 compliance preserved"
    echo "- Variable initializations modernized to use brace initialization {}"
else
    echo "❌ Final validation failed"
    exit 1
fi
