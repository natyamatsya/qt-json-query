#!/usr/bin/env bash
# Profile.sh — simple gperftools front-end that produces Callgrind output.
# Usage:
#   ./Profile.sh <BinaryName> [<SourceBaseName>]
#     <BinaryName>       Name (and path) of the executable to run / build.
#     <SourceBaseName>   Optional: if given, the script will compile <SourceBaseName>.cpp into <BinaryName> first.
#
# The script:
#   1. Optionally compiles a single .cpp file.
#   2. Runs the program under gperftools CPU profiler.
#   3. Converts the .prof file into Callgrind-compatible text using pprof.
#   4. Prints the resulting .callgrind path.

set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <BinaryName> [<SourceBaseName>]" >&2
  exit 1
fi

BIN="$1"
BUILD_DIR="${2:-build}"

# Determine libprofiler.dylib location via Homebrew (cross-arch safe).
LIBPROFILER="$(brew --prefix gperftools)/lib/libprofiler.dylib"

# 1. Ensure the binary exists; if not, attempt to build it with CMake.
if [[ ! -x "$BIN" ]]; then
  echo "Binary '$BIN' not found – attempting to build with CMake (directory: $BUILD_DIR, target: $BIN)…"
  if [[ ! -d "$BUILD_DIR" ]]; then
    echo "CMake build directory '$BUILD_DIR' does not exist." >&2
    exit 1
  fi
  cmake --build "$BUILD_DIR" --target "$BIN" --config Release || {
    echo "Failed to build target '$BIN'" >&2
    exit 1
  }
  # Try common output locations after build
  if [[ -x "$BUILD_DIR/$BIN" ]]; then
    BIN="$BUILD_DIR/$BIN"
  elif [[ -x "$BUILD_DIR/Release/$BIN" ]]; then
    BIN="$BUILD_DIR/Release/$BIN"
  else
    echo "Unable to locate built binary for target '$1'" >&2
    exit 1
  fi
fi

# 2. Run under gperftools CPU profiler.
PROF_FILE="${BIN}.prof"
echo "Running $BIN under gperftools (profile → $PROF_FILE) …"
CPUPROFILE="$PROF_FILE" DYLD_INSERT_LIBRARIES="$LIBPROFILER" "./${BIN}"

echo "Generating Callgrind output …"
CALLGRIND_FILE="${BIN}.callgrind"
pprof --callgrind "$BIN" "$PROF_FILE" > "$CALLGRIND_FILE"

echo "Profiling complete: $CALLGRIND_FILE"
