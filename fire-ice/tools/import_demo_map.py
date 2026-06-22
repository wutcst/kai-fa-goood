#!/usr/bin/env python3
"""Import a cropped region from Pixel Adventure sample map as level09 demo."""

from __future__ import annotations

import argparse
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def parse_csv_layer(layer: ET.Element, width: int, height: int) -> list[int]:
    data = layer.find("data")
    if data is None:
        return [0] * (width * height)
    text = (data.text or "").replace("\n", "").strip()
    if not text:
        return [0] * (width * height)
    return [int(v) for v in text.split(",") if v.strip()]


def crop_gids(gids: list[int], src_w: int, x: int, y: int, crop_w: int, crop_h: int) -> list[int]:
    out: list[int] = []
    for row in range(y, y + crop_h):
        start = row * src_w + x
        out.extend(gids[start:start + crop_w])
    return out


def build_demo_tmx(level: int, width: int, height: int, gids: list[int], spawn_x: int, spawn_y: int) -> str:
    tag = f"level{level:02d}"
    rows: list[str] = []
    for row_y in range(height):
        row = gids[row_y * width:(row_y + 1) * width]
        line = ",".join(str(g) for g in row)
        if row_y + 1 < height:
            line += ","
        rows.append(line)
    csv_body = "\n".join(rows)

    spawn_px = spawn_x * 16
    spawn_py = spawn_y * 16

    return f"""<?xml version="1.0" encoding="UTF-8"?>
<map version="1.10" tiledversion="1.12.2" orientation="orthogonal" renderorder="right-down" width="{width}" height="{height}" tilewidth="16" tileheight="16" infinite="0" nextlayerid="5" nextobjectid="10">
 <tileset firstgid="1" source="{tag}/Terrain (16x16).tsx"/>
 <tileset firstgid="321" source="{tag}/player1.tsx"/>
 <imagelayer id="2" name="Background" repeatx="1" repeaty="1">
  <image source="{tag}/background.png" width="395" height="63"/>
 </imagelayer>
 <layer id="1" name="碰撞" width="{width}" height="{height}">
  <data encoding="csv">
{csv_body}
</data>
 </layer>
 <objectgroup id="3" name="Objects">
  <object id="1" name="player1" gid="321" x="{spawn_px}" y="{spawn_py}" width="32" height="32"/>
  <object id="2" name="player2" gid="321" x="{spawn_px + 48}" y="{spawn_py}" width="32" height="32"/>
 </objectgroup>
</map>
"""


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("source_tmx", type=Path)
    parser.add_argument("--level", type=int, default=9)
    parser.add_argument("--x", type=int, default=0)
    parser.add_argument("--y", type=int, default=78)
    parser.add_argument("--width", type=int, default=50)
    parser.add_argument("--height", type=int, default=22)
    parser.add_argument("--spawn-x", type=int, default=20)
    parser.add_argument("--spawn-y", type=int, default=18)
    args = parser.parse_args()

    level = args.level
    tag = f"level{level:02d}"
    maps_dir = ROOT / "assets" / "maps"
    level_dir = maps_dir / tag
    level_dir.mkdir(parents=True, exist_ok=True)

    # Reuse level02 tileset stubs + background (same Pixel Adventure shared tilesets)
    src_level_dir = maps_dir / "level02"
    for name in [
        "Terrain (16x16).tsx",
        "Idle.tsx",
        "player1.tsx",
        "background.png",
    ]:
        src = src_level_dir / name
        if src.exists():
            (level_dir / name).write_bytes(src.read_bytes())

    root = ET.parse(args.source_tmx).getroot()
    src_w = int(root.get("width", "0"))
    src_h = int(root.get("height", "0"))
    layer = next(l for l in root.findall("layer") if l.get("name") == "map")
    gids = parse_csv_layer(layer, src_w, src_h)
    cropped = crop_gids(gids, src_w, args.x, args.y, args.width, args.height)

    tmx_path = maps_dir / f"{tag}.tmx"
    tmx_path.write_text(
        build_demo_tmx(level, args.width, args.height, cropped, args.spawn_x, args.spawn_y),
        encoding="utf-8",
    )
    print(f"Wrote {tmx_path}")

    collision_path = ROOT / "assets" / "levels" / f"{tag}_collision.txt"
    subprocess.run(
        [sys.executable, str(Path(__file__).parent / "export_level.py"), str(tmx_path), "-o", str(collision_path)],
        check=True,
    )


if __name__ == "__main__":
    main()
