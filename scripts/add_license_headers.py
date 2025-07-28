#!/usr/bin/env python3
"""
Script to add SPDX license headers to source files.

This script will recursively find all .hpp and .cpp files in the project
directory and add the SPDX license header if it's not already present.
"""

import os
import re
import sys
from pathlib import Path

# SPDX license header to add
SPDX_HEADER = "// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception\n"

def should_skip_file(file_path):
    """Check if the file should be skipped based on path patterns."""
    skip_patterns = [
        'build-',
        'cmake-build-',
        'third_party',
        'external',
        'vendor',
        '.git',
        'Testing',
        'tmp'
    ]
    return any(pattern in str(file_path) for pattern in skip_patterns)

def has_spdx_header(content):
    """Check if the file already has an SPDX license header."""
    # Check for SPDX header (case insensitive, with optional comment markers)
    spdx_pattern = re.compile(r'^\s*(//|/\*|#|;|<!--)?\s*SPDX-License-Identifier:', re.IGNORECASE | re.MULTILINE)
    return bool(spdx_pattern.search(content))

def add_license_header(file_path):
    """Add SPDX license header to the file if it doesn't already have one."""
    try:
        with open(file_path, 'r', encoding='utf-8', newline='') as f:
            content = f.read()

        if has_spdx_header(content):
            print(f"✓ {file_path} already has an SPDX header")
            return False

        lines = content.splitlines(keepends=True)
        new_content = []

        # Preserve shebang line if present
        if lines and lines[0].startswith('#!'):
            new_content.append(lines[0])
            lines = lines[1:]

        # Add SPDX header
        new_content.append(SPDX_HEADER)

        # Add a blank line after the header if the file isn't empty
        if lines and lines[0].strip():
            new_content.append('\n')

        # Add the rest of the content
        new_content.extend(lines)

        # Write the file back with the new header
        with open(file_path, 'w', encoding='utf-8', newline='') as f:
            f.write(''.join(new_content))

        print(f"✓ Added SPDX header to {file_path}")
        return True

    except Exception as e:
        print(f"✗ Error processing {file_path}: {str(e)}")
        return False

def main():
    # Get the project root directory (where this script is located)
    project_root = Path(__file__).parent.parent
    print(f"Project root: {project_root}")

    # Find all .hpp and .cpp files
    source_files = []
    for ext in ('*.hpp', '*.cpp', '*.h', '*.c'):
        source_files.extend(project_root.rglob(ext))

    print(f"Found {len(source_files)} source files to process")

    # Process each file
    modified_count = 0
    for file_path in source_files:
        if should_skip_file(file_path):
            print(f"⏩ Skipping {file_path}")
            continue

        if add_license_header(file_path):
            modified_count += 1

    print(f"\nDone! Modified {modified_count} files.")

if __name__ == "__main__":
    main()
