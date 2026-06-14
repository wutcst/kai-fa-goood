#!/usr/bin/env python3
"""
Import a Tiled map folder into the standard project layout:

  assets/maps/levelXX.tmx
  assets/maps/levelXX/          (tilesets + images)
  assets/levels/levelXX_collision.txt  (via export_level.py)

Usage:
  python tools/import_tiled_level.py "C:/Users/18037/Downloads/新建文件夹/新建文件夹" --level 1
"""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

FILE_MAP = {
    "Terrain (16x16).tsj": ("terrain.tsj", "terrain.png"),
    "Terrain (16x16).png": ("terrain.png", None),
    "Apple.tsj": ("apple.tsj", "apple.png"),
    "Apple.png": ("apple.png", None),
    "player-frog.tsj": ("player-frog.tsj", "player-frog.png"),
    "player-frog.png": ("player-frog.png", None),
    "Brown.png": ("background.png", None),
}

TILESET_SOURCES = [
    ("terrain.tsj", 1),
    ("apple.tsj", 243),
    ("player-frog.tsj", 260),
]


def level_tag(level: int) -> str:
    return f"level{level:02d}"


def find_source_dir(path: Path) -> Path:
    path = path.resolve()
    if not path.exists():
        raise SystemExit(f"Source not found: {path}")
    if (path / "map1.tmj").exists() or (path / "Terrain (16x16).tsj").exists():
        return path
    nested = path / "新建文件夹"
    if nested.is_dir() and (nested / "map1.tmj").exists():
        return nested
    raise SystemExit(f"No Tiled map files found under: {path}")


def write_tsj(dest: Path, image_name: str) -> None:
    data = json.loads(dest.read_text(encoding="utf-8"))
    data["image"] = image_name
    dest.write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding="utf-8")


def build_tmx(level: int, width: int, height: int, layer_csv: list[int], objects_xml: str) -> str:
    tag = level_tag(level)
    folder = tag
    csv_lines = []
    row = []
    for i, gid in enumerate(layer_csv):
        row.append(str(gid))
        if len(row) == width:
            csv_lines.append(",".join(row) + ",")
            row = []
    csv_body = "\n".join(csv_lines)

    tilesets = "\n".join(
        f' <tileset firstgid="{gid}" source="{folder}/{name}"/>' for name, gid in TILESET_SOURCES
    )

    return f"""<?xml version="1.0" encoding="UTF-8"?>
<map version="1.10" tiledversion="1.12.2" orientation="orthogonal" renderorder="right-down" width="{width}" height="{height}" tilewidth="16" tileheight="16" infinite="0" nextlayerid="5" nextobjectid="25">
{tilesets}
 <imagelayer id="2" name="Background" repeatx="1" repeaty="1">
  <image source="{folder}/background.png" width="64" height="64"/>
 </imagelayer>
 <layer id="1" name="碰撞" width="{width}" height="{height}">
  <data encoding="csv">
{csv_body}
</data>
 </layer>
 <objectgroup id="3" name="Objects">
{objects_xml}
 </objectgroup>
</map>
"""


def objects_from_tmj(tmj: dict) -> str:
    lines: list[str] = []
    for layer in tmj.get("layers", []):
        if layer.get("type") != "objectgroup":
            continue
        for obj in layer.get("objects", []):
            if "gid" not in obj:
                continue
            name = obj.get("name", "")
            lines.append(
                '  <object id="{id}" name="{name}" gid="{gid}" x="{x}" y="{y}" width="{w}" height="{h}"/>'.format(
                    id=obj.get("id", 0),
                    name=name,
                    gid=obj["gid"],
                    x=obj.get("x", 0),
                    y=obj.get("y", 0),
                    w=obj.get("width", 32),
                    h=obj.get("height", 32),
                )
            )
    return "\n".join(lines)


def objects_from_tmx(tmx_text: str) -> str:
    match = re.search(r"<objectgroup[^>]*>(.*?)</objectgroup>", tmx_text, re.S)
    if not match:
        return ""
    body = match.group(1)
    return "\n".join(" " + line.strip() for line in body.strip().splitlines() if line.strip())


def collision_csv_from_tmj(tmj: dict) -> tuple[int, int, list[int]]:
    width = int(tmj.get("width", 0))
    height = int(tmj.get("height", 0))
    for layer in tmj.get("layers", []):
        if layer.get("type") == "tilelayer" and layer.get("data"):
            return width, height, [int(v) for v in layer["data"]]
    raise SystemExit("No tile layer data found in .tmj")


def write_tiled_project(maps_dir: Path) -> None:
    project = {
        "automappingRulesFile": "",
        "commands": [],
        "compatibilityVersion": 1100,
        "extensionsPath": "extensions",
        "folders": ["."],
        "properties": [],
        "propertyTypes": [],
    }
    (maps_dir / "fire-ice.tiled-project").write_text(
        json.dumps(project, indent=4) + "\n", encoding="utf-8"
    )


def import_level(source_dir: Path, level: int) -> None:
    tag = level_tag(level)
    maps_dir = ROOT / "assets" / "maps"
    level_dir = maps_dir / tag
    tmx_path = maps_dir / f"{tag}.tmx"

    if level_dir.exists():
        shutil.rmtree(level_dir)
    level_dir.mkdir(parents=True)

    for src_name, (dest_name, tsj_image) in FILE_MAP.items():
        src = source_dir / src_name
        if not src.exists():
            print(f"Skip missing: {src_name}")
            continue
        dest = level_dir / dest_name
        shutil.copy2(src, dest)
        if dest.suffix == ".tsj" and tsj_image:
            write_tsj(dest, tsj_image)
        print(f"Copied {src_name} -> {tag}/{dest_name}")

    tmj_path = source_dir / "map1.tmj"
    existing_tmx = ROOT / "assets" / "maps" / "map1.tmx"

    if tmj_path.exists():
        tmj = json.loads(tmj_path.read_text(encoding="utf-8"))
        width, height, csv = collision_csv_from_tmj(tmj)
        objects_xml = objects_from_tmj(tmj)
        tmx_path.write_text(build_tmx(level, width, height, csv, objects_xml), encoding="utf-8")
        print(f"Generated {tmx_path.name} from map1.tmj")
    elif existing_tmx.exists():
        text = existing_tmx.read_text(encoding="utf-8")
        text = text.replace("map1/", f"{tag}/")
        text = text.replace("Terrain (16x16).tsj", "terrain.tsj")
        text = text.replace("Apple.tsj", "apple.tsj")
        text = text.replace("Brown.png", "background.png")
        text = re.sub(r'name="图像图层 1"', 'name="Background"', text)
        text = re.sub(r'name="对象层 1"', 'name="Objects"', text)
        tmx_path.write_text(text, encoding="utf-8")
        print(f"Generated {tmx_path.name} from legacy map1.tmx")
    else:
        raise SystemExit("Need map1.tmj or assets/maps/map1.tmx as source")

    write_tiled_project(maps_dir)

    export_script = ROOT / "tools" / "export_level.py"
    subprocess.run(
        [sys.executable, str(export_script), str(tmx_path)],
        cwd=ROOT,
        check=True,
    )


def cleanup_legacy_map1() -> None:
    maps_dir = ROOT / "assets" / "maps"
    levels_dir = ROOT / "assets" / "levels"
    for path in [
        maps_dir / "map1.tmx",
        maps_dir / "maps1.tiled-project",
        maps_dir / "map1",
        levels_dir / "map1_collision.txt",
    ]:
        if path.is_dir():
            shutil.rmtree(path)
            print(f"Removed {path}")
        elif path.exists():
            path.unlink()
            print(f"Removed {path}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Import Tiled map into levelXX layout")
    parser.add_argument("source", type=Path, help="Tiled source folder")
    parser.add_argument("--level", type=int, default=1, help="Level number (default: 1)")
    parser.add_argument("--keep-legacy", action="store_true", help="Keep old map1 files")
    args = parser.parse_args()

    source_dir = find_source_dir(args.source)
    import_level(source_dir, args.level)
    if not args.keep_legacy and args.level == 1:
        cleanup_legacy_map1()
    print(f"Done. Open assets/maps/fire-ice.tiled-project and edit {level_tag(args.level)}.tmx")


if __name__ == "__main__":
    main()
