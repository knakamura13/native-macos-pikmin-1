#!/usr/bin/env python3
"""Self-test for verify_scenario_png.py (run from CTest)."""
from __future__ import annotations

import struct
import sys
import tempfile
import zlib
from pathlib import Path

from verify_scenario_png import (
    compare_golden,
    distinct_color_count,
    read_png_rgb,
)

ROOT = Path(__file__).resolve().parent
GOLDEN = ROOT / "golden" / "boot_smoke_tick100_640x480.png"


def _crc32(chunk_type: bytes, data: bytes) -> int:
    import binascii

    return binascii.crc32(chunk_type + data) & 0xFFFFFFFF


def write_rgba_png(path: Path, width: int, height: int, rgba_rows: list[bytes], filter_type: int = 0) -> None:
    """Write an 8-bit RGBA PNG; each row is width*4 bytes, filter_type per scanline."""
    assert len(rgba_rows) == height
    assert all(len(r) == width * 4 for r in rgba_rows)
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    raw = bytearray()
    for row in rgba_rows:
        raw.append(filter_type)
        raw.extend(row)
    idat = zlib.compress(bytes(raw), 9)

    def chunk(ctype: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + ctype + data + struct.pack(">I", _crc32(ctype, data))

    with path.open("wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", idat))
        f.write(chunk(b"IEND", b""))


def rgba_rows_from_rgb_pixels(
    width: int, height: int, pixels: list[tuple[int, int, int]]
) -> list[bytes]:
    rows: list[bytes] = []
    for y in range(height):
        row = bytearray()
        for x in range(width):
            r, g, b = pixels[y * width + x]
            row.extend((r, g, b, 255))
        rows.append(bytes(row))
    return rows


def run_self_test() -> None:
    if not GOLDEN.is_file():
        raise AssertionError(f"missing golden {GOLDEN}")

    w, h, pixels = read_png_rgb(str(GOLDEN))
    assert w == 640 and h == 480
    n = distinct_color_count(pixels)
    # Stand-in art (#115) uses more distinct flat-shaded colours than grey cubes (~126).
    if not (150 <= n <= 290):
        raise AssertionError(f"golden distinct colours {n}, expected about 204 (150..290)")

    with tempfile.TemporaryDirectory() as tmp:
        tmp_path = Path(tmp)
        filter0 = tmp_path / "golden_filter0.png"
        rows = rgba_rows_from_rgb_pixels(w, h, pixels)
        write_rgba_png(filter0, w, h, rows, filter_type=0)
        _, _, decoded0 = read_png_rgb(str(filter0))
        ok, _ = compare_golden(pixels, decoded0, tolerance=0, max_diff_ratio=0.0)
        if not ok:
            raise AssertionError("filter-0 re-encoded golden must match itself exactly")

        ok_g, _ = compare_golden(pixels, decoded0, tolerance=10, max_diff_ratio=0.03)
        if not ok_g:
            raise AssertionError("filter-0 golden must pass production tolerances")

        tampered = list(pixels)
        y0 = h * 3 // 4
        for i in range(y0 * w, w * h):
            tampered[i] = (255, 0, 255)
        bad_png = tmp_path / "tampered_bottom.png"
        write_rgba_png(bad_png, w, h, rgba_rows_from_rgb_pixels(w, h, tampered), filter_type=1)
        _, _, bad_px = read_png_rgb(str(bad_png))
        bad_ok, _ = compare_golden(pixels, bad_px, tolerance=10, max_diff_ratio=0.03)
        if bad_ok:
            raise AssertionError("magenta bottom quarter must fail golden compare")

    print(f"verify_scenario_png self-test ok (golden {n} distinct colours)")


def main() -> int:
    try:
        run_self_test()
        return 0
    except AssertionError as e:
        print(f"self-test failed: {e}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
