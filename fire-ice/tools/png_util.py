"""Minimal PNG writer (stdlib only) for placeholder tilesets."""

from __future__ import annotations

import struct
import zlib
from typing import Iterable, Sequence


def write_rgba_png(path: str, width: int, height: int, rgba_rows: Sequence[bytes]) -> None:
    if len(rgba_rows) != height:
        raise ValueError("row count mismatch")
    raw = b""
    for row in rgba_rows:
        if len(row) != width * 4:
            raise ValueError("row width mismatch")
        raw += b"\x00" + row

    def chunk(tag: bytes, data: bytes) -> bytes:
        return (
            struct.pack(">I", len(data))
            + tag
            + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
        )

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", ihdr)
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")

    with open(path, "wb") as f:
        f.write(png)


def solid_tile(r: int, g: int, b: int, a: int = 255) -> bytes:
    return bytes([r, g, b, a] * 32 * 32)


def checker_tile(c1: tuple[int, int, int], c2: tuple[int, int, int]) -> bytes:
    px = []
    for y in range(32):
        for x in range(32):
            c = c1 if ((x // 4) + (y // 4)) % 2 == 0 else c2
            px.extend([c[0], c[1], c[2], 255])
    return bytes(px)


def strip_tiles(tiles: Iterable[bytes]) -> bytes:
    rows: list[bytes] = []
    for tile in tiles:
        for y in range(32):
            rows.append(tile[y * 32 * 4 : (y + 1) * 32 * 4])
    return b"".join(rows)


def write_tile_strip(path: str, tiles: Sequence[bytes]) -> None:
    width = 32 * len(tiles)
    height = 32
    rgba_rows = []
    offset = 0
    for y in range(height):
        row = b""
        for tile in tiles:
            row += tile[y * 32 * 4 : (y + 1) * 32 * 4]
        rgba_rows.append(row)
    write_rgba_png(path, width, height, rgba_rows)
