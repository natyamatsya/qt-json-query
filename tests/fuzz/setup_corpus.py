#!/usr/bin/env python3
"""
setup_corpus.py - Set up seed corpus files for LibFuzzer-based fuzz testing

This script converts the seed corpus text files into individual binary files
that LibFuzzer can use as initial test cases for more effective fuzzing.
"""

import os
import sys
import argparse
from pathlib import Path

def setup_corpus_from_file(seed_file: Path, corpus_dir: Path, prefix: str = "seed"):
    """
    Convert a text file with line-separated seeds into individual corpus files.

    Args:
        seed_file: Path to the seed file containing test cases
        corpus_dir: Directory to create corpus files in
        prefix: Prefix for corpus file names
    """
    if not seed_file.exists():
        print(f"Warning: Seed file {seed_file} does not exist")
        return 0

    # Create corpus directory if it doesn't exist
    corpus_dir.mkdir(parents=True, exist_ok=True)

    count = 0
    with open(seed_file, 'r', encoding='utf-8') as f:
        for line_num, line in enumerate(f, 1):
            line = line.strip()

            # Skip empty lines and comments
            if not line or line.startswith('#'):
                continue

            # Create corpus file
            corpus_file = corpus_dir / f"{prefix}_{count:04d}"
            with open(corpus_file, 'w', encoding='utf-8') as cf:
                cf.write(line)

            count += 1

    print(f"Created {count} corpus files in {corpus_dir}")
    return count

def setup_combined_corpus(jsonpath_seeds: Path, json_seeds: Path, corpus_dir: Path):
    """
    Create combined corpus files for the combined evaluation fuzzer.

    Args:
        jsonpath_seeds: Path to JSONPath seed file
        json_seeds: Path to JSON document seed file
        corpus_dir: Directory to create corpus files in
    """
    corpus_dir.mkdir(parents=True, exist_ok=True)

    # Read JSONPath seeds
    jsonpath_lines = []
    if jsonpath_seeds.exists():
        with open(jsonpath_seeds, 'r', encoding='utf-8') as f:
            jsonpath_lines = [line.strip() for line in f
                            if line.strip() and not line.startswith('#')]

    # Read JSON seeds
    json_lines = []
    if json_seeds.exists():
        with open(json_seeds, 'r', encoding='utf-8') as f:
            json_lines = [line.strip() for line in f
                        if line.strip() and not line.startswith('#')]

    count = 0
    # Create combinations of JSONPath + JSON
    for i, jsonpath in enumerate(jsonpath_lines[:50]):  # Limit to avoid too many files
        for j, json_doc in enumerate(json_lines[:20]):   # Limit to avoid too many files
            # Use first byte to indicate split point
            split_point = len(jsonpath) + 1
            combined = bytes([split_point % 256]) + jsonpath.encode('utf-8') + b'\x00' + json_doc.encode('utf-8')

            corpus_file = corpus_dir / f"combined_{count:04d}"
            with open(corpus_file, 'wb') as cf:
                cf.write(combined)

            count += 1
            if count >= 100:  # Limit total combinations
                break
        if count >= 100:
            break

    print(f"Created {count} combined corpus files in {corpus_dir}")
    return count

def main():
    parser = argparse.ArgumentParser(description="Set up fuzz test corpus files")
    parser.add_argument("--build-dir", type=Path,
                       help="Build directory (default: build)")
    parser.add_argument("--corpus-dir", type=Path,
                       help="Base corpus directory (default: <build-dir>/fuzz/corpus)")

    args = parser.parse_args()

    # Determine paths
    script_dir = Path(__file__).parent
    build_dir = args.build_dir or Path("build")
    corpus_base = args.corpus_dir or (build_dir / "fuzz" / "corpus")

    print(f"Setting up fuzz test corpus files...")
    print(f"Script directory: {script_dir}")
    print(f"Build directory: {build_dir}")
    print(f"Corpus base directory: {corpus_base}")

    total_files = 0

    # JSONPath parsing corpus
    jsonpath_seeds = script_dir / "corpus" / "jsonpath_seeds.txt"
    jsonpath_corpus = corpus_base / "fuzz_jsonpath_parsing"
    total_files += setup_corpus_from_file(jsonpath_seeds, jsonpath_corpus, "jsonpath")

    # JSON Pointer parsing corpus
    jsonpointer_seeds = script_dir / "corpus" / "jsonpointer_seeds.txt"
    jsonpointer_corpus = corpus_base / "fuzz_jsonpointer_parsing"
    total_files += setup_corpus_from_file(jsonpointer_seeds, jsonpointer_corpus, "jsonpointer")

    # JSON document corpus (for container cursor and filter storage)
    json_seeds = script_dir / "corpus" / "json_document_seeds.txt"

    container_corpus = corpus_base / "fuzz_container_cursor"
    total_files += setup_corpus_from_file(json_seeds, container_corpus, "json")

    filter_corpus = corpus_base / "fuzz_filter_storage"
    total_files += setup_corpus_from_file(json_seeds, filter_corpus, "json")

    # Combined evaluation corpus
    combined_corpus = corpus_base / "fuzz_combined_evaluation"
    total_files += setup_combined_corpus(jsonpath_seeds, json_seeds, combined_corpus)

    print(f"\nCorpus setup complete!")
    print(f"Total corpus files created: {total_files}")
    print(f"\nTo run fuzz tests:")
    print(f"  cmake --preset debug-qt -DENABLE_FUZZ_TESTS=ON")
    print(f"  cmake --build --preset debug-qt")
    print(f"  make fuzz_quick          # Quick 30-second test of all fuzzers")
    print(f"  make run_all_fuzzers     # Run all fuzzers for 5 minutes each")

if __name__ == "__main__":
    main()
