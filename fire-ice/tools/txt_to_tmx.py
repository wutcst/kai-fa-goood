#!/usr/bin/env python3
"""Bootstrap a Tiled .tmx from an existing ASCII level file."""

from __future__ import annotations

import argparse
from pathlib import Path

CHAR_TO_LOGIC = {
    "#": "solid",
    ".": "empty",
    "L": "lava",
    "W": "water",
    "F": "fire_door",
    "I": "water_door",
    "E": "fire_exit",
    "X": "water_exit",
    "G": "gem",
    "f": "fire_spawn",
    "w": "water_spawn",
    "B": "button",
}

# logic.tsx local tile id order (must match generate_tilesets.py)
LOGIC_ORDER = [
    "solid",
    "empty",
    "lava",
    "water",
    "fire_door",
    "water_door",
    "fire_exit",
    "water_exit",
    "gem",
    "fire_spawn",
    "water_spawn",
    "button",
]
LOGIC_GID = {name: idx + 1 for idx, name in enumerate(LOGIC_ORDER)}  # firstgid=1

FOREST_GID = {
    "empty": 101,
    "solid": 102,
    "lava": 103,
    "water": 104,
}


def read_rows(path: Path) -> list[str]:
    rows = []
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.strip():
            rows.append(line.rstrip("\r"))
    return rows


def csv(values: list[int]) -> str:
    return ",".join(str(v) for v in values)


def build_tmx(rows: list[str], map_name: str) -> str:
    height = len(rows)
    width = max(len(r) for r in rows)

    collision: list[int] = []
    background: list[int] = []
    walls: list[int] = []

    for row in rows:
        padded = row.ljust(width, "#")
        for ch in padded:
            logic = CHAR_TO_LOGIC.get(ch, "solid")
            collision.append(LOGIC_GID[logic])

            if logic in ("empty", "fire_spawn", "water_spawn", "gem", "fire_exit", "water_exit", "button"):
                background.append(FOREST_GID["empty"])
                walls.append(0)
            elif logic in ("lava", "water"):
                background.append(FOREST_GID[logic])
                walls.append(0)
            else:
                background.append(FOREST_GID["empty"])
                walls.append(FOREST_GID["solid"] if logic == "solid" else 0)

    return f"""<?xml version="1.0" encoding="UTF-8"?>
<map version="1.10" tiledversion="1.10.2" orientation="orthogonal" renderorder="right-down" width="{width}" height="{height}" tilewidth="32" tileheight="32" infinite="0" nextlayerid="6" nextobjectid="1">
 <tileset firstgid="1" source="../tilesets/logic.tsx"/>
 <tileset firstgid="101" source="../tilesets/forest.tsx"/>
 <layer id="1" name="Background" width="{width}" height="{height}">
  <data encoding="csv">
{csv(background)}
</data>
 </layer>
 <layer id="2" name="Walls" width="{width}" height="{height}">
  <data encoding="csv">
{csv(walls)}
</data>
 </layer>
 <layer id="3" name="Collision" width="{width}" height="{height}">
  <data encoding="csv">
{csv(collision)}
</data>
 </layer>
 <layer id="4" name="Decor" width="{width}" height="{height}">
  <data encoding="csv">
{",".join(["0"] * width * height)}
</data>
 </layer>
 <objectgroup id="5" name="Objects"/>
</map>
"""


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("txt", type=Path, help="Source ASCII map")
    parser.add_argument("-o", "--output", type=Path, required=True, help="Output .tmx path")
    args = parser.parse_args()

    rows = read_rows(args.txt)
    tmx = build_tmx(rows, args.txt.stem)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(tmx, encoding="utf-8")
    print(f"Wrote {args.output}")


if __name__ == "__main__":
    main()
