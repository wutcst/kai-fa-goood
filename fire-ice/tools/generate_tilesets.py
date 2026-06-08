#!/usr/bin/env python3
"""Generate placeholder logic + forest tileset PNG/TSX for Tiled."""

from __future__ import annotations

from pathlib import Path

from png_util import checker_tile, solid_tile, write_tile_strip

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "tilesets"


def write_logic_tsx() -> None:
    tiles = [
        ("solid", "#46372d"),
        ("empty", "#2d4a2d"),
        ("lava", "#dc4620"),
        ("water", "#3c8cff"),
        ("fire_door", "#8b4513"),
        ("water_door", "#4682b4"),
        ("fire_exit", "#ff6028"),
        ("water_exit", "#28a0ff"),
        ("gem", "#ffdc3c"),
        ("fire_spawn", "#ff8040"),
        ("water_spawn", "#4080ff"),
        ("button", "#c8c832"),
    ]
    png_tiles = []
    for name, hex_color in tiles:
        r = int(hex_color[1:3], 16)
        g = int(hex_color[3:5], 16)
        b = int(hex_color[5:7], 16)
        if name == "empty":
            png_tiles.append(checker_tile((r, g, b), (r + 10, g + 10, b + 10)))
        else:
            png_tiles.append(solid_tile(r, g, b))

    write_tile_strip(str(OUT / "logic_tiles.png"), png_tiles)

    props = []
    for idx, (name, _) in enumerate(tiles):
        props.append(
            f'  <tile id="{idx}">\n'
            f'   <properties>\n'
            f'    <property name="logic" value="{name}"/>\n'
            f"   </properties>\n"
            f"  </tile>"
        )

    tsx = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<tileset version="1.10" tiledversion="1.10.2" name="logic" '
        'tilewidth="32" tileheight="32" tilecount="12" columns="12">\n'
        ' <image source="logic_tiles.png" width="384" height="32"/>\n'
        + "\n".join(props)
        + "\n</tileset>\n"
    )
    (OUT / "logic.tsx").write_text(tsx, encoding="utf-8")


def write_forest_tsx() -> None:
    tiles = [
        checker_tile((34, 58, 34), (42, 70, 42)),  # floor
        checker_tile((70, 55, 45), (82, 65, 52)),  # wall
        solid_tile(220, 70, 30),  # lava decor
        solid_tile(60, 140, 255),  # water decor
        solid_tile(90, 120, 70),  # moss decor
        solid_tile(120, 90, 60),  # wood decor
    ]
    write_tile_strip(str(OUT / "forest_tiles.png"), tiles)

    tsx = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<tileset version="1.10" tiledversion="1.10.2" name="forest" '
        'tilewidth="32" tileheight="32" tilecount="6" columns="6">\n'
        ' <image source="forest_tiles.png" width="192" height="32"/>\n'
        "</tileset>\n"
    )
    (OUT / "forest.tsx").write_text(tsx, encoding="utf-8")


def main() -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    write_logic_tsx()
    write_forest_tsx()
    print(f"Tilesets written to {OUT}")


if __name__ == "__main__":
    main()
