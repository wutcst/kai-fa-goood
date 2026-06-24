#!/usr/bin/env python3
"""Sync VERSION across pom.xml, ReleaseVersion.java, CMakeLists.txt, and LauncherTest."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
VERSION_FILE = ROOT / "VERSION"
POM = ROOT / "pom.xml"
RELEASE_JAVA = ROOT / "src/main/java/cn/edu/whut/sept/fireice/ReleaseVersion.java"
LAUNCHER_TEST = ROOT / "src/test/java/cn/edu/whut/sept/fireice/LauncherTest.java"
CMAKE = ROOT / "CMakeLists.txt"

SEMVER_RE = re.compile(r"^\d+\.\d+\.\d+(-[\w.+-]+)?$")


def read_version() -> str:
    if not VERSION_FILE.is_file():
        raise SystemExit(f"Missing {VERSION_FILE}")
    version = VERSION_FILE.read_text(encoding="utf-8").strip()
    if not SEMVER_RE.match(version):
        raise SystemExit(f"Invalid version in VERSION: {version!r}")
    return version


def write_version(version: str) -> None:
    if not SEMVER_RE.match(version):
        raise SystemExit(f"Invalid semver: {version!r} (expected X.Y.Z or X.Y.Z-suffix)")
    VERSION_FILE.write_text(version + "\n", encoding="utf-8")


def replace_in_file(path: Path, pattern: str, repl: str) -> bool:
    text = path.read_text(encoding="utf-8")
    new_text, count = re.subn(pattern, repl, text, count=1, flags=re.MULTILINE)
    if count != 1:
        raise SystemExit(f"Could not update {path} (pattern matched {count} times)")
    path.write_text(new_text, encoding="utf-8")
    return True


def sync_all(version: str) -> None:
    write_version(version)
    replace_in_file(
        POM,
        r"(<artifactId>fire-ice</artifactId>\s*\n\s*<version>)[^<]+(</version>)",
        rf"\g<1>{version}\g<2>",
    )
    replace_in_file(
        RELEASE_JAVA,
        r'(public static final String VERSION = ")[^"]+(";\s*)',
        rf'\g<1>{version}\g<2>',
    )
    replace_in_file(
        LAUNCHER_TEST,
        r'(assertEquals\(")[^"]+(", ReleaseVersion\.VERSION\))',
        rf'\g<1>{version}\g<2>',
    )
    replace_in_file(
        CMAKE,
        r"(project\(FireIceOnline VERSION )\d+\.\d+\.\d+(-[\w.+-]+)?( LANGUAGES CXX\))",
        rf"\g<1>{version}\g<3>",
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Read or bump the project release version.")
    parser.add_argument("version", nargs="?", help="New semver to write and sync")
    parser.add_argument("--read", action="store_true", help="Print current VERSION and exit")
    args = parser.parse_args()

    if args.read or not args.version:
        if args.version:
            sync_all(args.version)
            print(args.version)
            return 0
        print(read_version())
        return 0

    sync_all(args.version)
    print(f"Version bumped to {args.version}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
