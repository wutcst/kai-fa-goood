#!/usr/bin/env python3
"""
Dual-route level generator — Z-weave paths, no parallel tiers.

Each level: shortcut weaves diagonally; detour takes longer Z with more slots.
Platforms are 2–4 tiles, x shifts ≥2 per step, rows rarely align.
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
MAX_PLAT_W = 5
MAX_EXIT_W = 11

# (dx, dy, width) — cursor moves, then places platform
Step = tuple[int, int, int]


class MapBuilder:
    def __init__(self) -> None:
        self.grid = [["." for _ in range(INNER_W)] for _ in range(INNER_H)]

    def _popup(self, x: int, y: int, w: int, h: int = 1) -> bool:
        for dy in range(h):
            for dx in range(w):
                px, py = x + dx, y + dy
                if CENTER_X0 <= px <= CENTER_X1 and CENTER_Y0 <= py <= CENTER_Y1:
                    return True
        return False

    def plat(self, x: int, y: int, w: int, *, wide: bool = False) -> None:
        if w > (MAX_EXIT_W if wide else MAX_PLAT_W):
            raise ValueError(f"too wide {w} at {x},{y}")
        if x <= 2 and x + w >= INNER_W - 3:
            raise ValueError(f"wall-span at {y}")
        if self._popup(x, y, w):
            return
        for dx in range(w):
            if 0 <= x + dx < INNER_W and 0 <= y < INNER_H:
                self.grid[y][x + dx] = "#"

    def weave(self, sx: int, sy: int, steps: list[Step]) -> tuple[int, int]:
        x, y = sx, sy
        for dx, dy, w in steps:
            x = max(3, min(INNER_W - 3 - w, x + dx))
            y += dy
            if 0 <= y < INNER_H:
                self.plat(x, y, w)
        return x, y

    def overlap(self, upper: tuple[int, int, int], lower: tuple[int, int, int]) -> None:
        self.plat(*upper)
        ux, uy, uw = upper
        lx, ly, lw = lower
        if ly <= uy:
            ly = uy + 1
        self.plat(lx, ly, lw)

    def slot(self, x: int, y: int, w: int = 3, h: int = 2) -> None:
        if self._popup(x, y, w, h):
            return
        for dx in range(w):
            self.grid[y][x + dx] = "#"
        for dy in range(1, h):
            self.grid[y + dy][x] = "#"
            self.grid[y + dy][x + w - 1] = "#"
        g = max(2, w - 1)
        gx = x + (w - g) // 2
        for dx in range(g):
            self.grid[y + h - 1][gx + dx] = "."

    def pillar(self, x: int, y: int, h: int = 2) -> None:
        if self._popup(x, y, 1, h):
            return
        for dy in range(h):
            if 0 <= y + dy < INNER_H:
                self.grid[y + dy][x] = "#"

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
    assert len(lines) == MAP_H and all(len(r) == MAP_W for r in lines)
    text = "\n".join(lines)
    for c in "fwEX":
        if c not in text:
            raise ValueError(f"missing {c}")
    if poison and ("p" not in text or "P" not in text):
        raise ValueError("missing poison")

    for y in range(CENTER_Y0, CENTER_Y1 + 1):
        inner = lines[y + 1][1:-1]
        for x in range(CENTER_X0, CENTER_X1 + 1):
            if inner[x] == "#":
                raise ValueError(f"popup blocked ({x},{y})")

    for y in range(INNER_H):
        if y >= 14:
            continue
        inner = lines[y + 1][1:-1]
        for x, w in _segments(inner):
            if x >= 3 and x + w <= INNER_W - 3 and w >= 8:
                raise ValueError(f"long bar w={w} y={y}")


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
<map version="1.10" tiledversion="1.12.2" orientation="orthogonal" renderorder="right-down" width="{w}" height="{h}" tilewidth="16" tileheight="16" infinite="0" nextlayerid="3" nextobjectid="1">
 <tileset firstgid="1" source="{ASSET_FOLDER}/terrain.tsj"/>
 <layer id="1" name="碰撞" width="{w}" height="{h}">
  <data encoding="csv">
{body}
</data>
 </layer>
</map>
"""


def _read_png(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    pos = 8
    w = h = 0
    raw = b""
    while pos < len(data):
        ln = struct.unpack(">I", data[pos : pos + 4])[0]
        kind = data[pos + 4 : pos + 8]
        chunk = data[pos + 8 : pos + 8 + ln]
        pos += 12 + ln
        if kind == b"IHDR":
            w, h = struct.unpack(">II", chunk[:8])
        elif kind == b"IDAT":
            raw += chunk
    return w, h, zlib.decompress(raw)


def _px(rgba: bytes, w: int, x: int, y: int) -> tuple[int, int, int, int]:
    i = (y * w + x) * 4
    return rgba[i], rgba[i + 1], rgba[i + 2], rgba[i + 3]


def render_preview(grid: list[str], path: Path, scale: int = 2) -> None:
    tw, th, terrain = _read_png(TERRAIN_DIR / "terrain.png")
    cols = tw // 16
    mw, mh = len(grid[0]), len(grid)
    pw, ph = mw * 16 * scale, mh * 16 * scale
    px = bytearray(pw * ph * 3)

    def setp(x: int, y: int, r: int, g: int, b: int) -> None:
        if 0 <= x < pw and 0 <= y < ph:
            i = (y * pw + x) * 3
            px[i], px[i + 1], px[i + 2] = r, g, b

    for y in range(ph):
        for x in range(pw):
            setp(x, y, 0, 0, 0)

    for y in range(mh):
        for x in range(mw):
            gid = autotile_gid(grid, x, y)
            if gid <= 0:
                continue
            tid = gid - 1
            sx, sy = (tid % cols) * 16, (tid // cols) * 16
            for dy in range(16):
                for dx in range(16):
                    r, g, b, a = _px(terrain, tw, sx + dx, sy + dy)
                    if a < 16:
                        continue
                    for sy2 in range(scale):
                        for sx2 in range(scale):
                            setp(x * 16 * scale + dx * scale + sx2, y * 16 * scale + dy * scale + sy2, r, g, b)

    def chunk(tag: bytes, buf: bytes) -> bytes:
        return struct.pack(">I", len(buf)) + tag + buf + struct.pack(">I", zlib.crc32(tag + buf) & 0xFFFFFFFF)

    rows = [b"\x00" + bytes(px[y * pw * 3 : (y + 1) * pw * 3]) for y in range(ph)]
    png = b"\x89PNG\r\n\x1a\n"
    ihdr = struct.pack(">IIBBBBB", pw, ph, 8, 2, 0, 0, 0)
    png += chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(b"".join(rows), 9)) + chunk(b"IEND", b"")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png)


def write_level(n: int, lines: list[str], poison: bool) -> None:
    validate_layout(lines, poison)
    LEVELS_DIR.mkdir(parents=True, exist_ok=True)
    MAPS_DIR.mkdir(parents=True, exist_ok=True)
    (LEVELS_DIR / f"level{n:02d}_collision.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
    (MAPS_DIR / f"level{n:02d}.tmx").write_text(build_tmx(lines), encoding="utf-8")
    render_preview(lines, PREVIEW_DIR / f"level{n:02d}.png")
    print(f"level{n:02d} ok")


def finish_2p(m: MapBuilder) -> None:
    m.plat(10, 15, 9, wide=True)
    m.put(14, 15, "E")
    m.put(18, 15, "X")


def finish_3p(m: MapBuilder) -> None:
    m.plat(9, 15, 10, wide=True)
    m.put(12, 15, "E")
    m.put(16, 15, "X")
    m.put(20, 15, "P")


# Shortcut: lower-left Z climb. Detour: upper-right longer Z with slots.
SHORTCUT = [
    (0, 0, 2), (3, 2, 2), (-2, 2, 3), (4, 2, 2), (-1, 3, 2), (2, 2, 3), (-3, 2, 2), (3, 2, 2),
]
DETOUR = [
    (0, 0, 2), (-4, 2, 2), (2, 2, 3), (-3, 3, 2), (1, 2, 3), (-2, 2, 2), (3, 3, 2), (-4, 2, 3), (2, 2, 2),
]
LINKS = [(0, 0, 2), (5, 2, 2), (-3, 3, 2), (4, 2, 2)]


def build_level1() -> list[str]:
    m = MapBuilder()
    m.spawn_2p()
    m.shell()
    m.weave(4, 2, SHORTCUT)
    m.weave(30, 2, DETOUR)
    m.slot(6, 4)
    m.slot(25, 6)
    m.overlap((8, 4, 3), (11, 6, 2))
    m.weave(12, 5, LINKS)
    m.weave(20, 11, [(0, 0, 2), (4, 2, 2), (-2, 2, 2)])
    m.pillar(9, 9, 2)
    m.plat(28, 12, 2)
    m.plat(5, 13, 2)
    m.plat(18, 14, 3)
    m.plat(32, 13, 2)
    finish_2p(m)
    return m.build()


def build_level2() -> list[str]:
    m = MapBuilder()
    m.spawn_2p()
    m.shell()
    m.weave(3, 2, SHORTCUT + [(2, 2, 2)])
    m.weave(31, 1, DETOUR)
    m.slot(5, 5)
    m.slot(27, 7)
    m.overlap((7, 5, 3), (10, 7, 2))
    m.overlap((24, 6, 3), (27, 8, 2))
    m.weave(11, 4, LINKS)
    m.weave(22, 10, [(-2, 2, 2), (3, 3, 2), (2, 2, 2)])
    m.pillar(8, 10, 2)
    m.plat(6, 13, 3)
    m.plat(22, 14, 2)
    m.plat(30, 12, 2)
    finish_2p(m)
    return m.build()


def build_level3() -> list[str]:
    m = MapBuilder()
    m.spawn_2p()
    m.shell()
    m.weave(4, 2, [
        (0, 0, 2), (2, 2, 2), (-3, 2, 3), (5, 2, 2), (-2, 3, 2), (3, 2, 2), (-4, 2, 3), (2, 3, 2),
    ])
    m.weave(32, 2, [
        (0, 0, 2), (-3, 2, 2), (2, 3, 3), (-4, 2, 2), (3, 2, 2), (-2, 3, 2), (4, 2, 2), (-3, 3, 2),
    ])
    m.slot(7, 5)
    m.slot(26, 6)
    m.overlap((9, 4, 3), (12, 6, 2))
    m.overlap((23, 5, 3), (26, 7, 2))
    m.weave(13, 5, [(0, 0, 2), (5, 2, 2), (-3, 3, 2)])
    m.weave(18, 11, [(0, 0, 2), (-3, 2, 2), (4, 2, 2)])
    m.pillar(10, 8, 2)
    m.plat(4, 13, 2)
    m.plat(15, 14, 3)
    m.plat(27, 13, 2)
    finish_2p(m)
    return m.build()


def build_level4() -> list[str]:
    m = MapBuilder()
    m.spawn_2p()
    m.shell()
    m.weave(3, 2, SHORTCUT + [(-2, 3, 2), (4, 2, 2)])
    m.weave(30, 2, DETOUR + [(-3, 2, 2)])
    m.slot(6, 6)
    m.slot(28, 5)
    m.overlap((8, 3, 3), (11, 5, 2))
    m.weave(14, 4, LINKS + [(3, 2, 2)])
    m.weave(8, 11, [(2, 2, 2), (-1, 3, 2), (4, 2, 2)])
    m.pillar(20, 9, 2)
    m.plat(32, 11, 2)
    m.plat(6, 14, 2)
    m.plat(24, 13, 3)
    finish_2p(m)
    return m.build()


def build_level5() -> list[str]:
    m = MapBuilder()
    m.spawn_3p()
    m.shell()
    m.weave(3, 2, SHORTCUT)
    m.weave(18, 2, [(0, 0, 2), (-2, 2, 3), (3, 2, 2), (-3, 3, 2), (2, 2, 2), (-2, 2, 3), (3, 3, 2)])
    m.weave(32, 2, DETOUR)
    m.slot(5, 4)
    m.slot(17, 5)
    m.slot(27, 6)
    m.overlap((7, 4, 3), (9, 6, 2))
    m.weave(12, 5, LINKS)
    m.plat(7, 13, 2)
    m.plat(20, 14, 3)
    m.plat(31, 12, 2)
    finish_3p(m)
    return m.build()


def build_level6() -> list[str]:
    m = MapBuilder()
    m.spawn_3p()
    m.shell()
    m.weave(4, 2, SHORTCUT + [(2, 2, 2)])
    m.weave(17, 1, [(0, 0, 3), (-3, 2, 2), (4, 3, 2), (-2, 2, 3), (3, 2, 2)])
    m.weave(31, 2, DETOUR)
    m.slot(6, 5)
    m.slot(16, 6)
    m.slot(26, 5)
    m.overlap((8, 5, 3), (11, 7, 2))
    m.overlap((22, 4, 3), (25, 6, 2))
    m.weave(13, 4, LINKS)
    m.pillar(9, 10, 2)
    m.plat(5, 13, 2)
    m.plat(22, 14, 3)
    finish_3p(m)
    return m.build()


def build_level7() -> list[str]:
    m = MapBuilder()
    m.spawn_3p()
    m.shell()
    m.weave(3, 2, SHORTCUT + [(-3, 3, 2), (4, 2, 2)])
    m.weave(16, 2, DETOUR[:6])
    m.weave(30, 1, DETOUR)
    m.slot(5, 4)
    m.slot(15, 5)
    m.slot(28, 4)
    m.overlap((7, 3, 3), (10, 5, 2))
    m.overlap((20, 6, 3), (23, 8, 2))
    m.weave(12, 5, LINKS + [(-2, 3, 2)])
    m.pillar(8, 9, 2)
    m.plat(33, 10, 2)
    m.plat(6, 14, 2)
    finish_3p(m)
    return m.build()


def build_level8() -> list[str]:
    m = MapBuilder()
    m.spawn_3p()
    m.shell()
    m.weave(3, 2, SHORTCUT + [(-2, 2, 3), (3, 3, 2), (2, 2, 2)])
    m.weave(15, 2, DETOUR)
    m.weave(31, 1, DETOUR + [(3, 2, 2)])
    m.slot(4, 4)
    m.slot(14, 5)
    m.slot(26, 4)
    m.overlap((6, 3, 3), (9, 5, 2))
    m.overlap((18, 4, 3), (21, 6, 2))
    m.overlap((25, 3, 3), (28, 5, 2))
    m.weave(11, 4, LINKS)
    m.weave(19, 11, [(-3, 2, 2), (4, 3, 2), (2, 2, 2)])
    m.pillar(7, 8, 2)
    m.plat(32, 9, 2)
    m.plat(5, 13, 2)
    m.plat(17, 14, 3)
    finish_3p(m)
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
    print("Done — run build.bat to sync into build/Release")


if __name__ == "__main__":
    main()
