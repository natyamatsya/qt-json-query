#!/bin/bash

# Automatic Code Reformatting Script
# Applies .clang-format to the entire codebase while excluding submodules
# Non-interactive version for automated execution

set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

echo "🎨 Starting automatic code reformatting with .clang-format"
echo "Project root: $PROJECT_ROOT"
echo

# Check if clang-format is available
CLANG_FORMAT=""
if command -v clang-format &> /dev/null; then
    CLANG_FORMAT="clang-format"
elif [ -f "/opt/homebrew/Cellar/llvm/20.1.8/bin/clang-format" ]; then
    CLANG_FORMAT="/opt/homebrew/Cellar/llvm/20.1.8/bin/clang-format"
elif [ -f "/opt/homebrew/Cellar/llvm@19/19.1.7/bin/clang-format" ]; then
    CLANG_FORMAT="/opt/homebrew/Cellar/llvm@19/19.1.7/bin/clang-format"
else
    echo "❌ Error: clang-format not found"
    echo "   Please install clang-format or ensure it's available"
    exit 1
fi

CLANG_FORMAT_VERSION=$($CLANG_FORMAT --version)
echo "📋 Using: $CLANG_FORMAT_VERSION"
echo

# Create backup
echo "📦 Creating backup..."
git stash push -m "Pre-clang-format backup $(date)" || echo "No changes to stash"

# Define directories to exclude (submodules and build directories)
EXCLUDE_DIRS=(
    "tests/references/jayway-json-path"
    "compliance/rfc-9535-jsonpath-test-suite"
    "tests/references/flitbit-json-ptr"
    "build-*"
    "cmake-build-*"
    ".git"
    "_deps"
    "external"
)

echo "🔍 Finding C++ files to format (excluding submodules)..."

# Build find command with exclusions
FIND_CMD="find . -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' -o -name '*.cc' -o -name '*.cxx' \)"

# Add exclusions to find command
for exclude_dir in "${EXCLUDE_DIRS[@]}"; do
    FIND_CMD="$FIND_CMD -not -path './$exclude_dir/*'"
done

# Get list of files to format
FILES_TO_FORMAT=$(eval "$FIND_CMD")
FILE_COUNT=$(echo "$FILES_TO_FORMAT" | wc -l)

echo "📊 Found $FILE_COUNT C++ files to format"
echo "🚀 Starting reformatting..."

# Format files with progress
FORMATTED_COUNT=0
FAILED_COUNT=0
FAILED_FILES=()

while IFS= read -r file; do
    if [ -n "$file" ]; then
        printf "\r🔧 Formatting: %-60s [%d/%d]" "$(basename "$file")" "$((FORMATTED_COUNT + 1))" "$FILE_COUNT"

        if $CLANG_FORMAT -i "$file" 2>/dev/null; then
            ((FORMATTED_COUNT++))
        else
            ((FAILED_COUNT++))
            FAILED_FILES+=("$file")
        fi
    fi
done <<< "$FILES_TO_FORMAT"

echo
echo

# Report results
echo "✅ Reformatting complete!"
echo "📊 Results:"
echo "   - Successfully formatted: $FORMATTED_COUNT files"
echo "   - Failed: $FAILED_COUNT files"

if [ $FAILED_COUNT -gt 0 ]; then
    echo
    echo "❌ Failed files:"
    for failed_file in "${FAILED_FILES[@]}"; do
        echo "   - $failed_file"
    done
fi

echo
echo "🔍 Checking for changes..."
if git diff --quiet; then
    echo "✨ No formatting changes needed - code was already properly formatted!"
else
    CHANGED_FILES=$(git diff --name-only | wc -l)
    echo "📝 Formatting applied to $CHANGED_FILES files"
    echo
    echo "📋 Changed files:"
    git diff --name-only | head -10
    if [ $CHANGED_FILES -gt 10 ]; then
        echo "   ... and $((CHANGED_FILES - 10)) more files"
    fi
fi

echo
echo "🎉 Code reformatting completed successfully!"
echo "💡 Review changes with: git diff"
echo "💡 Commit changes with: git add -A && git commit -m 'Apply clang-format to codebase'"
