#!/usr/bin/env python3
"""Reorganize assets/maps: shared tilesets, levelXX layout, fixed paths."""

from __future__ import annotations

import json
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAPS = ROOT / "assets" / "maps"
LEVELS = ROOT / "assets" / "levels"
BUILD_MAPS = ROOT / "build" / "Release" / "maps"
LEGACY_MAPS1 = ROOT / "maps1"

SHARED_DIRS = (
    "Background",
    "Items",
    "Terrain",
    "Traps",
    "Main Characters",
    "Menu",
    "Other",
)

MAP1_FILE_MAP = {
    "Terrain (16x16).tsj": ("terrain.tsj", "terrain.png"),
    "Terrain (16x16).png": ("terrain.png", None),
    "Apple.tsj": ("apple.tsj", "apple.png"),
    "Apple.png": ("apple.png", None),
    "player-frog.tsj": ("player-frog.tsj", "player-frog.png"),
    "player-frog.png": ("player-frog.png", None),
    "Brown.png": ("background.png", None),
}

MAP1_TILESETS = [
    ("terrain.tsj", 1),
    ("apple.tsj", 243),
    ("player-frog.tsj", 260),
]

LEVEL2_TSX = [
    "Terrain (16x16).tsx",
    "Idle.tsx",
    "On (24x8).tsx",
    "Sand Mud Ice (16x6).tsx",
    "Mud Particle.tsx",
    "Bananas.tsx",
    "player1.tsx",
]

LEVEL3_TSX = [
    "Terrain (16x16).tsx",
    "Idle.tsx",
    "Spiked Ball.tsx",
    "rock head.tsx",
    "Kiwi.tsx",
    "On (38x38).tsx",
    "Chain.tsx",
]


def write_tsj(dest: Path, image_name: str) -> None:
    data = json.loads(dest.read_text(encoding="utf-8"))
    data["image"] = image_name
    dest.write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding="utf-8")


def fix_tsx_image_paths(tsx_path: Path) -> None:
    text = tsx_path.read_text(encoding="utf-8")
    text = re.sub(
        r'source="(Background|Items|Terrain|Traps|Main Characters|Menu|Other)/',
        r'source="../tilesets/\1/',
        text,
    )
    tsx_path.write_text(text, encoding="utf-8")


def copy_tree_merge(src: Path, dst: Path) -> None:
    if not src.exists():
        return
    for item in src.rglob("*"):
        if item.is_dir():
            continue
        rel = item.relative_to(src)
        target = dst / rel
        target.parent.mkdir(parents=True, exist_ok=True)
        if not target.exists():
            shutil.copy2(item, target)


def setup_shared_tilesets() -> None:
    tilesets_dir = MAPS / "tilesets"
    if tilesets_dir.exists():
        shutil.rmtree(tilesets_dir)
    tilesets_dir.mkdir(parents=True)

    for name in SHARED_DIRS:
        copy_tree_merge(MAPS / "map2" / name, tilesets_dir / name)
        copy_tree_merge(MAPS / "map3" / name, tilesets_dir / name)

    print(f"Shared tilesets -> {tilesets_dir.relative_to(ROOT)}")


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


def collision_csv_from_tmj(tmj: dict) -> tuple[int, int, list[int]]:
    width = int(tmj.get("width", 0))
    height = int(tmj.get("height", 0))
    for layer in tmj.get("layers", []):
        if layer.get("type") == "tilelayer" and layer.get("data"):
            name = layer.get("name", "")
            if name in ("碰撞", "Collision", "图块层 1") or layer.get("data"):
                return width, height, [int(v) for v in layer["data"]]
    raise SystemExit("No tile layer data found in .tmj")


def build_level01_tmx(width: int, height: int, layer_csv: list[int], objects_xml: str) -> str:
    csv_lines = []
    row: list[str] = []
    for i, gid in enumerate(layer_csv):
        row.append(str(gid))
        if len(row) == width:
            csv_lines.append(",".join(row) + ",")
            row = []
    csv_body = "\n".join(csv_lines)
    tilesets = "\n".join(
        f' <tileset firstgid="{gid}" source="level01/{name}"/>' for name, gid in MAP1_TILESETS
    )
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<map version="1.10" tiledversion="1.12.2" orientation="orthogonal" renderorder="right-down" width="{width}" height="{height}" tilewidth="16" tileheight="16" infinite="0" nextlayerid="5" nextobjectid="25">
{tilesets}
 <imagelayer id="2" name="Background" repeatx="1" repeaty="1">
  <image source="level01/background.png" width="64" height="64"/>
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


def restore_level_assets() -> None:
    """Ensure per-level PNG files exist (tsj/tsx alone are not enough at runtime)."""
    copies = [
        (MAPS / "tilesets" / "Terrain" / "Terrain (16x16).png", MAPS / "level01" / "terrain.png"),
        (MAPS / "tilesets" / "Items" / "Fruits" / "Apple.png", MAPS / "level01" / "apple.png"),
        (MAPS / "tilesets" / "Background" / "Brown.png", MAPS / "level01" / "background.png"),
        (MAPS / "sources" / "map1" / "player-frog.png", MAPS / "level01" / "player-frog.png"),
        (MAPS / "tilesets" / "Background" / "Yellow.png", MAPS / "level02" / "background.png"),
        (MAPS / "sources" / "map2" / "player1.png", MAPS / "level02" / "player1.png"),
        (MAPS / "tilesets" / "Background" / "Green.png", MAPS / "level03" / "background.png"),
    ]
    for src, dst in copies:
        if not src.exists():
            print(f"  skip missing asset: {src.relative_to(ROOT)}")
            continue
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        print(f"  {dst.relative_to(ROOT)} <- {src.relative_to(ROOT)}")


def import_map1_to_level01() -> None:
    source = MAPS / "map1"
    level_dir = MAPS / "level01"
    tmx_path = MAPS / "level01.tmx"

    if level_dir.exists():
        shutil.rmtree(level_dir)
    level_dir.mkdir(parents=True)

    for src_name, (dest_name, tsj_image) in MAP1_FILE_MAP.items():
        src = source / src_name
        if not src.exists():
            if dest_name == "terrain.png":
                fallback = MAPS / "tilesets" / "Terrain" / "Terrain (16x16).png"
                if fallback.exists():
                    shutil.copy2(fallback, level_dir / dest_name)
                    print(f"  terrain.png <- tilesets/Terrain")
                    continue
            if dest_name == "apple.png":
                fallback = MAPS / "tilesets" / "Items" / "Fruits" / "Apple.png"
                if fallback.exists():
                    shutil.copy2(fallback, level_dir / dest_name)
                    print(f"  apple.png <- tilesets/Items/Fruits")
                    continue
            if dest_name == "background.png":
                fallback = MAPS / "tilesets" / "Background" / "Brown.png"
                if fallback.exists():
                    shutil.copy2(fallback, level_dir / dest_name)
                    print(f"  background.png <- tilesets/Background")
                    continue
            if dest_name == "player-frog.png":
                fallback = MAPS / "sources" / "map1" / "player-frog.png"
                if fallback.exists():
                    shutil.copy2(fallback, level_dir / dest_name)
                    print(f"  player-frog.png <- sources/map1")
                    continue
            print(f"  skip missing map1/{src_name}")
            continue
        dest = level_dir / dest_name
        shutil.copy2(src, dest)
        if dest.suffix == ".tsj" and tsj_image:
            write_tsj(dest, tsj_image)

    tmj_path = source / "map1.tmj"
    tmj = json.loads(tmj_path.read_text(encoding="utf-8"))
    width, height, csv = collision_csv_from_tmj(tmj)
    objects_xml = objects_from_tmj(tmj)
    tmx_path.write_text(build_level01_tmx(width, height, csv, objects_xml), encoding="utf-8")
    print(f"Generated level01.tmx ({width}x{height})")


def convert_map_tmx(source_dir: Path, level: int, tsx_files: list[str], bg_src: str) -> None:
    tag = f"level{level:02d}"
    level_dir = MAPS / tag
    tmx_src = source_dir / f"map{level}.tmx"
    tmx_dst = MAPS / f"{tag}.tmx"

    if level_dir.exists():
        shutil.rmtree(level_dir)
    level_dir.mkdir(parents=True)

    for tsx_name in tsx_files:
        src = source_dir / tsx_name
        if src.exists():
            shutil.copy2(src, level_dir / tsx_name)
            fix_tsx_image_paths(level_dir / tsx_name)

    bg_source = source_dir / bg_src
    bg_fallback = MAPS / "tilesets" / "Background" / ("Yellow.png" if level == 2 else "Green.png")
    if bg_source.exists():
        shutil.copy2(bg_source, level_dir / "background.png")
    elif bg_fallback.exists():
        shutil.copy2(bg_fallback, level_dir / "background.png")

    player_png = source_dir / "player1.png"
    player_fallback = MAPS / "sources" / f"map{level}" / "player1.png"
    if player_png.exists():
        shutil.copy2(player_png, level_dir / "player1.png")
    elif player_fallback.exists():
        shutil.copy2(player_fallback, level_dir / "player1.png")
        text = (level_dir / "player1.tsx").read_text(encoding="utf-8")
        text = text.replace('source="player1.png"', 'source="player1.png"')
        (level_dir / "player1.tsx").write_text(text, encoding="utf-8")

    text = tmx_src.read_text(encoding="utf-8")
    for tsx_name in tsx_files:
        text = text.replace(f'source="{tsx_name}"', f'source="{tag}/{tsx_name}"')
    text = re.sub(r'name="图像图层 1"', 'name="Background"', text)
    text = re.sub(r'name="图块层 1"', 'name="碰撞"', text)
    text = re.sub(r'name="对象层 1"', 'name="Objects"', text)
    text = re.sub(
        rf'<image source="{re.escape(bg_src)}"',
        f'<image source="{tag}/background.png"',
        text,
    )
    tmx_dst.write_text(text, encoding="utf-8")
    print(f"Generated {tag}.tmx from map{level}")


def restore_levels_from_build(start: int = 4, end: int = 8) -> None:
    if not BUILD_MAPS.exists():
        print("Build maps not found, skip restore level04-08")
        return
    for level in range(start, end + 1):
        tag = f"level{level:02d}"
        src = BUILD_MAPS / f"{tag}.tmx"
        dst = MAPS / f"{tag}.tmx"
        if src.exists():
            shutil.copy2(src, dst)
            print(f"Restored {tag}.tmx from build")


def restore_level01_assets_from_build() -> None:
    src_dir = BUILD_MAPS / "level01"
    dst_dir = MAPS / "level01"
    if not src_dir.exists():
        return
    for name in ("terrain.tsj", "terrain.png", "apple.tsj", "apple.png", "player-frog.tsj", "player-frog.png", "background.png"):
        src = src_dir / name
        if src.exists() and not (dst_dir / name).exists():
            shutil.copy2(src, dst_dir / name)


def archive_sources() -> None:
    sources = MAPS / "sources"
    if sources.exists():
        shutil.rmtree(sources)
    sources.mkdir()

    for name in ("map1", "map2", "map3"):
        src = MAPS / name
        if not src.exists():
            continue
        dst = sources / name
        shutil.copytree(src, dst)
        print(f"Archived sources/{name}")


def cleanup_legacy() -> None:
    for path in (MAPS / "map1", MAPS / "map2", MAPS / "map3", LEGACY_MAPS1):
        if path.exists():
            shutil.rmtree(path)
            print(f"Removed {path.relative_to(ROOT)}")


def write_tiled_project() -> None:
    project = {
        "automappingRulesFile": "",
        "commands": [],
        "compatibilityVersion": 1100,
        "extensionsPath": "extensions",
        "folders": [".", "tilesets", "level01", "level02", "level03", "sources"],
        "properties": [],
        "propertyTypes": [],
    }
    (MAPS / "fire-ice.tiled-project").write_text(
        json.dumps(project, indent=4) + "\n", encoding="utf-8"
    )


def write_readme() -> None:
    readme = """# Maps 目录说明

## 结构

```
assets/maps/
├── fire-ice.tiled-project   # Tiled 工程入口
├── level01.tmx … level08.tmx   # 游戏关卡地图
├── level01/ … level03/         # 各关卡专用图块集
├── tilesets/                   # 共享 Pixel Adventure 素材
│   ├── Background/
│   ├── Items/
│   ├── Terrain/
│   ├── Traps/
│   ├── Main Characters/
│   ├── Menu/
│   └── Other/
└── sources/                    # 原始 Tiled 工程归档（map1/2/3）
```

## 编辑流程

1. 用 Tiled 打开 `fire-ice.tiled-project`
2. 编辑 `levelXX.tmx`（保存为 `.tmx` 格式）
3. 导出碰撞：`python tools/export_level.py assets/maps/levelXX.tmx`
4. 重新编译运行游戏

## 路径约定

- 关卡地图引用 `levelXX/` 下的图块集
- 共享素材统一放在 `tilesets/`，图块集内用 `../tilesets/...` 引用
- 碰撞层命名：`碰撞` 或 `Collision`
- 对象层命名：`Objects`；`player1`/`player2` 为出生点
"""
    (MAPS / "README.md").write_text(readme, encoding="utf-8")


def export_collisions() -> None:
    export_script = ROOT / "tools" / "export_level.py"
    for level in range(1, 9):
        tmx = MAPS / f"level{level:02d}.tmx"
        if not tmx.exists():
            continue
        subprocess.run([sys.executable, str(export_script), str(tmx)], cwd=ROOT, check=True)


def main() -> None:
    print("=== 1. Shared tilesets ===")
    setup_shared_tilesets()

    print("\n=== 2. Import map1 -> level01 ===")
    import_map1_to_level01()

    print("\n=== 3. Convert map2 -> level02 ===")
    convert_map_tmx(MAPS / "map2", 2, LEVEL2_TSX, "Yellow.png")

    print("\n=== 4. Convert map3 -> level03 ===")
    convert_map_tmx(MAPS / "map3", 3, LEVEL3_TSX, "Green.png")

    print("\n=== 5. Restore level04-08 ===")
    restore_levels_from_build(4, 8)

    print("\n=== 6. Archive & cleanup ===")
    archive_sources()
    cleanup_legacy()
    write_tiled_project()
    write_readme()

    print("\n=== 7. Restore level PNG assets ===")
    restore_level_assets()

    print("\n=== 8. Export collisions ===")
    export_collisions()

    print("\nDone. Open assets/maps/fire-ice.tiled-project in Tiled.")


if __name__ == "__main__":
    main()
