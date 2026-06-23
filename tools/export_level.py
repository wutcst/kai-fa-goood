#!/usr/bin/env python3
"""
Export Tiled .tmx collision (+ optional object spawns) to ASCII level file.

Usage:
  python tools/export_level.py assets/maps/level01.tmx
  python tools/export_level.py assets/maps/level01.tmx -o assets/levels/level01_collision.txt
"""

from __future__ import annotations

import argparse
import json
import xml.etree.ElementTree as ET
from pathlib import Path

LOGIC_TO_CHAR = {
    "solid": "#",
    "oneway": "^",
    "thin": "t",  # 薄横条；Physics 用较低顶面碰撞
    "vanishing": "~",
    "spike": "S",
    "empty": ".",
    "lava": "L",
    "water": "W",
    "fire_door": "F",
    "water_door": "I",
    "fire_exit": "E",
    "water_exit": "X",
    "gem": "G",
    "fire_spawn": "f",
    "water_spawn": "w",
    "button": "B",
    "acid": "A",
}

COLLISION_LAYER_NAMES = ("Collision", "碰撞")


def parse_tileset_tsx(tsx_path: Path) -> dict[int, str]:
    root = ET.parse(tsx_path).getroot()
    mapping: dict[int, str] = {}
    for tile in root.findall("tile"):
        tile_id = int(tile.get("id", "0"))
        props = tile.find("properties")
        if props is None:
            continue
        for prop in props.findall("property"):
            name = prop.get("name")
            value = prop.get("value")
            if name == "logic" and value:
                mapping[tile_id] = str(value)
            elif name == "oneway" and value in ("true", "1", True):
                mapping[tile_id] = "oneway"
            elif name == "vanishing" and value in ("true", "1", True):
                mapping[tile_id] = "vanishing"
            elif name == "spike" and value in ("true", "1", True):
                mapping[tile_id] = "spike"
            elif name == "solid" and value in ("true", "1", True):
                mapping[tile_id] = "solid"
    return mapping


def parse_tileset_tsj(tsj_path: Path) -> dict[int, str]:
    data = json.loads(tsj_path.read_text(encoding="utf-8"))
    mapping: dict[int, str] = {}
    for tile in data.get("tiles", []):
        tile_id = int(tile.get("id", 0))
        for prop in tile.get("properties", []):
            name = prop.get("name")
            value = prop.get("value")
            if name == "logic" and value:
                mapping[tile_id] = str(value)
            elif name == "oneway" and value:
                mapping[tile_id] = "oneway"
            elif name == "vanishing" and value:
                mapping[tile_id] = "vanishing"
            elif name == "spike" and value:
                mapping[tile_id] = "spike"
            elif name == "solid" and value:
                mapping[tile_id] = "solid"
    return mapping


def parse_tileset(path: Path) -> dict[int, str]:
    suffix = path.suffix.lower()
    if suffix == ".tsx":
        return parse_tileset_tsx(path)
    if suffix == ".tsj":
        return parse_tileset_tsj(path)
    return {}


def load_tilesets(map_root: ET.Element, map_dir: Path) -> list[tuple[int, dict[int, str], str]]:
    tilesets: list[tuple[int, dict[int, str], str]] = []
    for ts in map_root.findall("tileset"):
        first_gid = int(ts.get("firstgid", "1"))
        source = ts.get("source")
        if not source:
            continue
        tileset_path = (map_dir / source).resolve()
        if not tileset_path.exists():
            continue
        tilesets.append((first_gid, parse_tileset(tileset_path), source.replace("\\", "/")))
    tilesets.sort(key=lambda item: item[0])
    return tilesets


def gid_to_local(gid: int, tilesets: list[tuple[int, dict[int, str], str]]) -> tuple[str, int] | tuple[None, None]:
    if gid == 0:
        return None, None
    chosen_source = None
    local_id = 0
    for first_gid, _mapping, source in tilesets:
        if gid >= first_gid:
            chosen_source = source
            local_id = gid - first_gid
        else:
            break
    if chosen_source is None:
        return None, None
    return chosen_source, local_id


def classify_by_tileset(source: str, local_id: int) -> str | None:
    source_lower = source.lower()
    if "idle" in source_lower:
        return "spike"
    if "on (24x8)" in source_lower:
        return "empty"
    if "sand mud ice" in source_lower:
        if local_id in {4, 6}:
            return "empty"
        if local_id in {26, 28, 37, 38, 48, 49}:
            return "vanishing"
        return None
    if "terrain" in source_lower:
        if local_id in {61, 62, 63}:
            return "vanishing"
        if local_id in {105, 106, 107, 149, 150, 151}:
            return "solid"
        # 薄横条平台（仅 level02 地形集中的横条图块）
        if "level02/terrain" in source_lower.replace("\\", "/") and local_id in {40, 41, 95}:
            return "thin"
        # Temple Gates 等关卡：地形图块中的门扇（gid 102/125/147 → local 101/124/146）
        if local_id == 101:
            return "fire_door"
        if local_id == 124:
            return "water_door"
        if local_id == 146:
            return "poison_door"
    return None


def gid_to_logic(gid: int, tilesets: list[tuple[int, dict[int, str], str]]) -> str | None:
    if gid == 0:
        return "empty"
    chosen = None
    local_id = 0
    source = ""
    for first_gid, mapping, src in tilesets:
        if gid >= first_gid:
            chosen = mapping
            local_id = gid - first_gid
            source = src
        else:
            break
    if chosen is not None:
        # 图块集规则优先于 tile 自定义 solid 属性（避免横条被标成整格 solid）
        inferred = classify_by_tileset(source, local_id)
        if inferred:
            return inferred
        if local_id in chosen:
            return chosen[local_id]
    source, local_id = gid_to_local(gid, tilesets)
    if source is None:
        return None
    return classify_by_tileset(source, local_id)


def parse_csv_data(layer: ET.Element) -> list[int]:
    data = layer.find("data")
    if data is None:
        return []
    text = (data.text or "").replace("\n", "").strip()
    if not text:
        return []
    return [int(v) for v in text.split(",") if v.strip()]


def layer_by_name(map_root: ET.Element, name: str) -> ET.Element | None:
    for layer in map_root.findall("layer"):
        if layer.get("name") == name:
            return layer
    return None


def find_collision_layer(map_root: ET.Element) -> ET.Element:
    for name in COLLISION_LAYER_NAMES:
        layer = layer_by_name(map_root, name)
        if layer is not None:
            return layer
    raise SystemExit(f"Missing collision layer (expected one of: {', '.join(COLLISION_LAYER_NAMES)})")


def nearest_empty_tile(rows: list[list[str]], tx: int, ty: int) -> tuple[int, int] | None:
    height = len(rows)
    width = len(rows[0]) if rows else 0
    for radius in range(0, max(width, height)):
        for dy in range(-radius, radius + 1):
            for dx in range(-radius, radius + 1):
                if abs(dx) != radius and abs(dy) != radius and radius > 0:
                    continue
                x = tx + dx
                y = ty + dy
                if 0 <= y < height and 0 <= x < width and rows[y][x] == ".":
                    return x, y
    return None


def seal_map_borders(rows: list[list[str]]) -> None:
    """Force solid walls on the outermost ring so players cannot fall out."""
    if not rows:
        return
    height = len(rows)
    width = len(rows[0])
    for x in range(width):
        if rows[0][x] == ".":
            rows[0][x] = "#"
        if rows[height - 1][x] in (".", "^"):
            rows[height - 1][x] = "#"
    for y in range(height):
        if rows[y][0] == ".":
            rows[y][0] = "#"
        if rows[y][width - 1] == ".":
            rows[y][width - 1] = "#"


def is_button_object(obj: ET.Element, tilesets: list[tuple[int, dict[int, str], str]]) -> bool:
    obj_type = (obj.get("type") or obj.get("name") or "").lower()
    if "button" in obj_type or obj_type in ("pressure", "plate"):
        return True
    gid = int(obj.get("gid", "0") or "0")
    if gid == 0:
        return False
    source, _local_id = gid_to_local(gid, tilesets)
    if source and "on (" in source.lower():
        return True
    return False


def object_anchor_tile(obj: ET.Element, tile_width: int, tile_height: int) -> tuple[int, int]:
    x = float(obj.get("x", "0"))
    y = float(obj.get("y", "0"))
    w = float(obj.get("width", tile_width))
    # Tiled 对象 y 为底边；取底边所在格作为按钮落点
    tx = int((x + w * 0.5) // tile_width)
    ty = int((y - 1) // tile_height)
    return tx, ty


def stamp_tile(rows: list[list[str]], tx: int, ty: int, char: str) -> None:
    if ty < 0 or ty >= len(rows) or tx < 0 or tx >= len(rows[ty]):
        return
    if rows[ty][tx] in (".", "^"):
        rows[ty][tx] = char
        return
    empty = nearest_empty_tile(rows, tx, ty)
    if empty is not None:
        ex, ey = empty
        rows[ey][ex] = char


def nearest_spawn_tile(rows: list[list[str]], tx: int, ty: int) -> tuple[int, int] | None:
    """Prefer an empty tile on the object's column, standing on solid ground below."""
    if not rows:
        return None
    height = len(rows)
    width = len(rows[0])
    start_y = max(0, min(ty, height - 1))
    for y in range(start_y, -1, -1):
        if not (0 <= tx < width):
            continue
        if rows[y][tx] != ".":
            continue
        if y + 1 < height and rows[y + 1][tx] in ("#", "^", "~"):
            return tx, y
    for y in range(start_y, -1, -1):
        if 0 <= tx < width and rows[y][tx] == ".":
            return tx, y
    return nearest_empty_tile(rows, tx, ty)


def apply_objects(
    rows: list[list[str]],
    map_root: ET.Element,
    tile_width: int,
    tile_height: int,
    tilesets: list[tuple[int, dict[int, str], str]],
) -> None:
    spawn_requests: list[tuple[str, int, int]] = []

    for obj_group in map_root.findall("objectgroup"):
        for obj in obj_group.findall("object"):
            obj_type = (obj.get("type") or obj.get("name") or "").lower()
            char = None
            if obj_type in ("fire_spawn", "fire", "player1", "player"):
                char = "f"
            elif obj_type in ("water_spawn", "water", "player2"):
                char = "w"
            elif obj_type in ("poison_spawn", "poison"):
                char = "p"
            elif is_button_object(obj, tilesets):
                tx, ty = object_anchor_tile(obj, tile_width, tile_height)
                stamp_tile(rows, tx, ty, "B")
                continue
            if char is None:
                continue
            x = float(obj.get("x", "0"))
            y = float(obj.get("y", "0"))
            w = float(obj.get("width", tile_width))
            h = float(obj.get("height", tile_height))
            # Tiled 对象 y 为底边；取脚底中心所在格作为出生点
            tx = int((x + w * 0.5) // tile_width)
            ty = int((y - 1) // tile_height)
            spawn_requests.append((char, tx, ty))

    for char, tx, ty in spawn_requests:
        spot = nearest_spawn_tile(rows, tx, ty) or nearest_empty_tile(rows, tx, ty)
        if spot is not None:
            x, y = spot
            rows[y][x] = char

    has_fire = any("f" in row for row in rows)
    has_water = any("w" in row for row in rows)
    if has_fire and not has_water:
        for y, row in enumerate(rows):
            for x, cell in enumerate(row):
                if cell == "f":
                    empty = nearest_empty_tile(rows, x + 1, y) or nearest_empty_tile(rows, x - 1, y)
                    if empty is not None:
                        ex, ey = empty
                        rows[ey][ex] = "w"
                    break
            if any("w" in row for row in rows):
                break


def export_tmx(tmx_path: Path, output_path: Path) -> None:
    map_root = ET.parse(tmx_path).getroot()
    width = int(map_root.get("width", "0"))
    height = int(map_root.get("height", "0"))
    tile_width = int(map_root.get("tilewidth", "32"))
    tile_height = int(map_root.get("tileheight", "32"))

    collision = find_collision_layer(map_root)
    tilesets = load_tilesets(map_root, tmx_path.parent)
    gids = parse_csv_data(collision)
    if len(gids) != width * height:
        raise SystemExit(f"Collision layer size mismatch: got {len(gids)}, expected {width * height}")

    rows: list[list[str]] = []
    for y in range(height):
        row: list[str] = []
        for x in range(width):
            gid = gids[y * width + x]
            if tilesets:
                logic = gid_to_logic(gid, tilesets)
                if logic is None:
                    # 碰撞层非空图块默认视为实心墙（装饰类 tileset 通常无 solid 属性）
                    row.append("." if gid == 0 else "#")
                else:
                    row.append(LOGIC_TO_CHAR.get(logic, "#"))
            else:
                row.append("." if gid == 0 else "#")
        rows.append(row)

    apply_objects(rows, map_root, tile_width, tile_height, tilesets)
    seal_map_borders(rows)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    lines = ["".join(row) for row in rows]
    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Exported {tmx_path.name} -> {output_path}")


def main() -> None:
    parser = argparse.ArgumentParser(description="Export Tiled collision layer to ASCII map")
    parser.add_argument("tmx", type=Path, help="Path to .tmx map file")
    parser.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Output .txt path (default: assets/levels/<stem>_collision.txt)",
    )
    args = parser.parse_args()

    tmx_path = args.tmx.resolve()
    if not tmx_path.exists():
        raise SystemExit(f"Not found: {tmx_path}")

    if args.output:
        output_path = args.output
    else:
        output_path = tmx_path.parents[1] / "levels" / f"{tmx_path.stem}_collision.txt"

    export_tmx(tmx_path, output_path)


if __name__ == "__main__":
    main()
