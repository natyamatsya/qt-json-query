#!/bin/bash

# Working clang-tidy approach with proper paths and configuration

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

echo "🎯 Starting working clang-tidy brace-initialization..."

# Use the LLVM clang-tidy we found
CLANG_TIDY="/opt/homebrew/Cellar/llvm/20.1.8/bin/clang-tidy"

if [[ ! -f "$CLANG_TIDY" ]]; then
    echo "❌ clang-tidy not found at $CLANG_TIDY"
    exit 1
fi

# Create backup
echo "📦 Creating backup..."
git stash push -m "Pre-working-clang-tidy backup $(date)" || echo "No changes to stash"

# Get a small subset of files to test with
TEST_FILES=(
    "src/json-query/json-path/JSONPathContextFilter.cpp"
    "src/json-query/json-path/JSONPathFilterFunctions.cpp"
    "src/json-query/json-path/JSONPath.cpp"
    "src/json-query/json-path/JSONPathEvaluate.cpp"
    "src/json-query/json-path/JSONPathFilterComparison.cpp"
)

echo "📁 Processing ${#TEST_FILES[@]} files with clang-tidy..."

# Run clang-tidy with very specific settings
"$CLANG_TIDY" \
    --checks="modernize-use-brace-initialization" \
    --header-filter="^$(pwd)/(src|include)/.*\.(h|hpp)$" \
    --fix \
    --fix-errors \
    --format-style=file \
    -p build-relwithdebinfo-llvm-clang \
    "${TEST_FILES[@]}" \
    -- \
    -isystem /opt/homebrew/Cellar/llvm/20.1.8/include/c++/v1 \
    -isystem /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/usr/include \
    2>/dev/null || {

    echo "⚠️  clang-tidy failed, trying fallback approach..."

    # Fallback: run without system includes
    "$CLANG_TIDY" \
        --checks="modernize-use-brace-initialization" \
        --header-filter="^$(pwd)/(src|include)/.*\.(h|hpp)$" \
        --fix \
        --fix-errors \
        -p build-relwithdebinfo-llvm-clang \
        "${TEST_FILES[@]}" \
        2>/dev/null || {

        echo "❌ clang-tidy failed completely, reverting..."
        git checkout -- src include
        git stash pop 2>/dev/null || true
        exit 1
    }
}

# Test build
echo "🔨 Testing build..."
if cmake --build build-relwithdebinfo-llvm-clang --parallel; then
    echo "✅ Build successful!"
else
    echo "❌ Build failed, reverting..."
    git checkout -- src include
    git stash pop 2>/dev/null || true
    exit 1
fi

# Test functionality
echo "🧪 Testing functionality..."
if (cd build-relwithdebinfo-llvm-clang && ./tests/rfc9535_tests --gtest_brief=1); then
    echo "✅ All tests pass!"
else
    echo "❌ Tests failed, reverting..."
    git checkout -- src include
    git stash pop 2>/dev/null || true
    exit 1
fi

echo "🎉 Working clang-tidy completed successfully!"
echo "📊 Summary of changes:"
git diff --stat
