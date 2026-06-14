#!/usr/bin/env python3
"""
Professional dual-route level generator for Fire-Ice Online.

Locked: 42×19 shell, level01 terrain autotile, no pickups/traps.
Layout rules enforced in code + validate_layout().
"""

from __future__ import annotations

import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAPS_DIR = ROOT / "assets" / "maps"
LEVELS_DIR = ROOT / "assets" / "levels"
PREVIEW_DIR = ROOT / "assets" / "previews"
TERRAIN_DIR = ROOT / "assets" / "maps" / "level01"
ASSET_FOLDER = "level01"

INNER_W, INNER_H = 40, 17
MAP_W, MAP_H = INNER_W + 2, INNER_H + 2
RAIL_L, RAIL_R = 1, INNER_W - 2
CENTER_X0, CENTER_X1 = 14, 26
CENTER_Y0, CENTER_Y1 = 6, 11
MAX_PLAT_W = 6          # no long bars
MAX_EXIT_W = 11


class MapBuilder:
    def __init__(self) -> None:
        self.grid = [["." for _ in range(INNER_W)] for _ in range(INNER_H)]
        self._plats: list[tuple[int, int, int]] = []

    def _popup(self, x: int, y: int, w: int = 1, h: int = 1) -> bool:
        for dy in range(h):
            for dx in range(w):
                px, py = x + dx, y + dy
                if CENTER_X0 <= px <= CENTER_X1 and CENTER_Y0 <= py <= CENTER_Y1:
                    return True
        return False

    def plat(self, x: int, y: int, w: int, *, allow_wide: bool = False) -> None:
        limit = MAX_EXIT_W if allow_wide else MAX_PLAT_W
        if w > limit:
            raise ValueError(f"Platform too wide ({w}>{limit}) at y={y}")
        if x <= 2 and x + w >= INNER_W - 3:
            raise ValueError(f"Platform spans wall-to-wall at y={y}")
        if self._popup(x, y, w, 1):
            return
        for dx in range(w):
            if 0 <= x + dx < INNER_W and 0 <= y < INNER_H:
                self.grid[y][x + dx] = "#"
        self._plats.append((x, y, w))

    def cantilever(self, side: str, y: int, w: int) -> None:
        self.plat(2 if side == "L" else INNER_W - 2 - w, y, w)

    def island(self, x: int, y: int, w: int) -> None:
        self.plat(x, y, w)

    def z_run(self, segs: list[tuple[int, int, int]]) -> None:
        for x, y, w in segs:
            self.plat(x, y, w)

    def nest(self, top: tuple[int, int, int], under: tuple[int, int, int]) -> None:
        self.plat(*top)
        self.plat(*under)

    def slot(self, x: int, y: int, w: int = 3, h: int = 2) -> None:
        if self._popup(x, y, w, h):
            return
        for dx in range(w):
            self.grid[y][x + dx] = "#"
        for dy in range(1, h):
            self.grid[y + dy][x] = "#"
            self.grid[y + dy][x + w - 1] = "#"
        gap = max(2, w - 1)
        gx = x + (w - gap) // 2
        for dx in range(gap):
            self.grid[y + h - 1][gx + dx] = "."

    def switch(self, points: list[tuple[int, int, int]]) -> None:
        """Short crossover steps between near / far routes (width 2–3 only)."""
        for x, y, w in points:
            self.plat(x, y, min(w, 3))

    def put(self, x: int, y: int, ch: str) -> None:
        if 0 <= x < INNER_W and 0 <= y < INNER_H:
            self.grid[y][x] = ch

    def spawn_2p(self) -> None:
        self.put(0, 0, "f")
        self.put(INNER_W - 1, 0, "w")

    def spawn_3p(self) -> None:
        self.put(0, 0, "f")
        self.put(INNER_W // 2, 0, "p")
        self.put(INNER_W - 1, 0, "w")

    def exits(self, y: int, xe: int, xx: int, xp: int | None = None) -> None:
        self.put(xe, y, "E")
        self.put(xx, y, "X")
        if xp is not None:
            self.put(xp, y, "P")

    def shell(self) -> None:
        for y in range(1, INNER_H):
            self.grid[y][RAIL_L] = "#"
            self.grid[y][RAIL_R] = "#"

    def build(self) -> list[str]:
        return ["#" * MAP_W] + ["#" + "".join(r) + "#" for r in self.grid] + ["#" * MAP_W]


def _segments(row: str) -> list[tuple[int, int]]:
    out: list[tuple[int, int]] = []
    i = 0
    while i < len(row):
        if row[i] == "#":
            j = i
            while j < len(row) and row[j] == "#":
                j += 1
            out.append((i, j - i))
            i = j
        else:
            i += 1
    return out


def validate_layout(lines: list[str], poison: bool) -> None:
    if len(lines) != MAP_H or any(len(r) != MAP_W for r in lines):
        raise ValueError("Must be 42×19")
    if lines[0] != lines[-1] != "#" * MAP_W:
        raise ValueError("Border invalid")

    text = "\n".join(lines)
    for c in "fwEX":
        if c not in text:
            raise ValueError(f"Missing {c}")
    if poison and ("p" not in text or "P" not in text):
        raise ValueError("Missing poison markers")

    for y in range(CENTER_Y0, CENTER_Y1 + 1):
        inner = lines[y + 1][1:-1]
        for x in range(CENTER_X0, CENTER_X1 + 1):
            if inner[x] == "#":
                raise ValueError(f"Popup blocked at ({x},{y})")

    mirror = 0
    for y in range(INNER_H):
        if y >= 13:
            continue
        inner = lines[y + 1][1:-1]
        left = [s for s in _segments(inner) if s[0] < 11 and s[1] >= 3]
        right = [s for s in _segments(inner) if s[0] + s[1] > 29 and s[1] >= 3]
        if left and right:
            mirror += 1
    if mirror >= 3:
        raise ValueError(f"Mirror rows={mirror}")

    for y in range(INNER_H):
        inner = lines[y + 1][1:-1]
        for x, w in _segments(inner):
            if w > MAX_EXIT_W:
                raise ValueError(f"Segment width {w} at y={y}")
            if x <= 2 and x + w >= INNER_W - 3:
                raise ValueError(f"Wall-span at y={y}")

    plat_rows: list[int] = []
    for y in range(INNER_H):
        inner = lines[y + 1][1:-1]
        for x, w in _segments(inner):
            if x >= 3 and x + w <= INNER_W - 3 and w >= 2:
                plat_rows.append(y)
                break
    if len(plat_rows) >= 5:
        gaps = [plat_rows[i + 1] - plat_rows[i] for i in range(len(plat_rows) - 1)]
        if len(set(gaps)) < 2:
            raise ValueError("Platforms too evenly spaced")


def autotile_gid(grid: list[str], x: int, y: int) -> int:
    if grid[y][x] != "#":
        return 0
    h, w = len(grid), len(grid[0])

    def s(nx: int, ny: int) -> bool:
        return 0 <= nx < w and 0 <= ny < h and grid[ny][nx] == "#"

    if y == 0:
        return 92 if x == 0 else 93 if x == w - 1 else 46
    if y == h - 1:
        return 114 if x == 0 else 27 if x == w - 1 else 2
    if x == 0:
        return 113
    if x == w - 1:
        return 111
    up, dn, lf, rt = s(x, y - 1), s(x, y + 1), s(x - 1, y), s(x + 1, y)
    if not up and not dn:
        return 7 if not lf and rt else 9 if lf and not rt else 8
    if not up and dn:
        return 7 if not lf and rt else 9 if lf and not rt else 8
    if up and not dn:
        return 30
    if up and dn and not lf and rt:
        return 30
    if up and dn and lf and not rt:
        return 31
    return 30


def build_tmx(grid: list[str]) -> str:
    w, h = len(grid[0]), len(grid)
    gids = [autotile_gid(grid, x, y) for y in range(h) for x in range(w)]
    body = "\n".join(",".join(str(gids[y * w + x]) for x in range(w)) + "," for y in range(h))
    return f"""<?xml version="1.0" encoding="UTF-8"?>
<map version="1.10" tiledversion="1.12.2" orientation="orthogonal" renderorder="right-down" width="{w}" height="{h}" tilewidth="16" tileheight="16" infinite="0" nextlayerid="4" nextobjectid="1">
 <tileset firstgid="1" source="{ASSET_FOLDER}/terrain.tsj"/>
 <imagelayer id="2" name="Background" repeatx="1" repeaty="1">
  <image source="{ASSET_FOLDER}/background.png" width="64" height="64"/>
 </imagelayer>
 <layer id="1" name="碰撞" width="{w}" height="{h}">
  <data encoding="csv">
{body}
</data>
 </layer>
</map>
"""


def _read_png(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("Not PNG")
    pos = 8
    w = h = 0
    raw = b""
    while pos < len(data):
        length = struct.unpack(">I", data[pos : pos + 4])[0]
        kind = data[pos + 4 : pos + 8]
        chunk = data[pos + 8 : pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            w, h = struct.unpack(">II", chunk[:8])
        elif kind == b"IDAT":
            raw += chunk
    rgba = zlib.decompress(raw)
    return w, h, rgba


def _png_pixel(rgba: bytes, w: int, x: int, y: int) -> tuple[int, int, int, int]:
    i = (y * w + x) * 4
    return rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3]


def render_preview(grid: list[str], out_path: Path, scale: int = 2) -> None:
    """Bake terrain tiles to PNG (black bg, hard pixels, 2× for 32px look)."""
    tw, th, terrain = _read_png(TERRAIN_DIR / "terrain.png")
    cols = tw // 16
    mw, mh = len(grid[0]), len(grid)
    pw, ph = mw * 16 * scale, mh * 16 * scale
    pixels = bytearray(pw * ph * 3)

    def set_px(px: int, py: int, r: int, g: int, b: int) -> None:
        if 0 <= px < pw and 0 <= py < ph:
            i = (py * pw + px) * 3
            pixels[i], pixels[i + 1], pixels[i + 2] = r, g, b

    for py in range(ph):
        for px in range(pw):
            set_px(px, py, 0, 0, 0)

    for y in range(mh):
        for x in range(mw):
            gid = autotile_gid(grid, x, y)
            if gid <= 0:
                continue
            tid = gid - 1
            sx = (tid % cols) * 16
            sy = (tid // cols) * 16
            for dy in range(16):
                for dx in range(16):
                    r, g, b, a = _png_pixel(terrain, tw, sx + dx, sy + dy)
                    if a < 16:
                        continue
                    for sy2 in range(scale):
                        for sx2 in range(scale):
                            set_px(
                                x * 16 * scale + dx * scale + sx2,
                                y * 16 * scale + dy * scale + sy2,
                                r,
                                g,
                                b,
                            )

    def chunk(tag: bytes, buf: bytes) -> bytes:
        crc = zlib.crc32(tag + buf) & 0xFFFFFFFF
        return struct.pack(">I", len(buf)) + tag + buf + struct.pack(">I", crc)

    raw_rows = []
    stride = pw * 3
    for y in range(ph):
        row = b"\x00" + bytes(pixels[y * stride : (y + 1) * stride])
        raw_rows.append(row)
    compressed = zlib.compress(b"".join(raw_rows), 9)

    ihdr = struct.pack(">IIBBBBB", pw, ph, 8, 2, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", compressed) + chunk(b"IEND", b"")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_bytes(png)


def write_level(n: int, lines: list[str], poison: bool) -> None:
    validate_layout(lines, poison)
    LEVELS_DIR.mkdir(parents=True, exist_ok=True)
    MAPS_DIR.mkdir(parents=True, exist_ok=True)
    (LEVELS_DIR / f"level{n:02d}_collision.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    (MAPS_DIR / f"level{n:02d}.tmx").write_text(build_tmx(lines), encoding="utf-8")
    render_preview(lines, PREVIEW_DIR / f"level{n:02d}.png")
    solids = sum(r[1:-1].count("#") for r in lines[1:-1])
    print(f"level{n:02d}  solids={solids}  preview=assets/previews/level{n:02d}.png")


def exit_2p(m: MapBuilder, y: int = 15) -> None:
    m.plat(10, y, 10, allow_wide=True)
    m.exits(y, 14, 18)


def exit_3p(m: MapBuilder, y: int = 15) -> None:
    m.plat(9, y, 10, allow_wide=True)
    m.exits(y, 12, 16, 20)


def floor_ripple(m: MapBuilder) -> None:
    """Staggered bottom steps — not wall-to-wall, uneven heights."""
    m.plat(4, 13, 3)
    m.plat(11, 14, 2)
    m.plat(17, 13, 4)
    m.plat(24, 14, 3)
    m.plat(31, 13, 2)


# ── Level 1: intro — near lower-Z vs far upper-loop ─────────────────────────
def build_level1() -> list[str]:
    m = MapBuilder()
    m.spawn_2p()
    m.shell()
    # Near (shortcut)
    m.z_run([(3, 2, 2), (6, 4, 3), (4, 7, 2), (7, 10, 3), (10, 13, 2)])
    m.slot(4, 3)
    # Far (detour)
    m.z_run([(31, 2, 2), (27, 4, 3), (30, 6, 2), (25, 8, 3), (29, 11, 2), (26, 13, 3)])
    m.slot(28, 5)
    m.nest((8, 3, 3), (11, 5, 2))
    m.island(20, 4, 2)
    m.island(15, 11, 2)
    m.cantilever("L", 9, 2)
    m.switch([(13, 5, 2), (18, 5, 2), (11, 12, 2), (17, 13, 2), (23, 11, 2)])
    floor_ripple(m)
    exit_2p(m)
    return m.build()


def build_level2() -> list[str]:
    m = MapBuilder()
    m.spawn_2p()
    m.shell()
    m.z_run([(3, 2, 2), (7, 3, 3), (5, 6, 2), (9, 8, 3), (6, 11, 2), (10, 14, 3)])
    m.z_run([(32, 2, 2), (28, 5, 3), (31, 7, 2), (26, 9, 3), (30, 12, 2)])
    m.slot(5, 7)
    m.slot(27, 6)
    m.nest((7, 4, 3), (10, 6, 2))
    m.nest((24, 8, 3), (27, 10, 2))
    m.island(18, 3, 2)
    m.island(21, 10, 2)
    m.cantilever("R", 12, 2)
    m.switch([(12, 5, 2), (20, 4, 2), (14, 11, 2), (22, 12, 2)])
    floor_ripple(m)
    exit_2p(m)
    return m.build()


def build_level3() -> list[str]:
    m = MapBuilder()
    m.spawn_2p()
    m.shell()
    m.z_run([(4, 2, 2), (8, 4, 3), (5, 7, 2), (9, 9, 3), (7, 12, 2), (11, 14, 3)])
    m.z_run([(30, 3, 2), (26, 5, 3), (29, 7, 2), (24, 10, 3), (28, 13, 2)])
    m.slot(6, 5)
    m.slot(25, 8)
    m.nest((6, 3, 3), (9, 5, 2))
    m.nest((23, 6, 3), (26, 8, 2))
    m.island(16, 4, 2)
    m.island(19, 11, 2)
    m.cantilever("L", 8, 2)
    m.switch([(13, 5, 2), (19, 6, 2), (12, 11, 2), (21, 12, 2)])
    floor_ripple(m)
    exit_2p(m)
    return m.build()


def build_level4() -> list[str]:
    m = MapBuilder()
    m.spawn_2p()
    m.shell()
    m.z_run([(3, 2, 2), (6, 5, 2), (4, 8, 3), (8, 10, 2), (5, 13, 3), (10, 15, 2)])
    m.z_run([(32, 2, 2), (28, 4, 3), (31, 6, 2), (25, 9, 3), (29, 12, 2), (27, 14, 2)])
    m.slot(4, 6)
    m.slot(30, 5)
    m.nest((7, 3, 3), (10, 5, 2))
    m.nest((22, 7, 3), (25, 9, 2))
    m.island(17, 3, 2)
    m.island(14, 12, 2)
    m.island(22, 11, 2)
    m.switch([(12, 4, 2), (18, 5, 2), (11, 12, 2), (20, 13, 2)])
    floor_ripple(m)
    exit_2p(m)
    return m.build()


def build_level5() -> list[str]:
    m = MapBuilder()
    m.spawn_3p()
    m.shell()
    m.z_run([(3, 2, 2), (6, 5, 2), (4, 8, 3), (7, 11, 2), (5, 14, 2)])
    m.z_run([(18, 2, 3), (15, 4, 2), (19, 7, 3), (16, 10, 2), (18, 13, 3)])
    m.z_run([(31, 2, 2), (27, 4, 3), (30, 7, 2), (26, 10, 3), (29, 13, 2)])
    m.slot(4, 4)
    m.slot(16, 5)
    m.slot(28, 5)
    m.nest((6, 3, 3), (8, 5, 2))
    m.nest((17, 6, 3), (19, 8, 2))
    m.island(12, 4, 2)
    m.island(22, 4, 2)
    m.switch([(11, 5, 2), (21, 6, 2), (10, 12, 2), (24, 11, 2)])
    floor_ripple(m)
    exit_3p(m)
    return m.build()


def build_level6() -> list[str]:
    m = MapBuilder()
    m.spawn_3p()
    m.shell()
    m.z_run([(3, 2, 2), (7, 4, 3), (5, 7, 2), (9, 9, 3), (6, 12, 2), (10, 14, 2)])
    m.z_run([(17, 3, 3), (14, 6, 2), (18, 8, 3), (15, 11, 2), (17, 14, 3)])
    m.z_run([(32, 2, 2), (28, 5, 3), (31, 8, 2), (26, 11, 3), (30, 14, 2)])
    m.slot(5, 5)
    m.slot(15, 7)
    m.slot(27, 6)
    m.nest((6, 3, 3), (9, 5, 2))
    m.nest((16, 5, 3), (19, 7, 2))
    m.nest((25, 4, 3), (28, 6, 2))
    m.island(12, 5, 2)
    m.island(22, 10, 2)
    m.switch([(11, 4, 2), (20, 5, 2), (13, 12, 2), (24, 13, 2)])
    floor_ripple(m)
    exit_3p(m)
    return m.build()


def build_level7() -> list[str]:
    m = MapBuilder()
    m.spawn_3p()
    m.shell()
    m.z_run([(3, 2, 2), (6, 4, 2), (4, 6, 3), (8, 8, 2), (5, 11, 3), (9, 14, 2)])
    m.z_run([(16, 2, 3), (13, 5, 2), (18, 7, 3), (14, 10, 2), (17, 13, 3)])
    m.z_run([(31, 2, 2), (27, 4, 3), (30, 6, 2), (25, 9, 3), (29, 12, 2), (27, 14, 2)])
    m.slot(4, 5)
    m.slot(14, 6)
    m.slot(26, 5)
    m.nest((7, 3, 3), (10, 5, 2))
    m.nest((15, 4, 3), (18, 6, 2))
    m.nest((24, 3, 3), (27, 5, 2))
    m.island(11, 5, 2)
    m.island(21, 8, 2)
    m.island(33, 10, 2)
    m.switch([(12, 5, 2), (21, 4, 2), (11, 12, 2), (23, 11, 2)])
    floor_ripple(m)
    exit_3p(m)
    return m.build()


def build_level8() -> list[str]:
    m = MapBuilder()
    m.spawn_3p()
    m.shell()
    m.z_run([(3, 2, 2), (7, 3, 3), (5, 5, 2), (9, 7, 3), (6, 10, 2), (10, 12, 3), (7, 14, 2)])
    m.z_run([(15, 2, 3), (12, 4, 2), (17, 6, 3), (13, 9, 2), (16, 11, 3), (14, 14, 2)])
    m.z_run([(32, 2, 2), (28, 4, 3), (31, 6, 2), (25, 8, 3), (29, 11, 2), (26, 13, 3)])
    m.slot(4, 4)
    m.slot(13, 5)
    m.slot(27, 4)
    m.nest((6, 3, 3), (9, 5, 2))
    m.nest((16, 4, 3), (19, 6, 2))
    m.nest((24, 3, 3), (27, 5, 2))
    m.island(11, 4, 2)
    m.island(20, 7, 2)
    m.island(33, 9, 2)
    m.cantilever("L", 11, 2)
    m.switch([(12, 5, 2), (22, 5, 2), (10, 11, 2), (21, 12, 2), (18, 13, 2)])
    floor_ripple(m)
    exit_3p(m)
    return m.build()


LEVELS = [
    (1, False, build_level1),
    (2, False, build_level2),
    (3, False, build_level3),
    (4, False, build_level4),
    (5, True, build_level5),
    (6, True, build_level6),
    (7, True, build_level7),
    (8, True, build_level8),
]


def main() -> None:
    for n, poison, fn in LEVELS:
        write_level(n, fn(), poison)
    print("Done.")


if __name__ == "__main__":
    main()
