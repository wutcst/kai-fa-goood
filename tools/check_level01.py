#!/usr/bin/env python3
"""Print level01 map/collision info from all known locations."""

from __future__ import annotations

import re
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CANDIDATES = [
    ROOT / "assets" / "maps" / "level01.tmx",
    ROOT / "assets" / "levels" / "level01_collision.txt",
    ROOT / "build" / "maps" / "level01.tmx",
    ROOT / "build" / "levels" / "level01_collision.txt",
    ROOT / "build" / "Release" / "maps" / "level01.tmx",
    ROOT / "build" / "Release" / "levels" / "level01_collision.txt",
]


def describe_tmx(path: Path) -> str:
    root = ET.parse(path).getroot()
    w = root.get("width", "?")
    h = root.get("height", "?")
    objects = root.findall(".//object")
    player = next((o for o in objects if (o.get("name") or "").lower() in ("player1", "player")), None)
    apples = sum(1 for o in objects if o.get("gid") == "243")
    max_id = max((int(o.get("id", "0")) for o in objects), default=0)
    spawn = f"player1@({player.get('x')},{player.get('y')})" if player is not None else "no player1"
    bad_ts = [
        ts.get("source", "")
        for ts in root.findall("tileset")
        if "build/" in (ts.get("source") or "").replace("\\", "/")
    ]
    extra = f", BAD_TILESET={bad_ts[0]}" if bad_ts else ""
    return f"tmx {w}x{h}, objects={len(objects)}, apples={apples}, maxId={max_id}, {spawn}{extra}"


def describe_collision(path: Path) -> str:
    lines = path.read_text(encoding="utf-8").strip().splitlines()
    if not lines:
        return "collision empty"
    return f"collision {len(lines)}x{len(lines[0])}, spawns={''.join(lines).count('f')}f/{''.join(lines).count('w')}w"


def main() -> None:
    print("level01 asset check\n")
    for path in CANDIDATES:
        if not path.exists():
            print(f"MISSING  {path.relative_to(ROOT)}")
            continue
        if path.suffix == ".tmx":
            info = describe_tmx(path)
        else:
            info = describe_collision(path)
        print(f"{path.relative_to(ROOT)} | {path.stat().st_mtime} | {info}")

    print(
        "\nTip: level01 should match your Tiled map size (e.g. 60x40)."
        "\nIf player1 is missing, add name=\"player1\" to the frog object in Tiled."
    )


if __name__ == "__main__":
    main()
