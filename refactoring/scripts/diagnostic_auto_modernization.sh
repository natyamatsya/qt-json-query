#!/bin/bash

# Diagnostic Auto Modernization Script
# Stops on build failure WITHOUT reverting changes to allow manual fixing
# Based on incremental approach but preserves failed changes for diagnosis

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

echo "🚀 Starting Diagnostic Auto Modernization"
echo "⚠️  This script will STOP on build failure without reverting changes"
echo "   You can then manually fix the compilation errors and continue"

# Verify the modernizer tool exists
if [[ ! -f "scripts/comprehensive_auto_modernizer" ]]; then
    echo "❌ Error: comprehensive_auto_modernizer not found"
    echo "   Please compile it first: g++ -std=c++17 -O2 -o scripts/comprehensive_auto_modernizer scripts/comprehensive_auto_modernizer.cpp"
    exit 1
fi

echo "📍 Using custom auto modernizer: scripts/comprehensive_auto_modernizer"
echo ""

# Create backup
echo "📦 Creating backup..."
git stash push -m "Pre-diagnostic-auto-modernization backup $(date)" || echo "No changes to stash"

# Function to process a single file with diagnostic output
process_file_with_diagnostics() {
    local file="$1"

    echo "  📄 Processing: $file"

    # Apply modernization
    local output
    output=$(./scripts/comprehensive_auto_modernizer "$file" 2>&1)
    local changes=$(echo "$output" | grep "Total changes applied:" | cut -d: -f2 | tr -d ' ' || echo "0")

    if [[ "$changes" == "0" ]] || [[ -z "$changes" ]]; then
        echo "    ℹ️  No changes needed for $file"
        return 0
    fi

    echo "    🔧 Applied $changes auto modernizations to $file"
    echo "    📋 Modernization details:"
    echo "$output" | grep -E "✅|❌" | sed 's/^/      /'

    # Validate build after this file
    echo "    🧪 Validating build..."
    if cmake --build build-relwithdebinfo-apple-clang --parallel 2>&1; then
        echo "    ✅ Build successful after $file"
        return 0
    else
        echo "    ❌ Build failed after $file"
        echo ""
        echo "🛑 BUILD FAILURE DETECTED"
        echo "📁 Failed file: $file"
        echo "🔧 Applied $changes modernizations before failure"
        echo ""
        echo "🔍 To diagnose the issue:"
        echo "   1. Check the build output above for compilation errors"
        echo "   2. Examine the changes in: $file"
        echo "   3. Fix the compilation errors manually"
        echo "   4. Test the build: cmake --build build-relwithdebinfo-apple-clang --parallel"
        echo "   5. Continue with remaining files if desired"
        echo ""
        echo "📊 Progress so far:"
        echo "   Current file: $file"
        echo "   Changes applied: $changes"
        echo ""
        echo "🔄 To see what was changed:"
        echo "   git diff $file"
        echo ""
        echo "💡 Changes have NOT been reverted - you can fix them manually!"

        return 1
    fi
}

# Counters
total_files=0
successful_files=0
failed_file=""

echo "🔍 Processing files in order of safety (least likely to break build)"

# Start with examples (safest)
echo ""
echo "Phase 1: Examples (.cpp files)"
for file in examples/*.cpp; do
    if [[ -f "$file" ]]; then
        ((total_files++))
        if process_file_with_diagnostics "$file"; then
            ((successful_files++))
        else
            failed_file="$file"
            break
        fi
    fi
done

# Continue with benchmarks if no failure yet
if [[ -z "$failed_file" ]]; then
    echo ""
    echo "Phase 2: Benchmarks (.cpp files)"
    for file in benchmarks/*.cpp; do
        if [[ -f "$file" ]]; then
            ((total_files++))
            if process_file_with_diagnostics "$file"; then
                ((successful_files++))
            else
                failed_file="$file"
                break
            fi
        fi
    done
fi

# Continue with performance tools if no failure yet
if [[ -z "$failed_file" ]]; then
    echo ""
    echo "Phase 3: Performance Tools (.cpp files)"
    for file in perf/src/*.cpp; do
        if [[ -f "$file" ]]; then
            ((total_files++))
            if process_file_with_diagnostics "$file"; then
                ((successful_files++))
            else
                failed_file="$file"
                break
            fi
        fi
    done
fi

# Continue with core source files if no failure yet
if [[ -z "$failed_file" ]]; then
    echo ""
    echo "Phase 4: Core Source Files (.cpp files)"
    for file in src/json-query/json-path/*.cpp src/json-query/json-path/internal/*.cpp src/json-query/json-pointer/*.cpp; do
        if [[ -f "$file" ]]; then
            ((total_files++))
            if process_file_with_diagnostics "$file"; then
                ((successful_files++))
            else
                failed_file="$file"
                break
            fi
        fi
    done
fi

# Continue with headers if no failure yet (most risky)
if [[ -z "$failed_file" ]]; then
    echo ""
    echo "Phase 5: Header Files (.hpp files) - Most Conservative"
    for file in include/json-query/json-path/*.hpp include/json-query/json-path/internal/*.hpp include/json-query/json-pointer/*.hpp; do
        if [[ -f "$file" ]]; then
            ((total_files++))
            if process_file_with_diagnostics "$file"; then
                ((successful_files++))
            else
                failed_file="$file"
                break
            fi
        fi
    done
fi

# Final summary
echo ""
echo "📊 Diagnostic Auto Modernization Summary:"
echo "   Total files processed: $total_files"
echo "   Successfully modernized: $successful_files"

if [[ -n "$failed_file" ]]; then
    echo "   ❌ Failed on file: $failed_file"
    echo ""
    echo "🔧 Next Steps:"
    echo "   1. Fix the compilation errors in: $failed_file"
    echo "   2. Verify the build works: cmake --build build-relwithdebinfo-apple-clang --parallel"
    echo "   3. Run this script again to continue with remaining files"
    echo "   4. Or manually process remaining files one by one"
    echo ""
    echo "📁 Files remaining to process:"
    echo "   Run: find src examples perf benchmarks include -name '*.cpp' -o -name '*.hpp' | wc -l"
    echo "   Processed: $total_files"

    exit 1
else
    echo "   ✅ All files processed successfully!"
    echo ""
    echo "🧪 Running final comprehensive test suite..."
    if (cd build-relwithdebinfo-apple-clang && ./tests/rfc9535_tests --gtest_brief=1 | tail -5); then
        echo "✅ All RFC 9535 tests pass after diagnostic auto modernization!"
    else
        echo "❌ Tests failed - manual investigation required"
        exit 1
    fi

    echo ""
    echo "🎉 Diagnostic Auto Modernization completed successfully!"
    echo "📈 Changes made:"
    git diff --stat

    echo ""
    echo "🔍 Sample transformations:"
    git diff | head -30
fi
