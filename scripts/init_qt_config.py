#!/usr/bin/env python3
"""
Auto-generate qt.user.json by discovering Qt installations on this machine.

Minimal, stdlib-only distillation of the discovery mechanism found in larger
Qt projects (environment detection first, then platform-specific search
roots). The generated qt.user.json (git-ignored) is consumed by
Init-DevEnv.ps1 on Windows and can be read by any other tooling; on
macOS/Linux it is simply a convenient record of usable prefixes.

Usage:
    python scripts/init_qt_config.py            # write qt.user.json (no overwrite)
    python scripts/init_qt_config.py --force    # overwrite an existing config
    python scripts/init_qt_config.py --print    # discovery report only, no write

A "Qt prefix" is the kit directory containing lib/cmake/Qt6/Qt6Config.cmake,
e.g. D:/dev/sdk/qt/qt-6.8.5/6.8.5/msvc2022_64 or ~/Qt/6.8.3/macos.
Additional search roots can be supplied via the QT_SEARCH_ROOTS environment
variable (path-separator separated).
"""

import argparse
import json
import os
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
USER_CONFIG = REPO_ROOT / "qt.user.json"

# Kit directory names per platform, in preference order.
KIT_PATTERNS = {
    "win32": ["msvc2022_64", "msvc2019_64"],
    "darwin": ["macos"],
    "linux": ["gcc_64"],
}

SEARCH_ROOTS = {
    "win32": ["C:/Qt", "D:/dev/sdk/qt"],
    "darwin": ["~/Qt", "/opt/Qt", "/usr/local/Qt"],
    "linux": ["~/Qt", "/opt/Qt", "/usr/local/Qt"],
}


def platform_key() -> str:
    if sys.platform.startswith("win"):
        return "win32"
    if sys.platform == "darwin":
        return "darwin"
    return "linux"


def is_qt_prefix(path: Path) -> bool:
    return (path / "lib" / "cmake" / "Qt6" / "Qt6Config.cmake").is_file()


def qt_version_of(prefix: Path) -> str:
    """Read the Qt version from Qt6ConfigVersion.cmake (no subprocess needed)."""
    version_file = prefix / "lib" / "cmake" / "Qt6" / "Qt6ConfigVersion.cmake"
    try:
        match = re.search(
            r'PACKAGE_VERSION\s+"(\d+\.\d+\.\d+)"', version_file.read_text()
        )
        if match:
            return match.group(1)
    except OSError:
        pass
    # Fall back to a version-looking path component (e.g. .../6.8.5/msvc2022_64)
    for part in reversed(prefix.parts):
        if re.fullmatch(r"\d+\.\d+\.\d+", part):
            return part
    return "0.0.0"


def prefixes_from_environment() -> list[Path]:
    """Qt prefixes already configured in the environment (respected first)."""
    candidates: list[Path] = []
    qt6_dir = os.environ.get("Qt6_DIR")
    if qt6_dir:
        # Qt6_DIR points at <prefix>/lib/cmake/Qt6
        candidates.append(Path(qt6_dir).parent.parent.parent)
    for entry in os.environ.get("CMAKE_PREFIX_PATH", "").split(os.pathsep):
        if entry:
            candidates.append(Path(entry))
    return [p for p in candidates if is_qt_prefix(p)]


def prefixes_from_search_roots() -> list[Path]:
    """Scan platform search roots for <root>/[qt-x.y.z/]x.y.z/<kit> layouts."""
    key = platform_key()
    roots = [Path(r).expanduser() for r in SEARCH_ROOTS[key]]
    roots += [
        Path(r).expanduser()
        for r in os.environ.get("QT_SEARCH_ROOTS", "").split(os.pathsep)
        if r
    ]

    found: list[Path] = []
    for root in roots:
        if not root.is_dir():
            continue
        # Kits sit two or three levels below the root:
        #   <root>/<x.y.z>/<kit>  and  <root>/<qt-x.y.z>/<x.y.z>/<kit>
        for kit in KIT_PATTERNS[key]:
            found.extend(root.glob(f"*/{kit}"))
            found.extend(root.glob(f"*/*/{kit}"))
    return [p for p in found if is_qt_prefix(p)]


def discover() -> list[dict]:
    """All usable Qt prefixes, deduplicated, environment first then by version."""
    seen: set[str] = set()
    installations: list[dict] = []
    for source, prefixes in (
        ("environment", prefixes_from_environment()),
        ("search_path", sorted(
            prefixes_from_search_roots(),
            key=lambda p: tuple(map(int, qt_version_of(p).split("."))),
            reverse=True,
        )),
    ):
        for prefix in prefixes:
            normalized = prefix.resolve().as_posix()
            if normalized in seen:
                continue
            seen.add(normalized)
            installations.append(
                {
                    "prefix": normalized,
                    "version": qt_version_of(prefix),
                    "detected_from": source,
                }
            )
    return installations


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    parser.add_argument("--force", action="store_true",
                        help="overwrite an existing qt.user.json")
    parser.add_argument("--print", dest="print_only", action="store_true",
                        help="report discovered installations without writing")
    args = parser.parse_args()

    installations = discover()
    if not installations:
        print("No Qt installation found. Searched environment (Qt6_DIR, "
              "CMAKE_PREFIX_PATH) and platform search roots; extend via "
              "QT_SEARCH_ROOTS or write qt.user.json manually "
              "(see qt.user.example.json).", file=sys.stderr)
        return 1

    print("Discovered Qt installations:")
    for inst in installations:
        print(f"  {inst['version']:<8} {inst['prefix']}  [{inst['detected_from']}]")

    if args.print_only:
        return 0

    if USER_CONFIG.exists() and not args.force:
        print(f"\n{USER_CONFIG.name} already exists - keeping it (use --force to regenerate).")
        return 0

    config = {
        "userFileVersion": 1,
        "qtPrefixes": [inst["prefix"] for inst in installations],
    }
    USER_CONFIG.write_text(json.dumps(config, indent=4) + "\n")
    print(f"\nWrote {USER_CONFIG}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
