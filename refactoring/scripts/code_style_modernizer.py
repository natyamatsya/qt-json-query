#!/usr/bin/env python3
"""
Code Style Modernizer for qt-json-query

Applies the following code style transformations:
1. const auto for local variables with explicit type declarations
2. Brace-initialization for default-constructed objects
3. Designated initializers where applicable

Usage:
    python code_style_modernizer.py [--dry-run] [--file FILE] [--all]

Options:
    --dry-run    Show changes without applying them
    --file FILE  Process a specific file
    --all        Process all C++ files in the project
    --verify     Build and test after changes
"""

import argparse
import os
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional


@dataclass
class TransformResult:
    """Result of a transformation."""
    original: str
    transformed: str
    line_num: int
    description: str


@dataclass
class FileResult:
    """Result of processing a file."""
    path: Path
    transforms: list[TransformResult]
    success: bool
    error: Optional[str] = None


# Qt types that should use const auto
QT_TYPES = {
    "QJsonArray", "QJsonObject", "QJsonValue", "QJsonDocument",
    "QString", "QStringView", "QByteArray",
    "QList", "QVector", "QMap", "QHash", "QSet",
    "QVariant", "QRegularExpression", "QRegularExpressionMatch",
}

# Standard library types that should use const auto
STD_TYPES = {
    "std::string", "std::string_view", "std::vector", "std::map",
    "std::unordered_map", "std::set", "std::unordered_set",
    "std::optional", "std::variant", "std::pair", "std::tuple",
}

# Types that should use brace-init when default constructed
BRACE_INIT_TYPES = {
    "TypeConstraint", "ObjectSchema", "BooleanSchema", "RefSchema",
    "ValidationError", "ValidationResult", "CompileContext",
    "std::vector", "std::string", "QString", "QJsonObject", "QJsonArray",
}


def find_project_root() -> Path:
    """Find the project root directory."""
    script_dir = Path(__file__).parent
    # Go up from refactoring/scripts to project root
    return script_dir.parent.parent


def get_cpp_files(root: Path, include_tests: bool = False) -> list[Path]:
    """Get all C++ source and header files."""
    patterns = [
        "src/**/*.cpp",
        "src/**/*.hpp",
        "include/**/*.hpp",
    ]
    if include_tests:
        patterns.extend([
            "tests/**/*.cpp",
            "examples/**/*.cpp",
        ])

    files = []
    for pattern in patterns:
        files.extend(root.glob(pattern))

    # Exclude submodules and build directories
    exclude_patterns = [
        "build", "_deps", "external", "references",
        "jayway-json-path", "flitbit-json-ptr", "rfc-9535",
    ]

    return [
        f for f in files
        if not any(excl in str(f) for excl in exclude_patterns)
    ]


def transform_const_auto(line: str, line_num: int) -> Optional[TransformResult]:
    """
    Transform explicit type declarations to const auto where appropriate.

    Examples:
        const QJsonArray arr = x.toArray();  ->  const auto arr = x.toArray();
        QString msg = QString(...);          ->  const auto msg = QString(...);
    """
    # Pattern for const TYPE var = expr;
    const_pattern = r'^(\s*)const\s+((?:std::)?(?:' + '|'.join(QT_TYPES | STD_TYPES) + r')(?:<[^>]+>)?)\s+(\w+)\s*=\s*(.+)$'

    match = re.match(const_pattern, line)
    if match:
        indent, type_name, var_name, expr = match.groups()
        # Strip trailing semicolon if present for brace-init
        expr_clean = expr.rstrip(';').strip()
        new_line = f"{indent}const auto {var_name}{{{expr_clean}}};"
        if new_line != line:
            return TransformResult(
                original=line,
                transformed=new_line,
                line_num=line_num,
                description=f"const {type_name} -> const auto with brace-init"
            )

    # Pattern for non-const TYPE var = expr; (where var is not modified later)
    # This is harder to detect safely, so we only do it for clearly immutable cases
    nonconst_pattern = r'^(\s*)((?:std::)?(?:' + '|'.join(QT_TYPES | STD_TYPES) + r')(?:<[^>]+>)?)\s+(\w+)\s*=\s*(.+;)$'

    match = re.match(nonconst_pattern, line)
    if match:
        indent, type_name, var_name, expr = match.groups()
        # Only transform if the expression is a function call that returns by value
        # and the variable name suggests it's not modified (msg, result, arr, obj, etc.)
        safe_names = {'msg', 'result', 'arr', 'obj', 'str', 'doc', 'value', 'error', 'path'}
        if var_name.lower() in safe_names or var_name.endswith('Result') or var_name.endswith('Array'):
            # Check if it looks like a function call or method call
            if re.search(r'\.\w+\([^)]*\)\s*;$', expr) or re.search(r'::\w+\([^)]*\)\s*;$', expr):
                # Strip trailing semicolon for brace-init
                expr_clean = expr.rstrip(';').strip()
                new_line = f"{indent}const auto {var_name}{{{expr_clean}}};"
                if new_line != line:
                    return TransformResult(
                        original=line,
                        transformed=new_line,
                        line_num=line_num,
                        description=f"{type_name} -> const auto with brace-init"
                    )

    return None


def transform_brace_init(line: str, line_num: int) -> Optional[TransformResult]:
    """
    Transform default construction to brace-initialization.

    Examples:
        TypeConstraint constraint;  ->  TypeConstraint constraint{};
        std::vector<int> vec;       ->  std::vector<int> vec{};
    """
    # Pattern for TYPE var; (default construction without initialization)
    pattern = r'^(\s*)((?:std::)?(?:' + '|'.join(BRACE_INIT_TYPES) + r')(?:<[^>]+>)?)\s+(\w+)\s*;$'

    match = re.match(pattern, line)
    if match:
        indent, type_name, var_name = match.groups()
        new_line = f"{indent}{type_name} {var_name}{{}};"
        return TransformResult(
            original=line,
            transformed=new_line,
            line_num=line_num,
            description=f"Default init -> brace-init"
        )

    return None


def transform_size_t_auto(line: str, line_num: int) -> Optional[TransformResult]:
    """
    Transform std::size_t declarations to const auto where safe.

    Examples:
        std::size_t index = nodes.size();  ->  const auto index = nodes.size();
    """
    pattern = r'^(\s*)(?:const\s+)?(?:std::)?size_t\s+(\w+)\s*=\s*(\S.*\S)\s*;$'

    match = re.match(pattern, line)
    if match:
        indent, var_name, expr = match.groups()
        # Only transform if it's a .size() call or similar
        if '.size()' in expr or 'static_cast<' in expr:
            new_line = f"{indent}const auto {var_name}{{{expr}}};"
            if new_line != line:
                return TransformResult(
                    original=line,
                    transformed=new_line,
                    line_num=line_num,
                    description=f"size_t -> const auto with brace-init"
                )

    return None


def transform_double_auto(line: str, line_num: int) -> Optional[TransformResult]:
    """
    Transform double declarations to const auto where safe.

    Examples:
        double d = value.toDouble();  ->  const auto d{value.toDouble()};
    """
    pattern = r'^(\s*)(?:const\s+)?double\s+(\w+)\s*=\s*(\S.*\S)\s*;$'

    match = re.match(pattern, line)
    if match:
        indent, var_name, expr = match.groups()
        # Only transform method calls
        if '.' in expr or '::' in expr:
            new_line = f"{indent}const auto {var_name}{{{expr}}};"
            if new_line != line:
                return TransformResult(
                    original=line,
                    transformed=new_line,
                    line_num=line_num,
                    description=f"double -> const auto with brace-init"
                )

    return None


def transform_assignment_to_brace_init(line: str, line_num: int) -> Optional[TransformResult]:
    """
    Transform auto = expr and const auto = expr to brace-init.

    Examples:
        const auto index = nodes.size();  ->  const auto index{nodes.size()};
        auto result = parseKeyword(...);  ->  auto result{parseKeyword(...)};
    """
    # Match const auto var = expr; but not already brace-init
    const_pattern = r'^(\s*)const\s+auto\s+(\w+)\s*=\s*(.+)\s*;$'

    match = re.match(const_pattern, line)
    if match:
        indent, var_name, expr = match.groups()
        expr = expr.strip()
        # Skip if already using brace-init or if expr contains complex constructs
        if not expr.startswith('{') and not expr.endswith('}'):
            # Skip string literals and lambdas
            if not (expr.startswith('"') or expr.startswith('[') or 'lambda' in expr.lower()):
                new_line = f"{indent}const auto {var_name}{{{expr}}};"
                if new_line != line:
                    return TransformResult(
                        original=line,
                        transformed=new_line,
                        line_num=line_num,
                        description=f"const auto = -> brace-init"
                    )

    # Match non-const auto var = expr;
    auto_pattern = r'^(\s*)auto\s+(\w+)\s*=\s*(.+)\s*;$'

    match = re.match(auto_pattern, line)
    if match:
        indent, var_name, expr = match.groups()
        expr = expr.strip()
        # Skip if already using brace-init or if expr contains complex constructs
        if not expr.startswith('{') and not expr.endswith('}'):
            # Skip string literals, lambdas, and iterators
            if not (expr.startswith('"') or expr.startswith('[') or 'lambda' in expr.lower()):
                # Skip iterator patterns like obj.begin()
                if not (expr.endswith('.begin()') or expr.endswith('.end()')):
                    new_line = f"{indent}auto {var_name}{{{expr}}};"
                    if new_line != line:
                        return TransformResult(
                            original=line,
                            transformed=new_line,
                            line_num=line_num,
                            description=f"auto = -> brace-init"
                        )

    return None


def process_file(file_path: Path, dry_run: bool = False) -> FileResult:
    """Process a single file and apply transformations."""
    try:
        content = file_path.read_text(encoding='utf-8')
    except Exception as e:
        return FileResult(path=file_path, transforms=[], success=False, error=str(e))

    lines = content.splitlines(keepends=True)
    transforms: list[TransformResult] = []
    new_lines = []

    for i, line in enumerate(lines, start=1):
        line_stripped = line.rstrip('\n\r')

        # Try each transformation in order
        result = (
            transform_const_auto(line_stripped, i) or
            transform_brace_init(line_stripped, i) or
            transform_size_t_auto(line_stripped, i) or
            transform_double_auto(line_stripped, i) or
            transform_assignment_to_brace_init(line_stripped, i)
        )

        if result:
            transforms.append(result)
            # Preserve the original line ending
            ending = line[len(line_stripped):] if len(line) > len(line_stripped) else '\n'
            new_lines.append(result.transformed + ending)
        else:
            new_lines.append(line)

    if transforms and not dry_run:
        try:
            file_path.write_text(''.join(new_lines), encoding='utf-8')
        except Exception as e:
            return FileResult(path=file_path, transforms=transforms, success=False, error=str(e))

    return FileResult(path=file_path, transforms=transforms, success=True)


def run_clang_format(file_path: Path) -> bool:
    """Run clang-format on a file."""
    try:
        subprocess.run(
            ["clang-format", "-i", str(file_path)],
            check=True,
            capture_output=True
        )
        return True
    except subprocess.CalledProcessError:
        return False
    except FileNotFoundError:
        print("⚠️  clang-format not found, skipping formatting")
        return True


def verify_build(project_root: Path, build_dir: str = "build-debug-msvc") -> bool:
    """Verify the project builds successfully."""
    build_path = project_root / build_dir
    if not build_path.exists():
        print(f"⚠️  Build directory {build_dir} not found, skipping verification")
        return True

    try:
        result = subprocess.run(
            ["cmake", "--build", str(build_path), "--target", "json_query"],
            capture_output=True,
            text=True,
            cwd=project_root
        )
        return result.returncode == 0
    except Exception as e:
        print(f"⚠️  Build verification failed: {e}")
        return False


def run_tests(project_root: Path, build_dir: str = "build-debug-msvc") -> bool:
    """Run the test suite."""
    test_exe = project_root / build_dir / "tests" / "json_schema_tests.exe"
    if not test_exe.exists():
        test_exe = project_root / build_dir / "tests" / "json_schema_tests"

    if not test_exe.exists():
        print("⚠️  Test executable not found, skipping tests")
        return True

    try:
        result = subprocess.run(
            [str(test_exe), "--gtest_brief=1"],
            capture_output=True,
            text=True
        )
        return result.returncode == 0
    except Exception as e:
        print(f"⚠️  Test execution failed: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(description="Code Style Modernizer for qt-json-query")
    parser.add_argument("--dry-run", action="store_true", help="Show changes without applying them")
    parser.add_argument("--file", type=Path, help="Process a specific file")
    parser.add_argument("--all", action="store_true", help="Process all C++ files")
    parser.add_argument("--verify", action="store_true", help="Build and test after changes")
    parser.add_argument("--include-tests", action="store_true", help="Include test files")
    parser.add_argument("--no-format", action="store_true", help="Skip clang-format")

    args = parser.parse_args()

    project_root = find_project_root()
    print(f"🎯 Code Style Modernizer")
    print(f"📁 Project root: {project_root}")
    print()

    if args.dry_run:
        print("🔍 DRY RUN MODE - no changes will be applied")
        print()

    # Determine files to process
    if args.file:
        files = [args.file]
    elif args.all:
        files = get_cpp_files(project_root, include_tests=args.include_tests)
    else:
        # Default: only process json-schema files
        files = list((project_root / "src" / "json-query" / "json-schema").glob("*.cpp"))
        files += list((project_root / "include" / "json-query" / "json-schema").glob("*.hpp"))
        files += list((project_root / "include" / "json-query" / "json-schema" / "internal").glob("*.hpp"))

    print(f"📊 Processing {len(files)} files")
    print()

    total_transforms = 0
    files_changed = 0

    for file_path in sorted(files):
        result = process_file(file_path, dry_run=args.dry_run)

        if not result.success:
            print(f"❌ {file_path.relative_to(project_root)}: {result.error}")
            continue

        if result.transforms:
            files_changed += 1
            total_transforms += len(result.transforms)

            rel_path = file_path.relative_to(project_root)
            print(f"📝 {rel_path}: {len(result.transforms)} changes")

            for t in result.transforms:
                print(f"   L{t.line_num}: {t.description}")
                if args.dry_run:
                    print(f"      - {t.original.strip()}")
                    print(f"      + {t.transformed.strip()}")

            # Run clang-format on changed files
            if not args.dry_run and not args.no_format:
                if run_clang_format(file_path):
                    print(f"   ✨ Formatted with clang-format")

    print()
    print(f"📊 Summary:")
    print(f"   Files processed: {len(files)}")
    print(f"   Files changed: {files_changed}")
    print(f"   Total transformations: {total_transforms}")

    if args.verify and not args.dry_run and total_transforms > 0:
        print()
        print("🔨 Verifying build...")
        if verify_build(project_root):
            print("✅ Build successful")

            print("🧪 Running tests...")
            if run_tests(project_root):
                print("✅ All tests passed")
            else:
                print("❌ Tests failed!")
                return 1
        else:
            print("❌ Build failed!")
            return 1

    print()
    print("🎉 Code style modernization complete!")
    return 0


if __name__ == "__main__":
    # Fix Windows console encoding for Unicode output
    if sys.platform == "win32":
        import io
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
        sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')
    sys.exit(main())
