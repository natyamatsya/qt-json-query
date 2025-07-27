# Pre-commit Setup Guide

This project uses [pre-commit](https://pre-commit.com/) to automatically enforce code formatting and quality checks before commits.

## Installation

### 1. Install pre-commit

```bash
# Using pip
pip install pre-commit

# Using homebrew (macOS)
brew install pre-commit

# Using conda
conda install -c conda-forge pre-commit
```

### 2. Install the git hook scripts

```bash
# Navigate to project root
cd /path/to/qt-json-query

# Install the pre-commit hooks
pre-commit install
```

### 3. (Optional) Run on all files

```bash
# Run pre-commit on all files in the repository
pre-commit run --all-files
```

## What it does

The pre-commit configuration automatically:

### C++ Code Formatting
- **clang-format**: Applies the project's `.clang-format` configuration
- **Allman brace style**: Opening braces on next line
- **Single-line flow control**: Removes unnecessary braces
- **Excludes submodules**: Skips `tests/references/` and compliance test suites

### Code Quality Checks
- **Trailing whitespace**: Removes trailing spaces
- **End-of-file**: Ensures files end with newline
- **Merge conflicts**: Detects unresolved merge markers
- **YAML/JSON syntax**: Validates configuration files
- **Large files**: Prevents accidentally committing large files
- **Case conflicts**: Checks for filename case issues

### CMake Formatting
- **cmake-format**: Formats CMakeLists.txt and .cmake files

## Usage

Once installed, pre-commit runs automatically on every `git commit`. If any issues are found:

1. **Formatting issues**: Files are automatically fixed
2. **Syntax errors**: Commit is blocked until fixed
3. **Review changes**: Check the auto-formatted files
4. **Re-commit**: Run `git add` and `git commit` again

## Manual Usage

```bash
# Run on specific files
pre-commit run --files src/some-file.cpp

# Run all hooks on all files
pre-commit run --all-files

# Run specific hook
pre-commit run clang-format

# Skip hooks (not recommended)
git commit --no-verify
```

## Configuration

The configuration is in `.pre-commit-config.yaml`. Key settings:

- **clang-format version**: Currently using v18.1.8
- **Excluded paths**: Submodules and build directories
- **File patterns**: C++ files (.cpp, .hpp, .cc, .cxx, .c, .h, .cu)

## Troubleshooting

### Hook installation failed
```bash
# Update pre-commit
pip install --upgrade pre-commit

# Reinstall hooks
pre-commit clean
pre-commit install
```

### Formatting conflicts
```bash
# Check clang-format version
clang-format --version

# Manually format specific file
clang-format -i src/file.cpp
```

### Skip specific commit
```bash
# Only when absolutely necessary
git commit --no-verify -m "Emergency fix"
```

## Benefits

- **Consistent code style**: All commits follow the same formatting
- **Automated quality**: Catches common issues before they reach the repository
- **Team collaboration**: Reduces formatting-related merge conflicts
- **CI/CD ready**: Ensures code quality from the start
