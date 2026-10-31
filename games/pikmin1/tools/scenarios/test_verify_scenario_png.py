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
    count_in_region,
    distinct_color_count,
    edge_object_pixels,
    is_saturated_red,
    is_silver,
    read_png_rgb,
    verify_showcase_composition,
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
    if not (100 <= n <= 400):
        raise AssertionError(f"golden distinct colours {n}, expected flat-shaded stand-in range (100..400)")

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

        ok_show, _ = verify_showcase_composition(
            w, h, pixels, max_near_black_ratio=0.95, min_center_non_grass=0, min_center_distinct=0
        )
        if not ok_show:
            raise AssertionError("boot golden must pass loose showcase composition helper")

    # Showcase guards on synthetic frames: grass with a red block, a silver block,
    # and a model running off the left border.
    sw, sh = 64, 48
    grass = (85, 150, 70)
    frame = [grass] * (sw * sh)
    for y in range(5, 15):
        for x in range(5, 15):
            frame[y * sw + x] = (230, 50, 55)
        for x in range(40, 50):
            frame[y * sw + x] = (215, 220, 235)
    if count_in_region(sw, sh, frame, (0, 0, 20, 20), is_saturated_red) != 100:
        raise AssertionError("red block must count 100 saturated red pixels")
    if count_in_region(sw, sh, frame, (0, 0, 20, 20), is_silver) != 0:
        raise AssertionError("red block must not count as silver")
    if count_in_region(sw, sh, frame, (35, 0, 60, 20), is_silver) != 100:
        raise AssertionError("silver block must count 100 silver pixels")
    if edge_object_pixels(sw, sh, frame, y_max=40) != 0:
        raise AssertionError("frame with models inside must have a clear border")
    for x in range(10, 20):
        frame[x] = (0, 0, 0)
    if edge_object_pixels(sw, sh, frame, y_max=40) != 0:
        raise AssertionError("black clear colour through the horizon is not a clipped model")
    for y in range(20, 30):
        for x in range(0, 6):
            frame[y * sw + x] = (230, 50, 55)
    if edge_object_pixels(sw, sh, frame, y_max=40) != 20:
        raise AssertionError("model on the left border must leave 20 border pixels (2 columns x 10 rows)")

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
