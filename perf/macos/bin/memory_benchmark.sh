#!/bin/bash

# Memory allocation tracking script for JSONPath performance analysis
# Uses system-level tools available on macOS

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_ROOT/build-relwithdebinfo"

echo "=== JSONPath Memory Allocation Tracking ==="
echo "Using system-level memory profiling tools"
echo ""

# Ensure build directory exists
if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory not found: $BUILD_DIR"
    exit 1
fi

cd "$BUILD_DIR"

# Function to run memory analysis with different approaches
run_memory_analysis() {
    local test_name="$1"
    local executable="$2"

    echo "--- Memory Analysis: $test_name ---"

    # Method 1: Use time command to get memory usage
    echo "Method 1: System resource usage"
    /usr/bin/time -l "$executable" 2>&1 | grep -E "(maximum resident set size|peak memory footprint)"

    # Method 2: Use leaks command (macOS specific)
    echo "Method 2: Memory leaks detection"
    leaks --atExit -- "$executable" 2>/dev/null | grep -E "(leaks for|total leaked bytes)" || echo "No leaks detected"

    # Method 3: Use heap profiling with gperftools if available
    if command -v pprof >/dev/null 2>&1; then
        echo "Method 3: Heap profiling with gperftools"
        QT_LOGGING_RULES="*=false" HEAPPROFILE=/tmp/heap_profile_$test_name "$executable" >/dev/null 2>&1 || true

        # Check if heap profile was created
        if ls /tmp/heap_profile_$test_name.* >/dev/null 2>&1; then
            echo "Heap profile created: /tmp/heap_profile_$test_name.*"
            # Get the latest heap profile
            latest_profile=$(ls -t /tmp/heap_profile_$test_name.* | head -1)
            echo "Analyzing heap profile: $latest_profile"
            pprof --text "$executable" "$latest_profile" 2>/dev/null | head -10 || echo "Heap analysis not available"
        else
            echo "Heap profiling not available (gperftools not linked)"
        fi
    fi

    echo ""
}

# Method 4: Custom memory tracking with malloc interposition
create_malloc_tracker() {
    cat > /tmp/malloc_tracker.c << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>

static void* (*real_malloc)(size_t) = NULL;
static void (*real_free)(void*) = NULL;
static size_t total_allocated = 0;
static size_t allocation_count = 0;

static void init_hooks() {
    if (!real_malloc) {
        real_malloc = dlsym(RTLD_NEXT, "malloc");
        real_free = dlsym(RTLD_NEXT, "free");
    }
}

void* malloc(size_t size) {
    init_hooks();
    void* ptr = real_malloc(size);
    if (ptr) {
        total_allocated += size;
        allocation_count++;
        if (allocation_count % 1000 == 0) {
            fprintf(stderr, "Allocations: %zu, Total: %zu bytes\n", allocation_count, total_allocated);
        }
    }
    return ptr;
}

void free(void* ptr) {
    init_hooks();
    if (ptr) {
        real_free(ptr);
    }
}

__attribute__((destructor))
void cleanup() {
    fprintf(stderr, "Final stats - Allocations: %zu, Total allocated: %zu bytes\n", allocation_count, total_allocated);
}
EOF

    # Compile the malloc tracker
    if cc -shared -fPIC -o /tmp/malloc_tracker.dylib /tmp/malloc_tracker.c -ldl 2>/dev/null; then
        echo "Method 4: Custom malloc tracking library created"
        return 0
    else
        echo "Method 4: Failed to create malloc tracking library"
        return 1
    fi
}

# Run memory analysis on different executables
echo "Building memory tracking library..."
if create_malloc_tracker; then
    echo "Custom malloc tracker available"
    MALLOC_TRACKER_AVAILABLE=1
else
    echo "Custom malloc tracker not available"
    MALLOC_TRACKER_AVAILABLE=0
fi

echo ""

# Test profile_test executable
if [ -f "./profile_test" ]; then
    echo "=== Testing profile_test executable ==="
    run_memory_analysis "profile_test" "./profile_test"

    if [ "$MALLOC_TRACKER_AVAILABLE" -eq 1 ]; then
        echo "--- Custom malloc tracking ---"
        DYLD_INSERT_LIBRARIES=/tmp/malloc_tracker.dylib QT_LOGGING_RULES="*=false" ./profile_test 2>&1 | tail -5
        echo ""
    fi
fi

# Test memory_allocation_test executable
if [ -f "./tests/memory_allocation_test" ]; then
    echo "=== Testing memory_allocation_test executable ==="
    run_memory_analysis "memory_allocation_test" "./tests/memory_allocation_test"

    if [ "$MALLOC_TRACKER_AVAILABLE" -eq 1 ]; then
        echo "--- Custom malloc tracking ---"
        DYLD_INSERT_LIBRARIES=/tmp/malloc_tracker.dylib QT_LOGGING_RULES="*=false" ./tests/memory_allocation_test 2>&1 | tail -10
        echo ""
    fi
fi

# Test RFC 9535 compliance tests for memory usage
if [ -f "./tests/rfc9535_tests" ]; then
    echo "=== Testing RFC 9535 compliance test memory usage ==="
    run_memory_analysis "rfc9535_tests" "./tests/rfc9535_tests --gtest_brief=1"
fi

echo "=== Memory Analysis Complete ==="
echo ""
echo "Summary:"
echo "- System resource usage shows peak memory consumption"
echo "- Leak detection identifies memory leaks"
echo "- Heap profiling (if available) shows allocation patterns"
echo "- Custom malloc tracking provides detailed allocation statistics"
echo ""
echo "For detailed heap analysis, use:"
echo "  HEAPPROFILE=/tmp/heap ./your_executable"
echo "  pprof --web ./your_executable /tmp/heap.*.heap"

# Cleanup
rm -f /tmp/malloc_tracker.c /tmp/malloc_tracker.dylib /tmp/heap_profile_*
