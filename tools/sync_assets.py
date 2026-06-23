#!/usr/bin/env python3
"""Copy assets/levels and assets/maps into build output folders."""

from __future__ import annotations

import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC_LEVELS = ROOT / "assets" / "levels"
SRC_MAPS = ROOT / "assets" / "maps"
TARGETS = [
    ROOT / "build" / "levels",
    ROOT / "build" / "maps",
    ROOT / "build" / "Release" / "levels",
    ROOT / "build" / "Release" / "maps",
]


def sync_dir(src: Path, dst: Path) -> None:
    if not src.is_dir():
        raise SystemExit(f"Missing source directory: {src}")
    dst.mkdir(parents=True, exist_ok=True)
    shutil.copytree(src, dst, dirs_exist_ok=True)
    print(f"Synced {src.relative_to(ROOT)} -> {dst.relative_to(ROOT)}")


def main() -> None:
    for target_root in TARGETS:
        parent = target_root.parent
        if not parent.exists():
            continue
        if target_root.name == "levels":
            sync_dir(SRC_LEVELS, target_root)
        else:
            sync_dir(SRC_MAPS, target_root)
    print("Done. Restart the game after syncing.")


if __name__ == "__main__":
    main()
