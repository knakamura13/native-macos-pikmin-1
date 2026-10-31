#!/usr/bin/env python3
"""Verify a PNG written by --scenario-screenshot (CTest helper)."""
from __future__ import annotations

import argparse
import struct
import sys
import zlib


def read_png_rgb(path: str) -> tuple[int, int, list[tuple[int, int, int]]]:
    with open(path, "rb") as f:
        sig = f.read(8)
        if sig != b"\x89PNG\r\n\x1a\n":
            raise ValueError("not a PNG (bad signature)")
        width = height = 0
        idat = bytearray()
        while True:
            chunk = f.read(8)
            if len(chunk) < 8:
                break
            length, ctype = struct.unpack(">I4s", chunk)
            data = f.read(length)
            f.read(4)  # crc
            if ctype == b"IHDR":
                width, height = struct.unpack(">II", data[:8])
            elif ctype == b"IDAT":
                idat.extend(data)
            elif ctype == b"IEND":
                break
        if width <= 0 or height <= 0:
            raise ValueError("missing IHDR")
        raw = zlib.decompress(bytes(idat))
        stride = 1 + width * 3
        if len(raw) < height * stride:
            raise ValueError("truncated image data")
        pixels: list[tuple[int, int, int]] = []
        for y in range(height):
            row = raw[y * stride + 1 : y * stride + 1 + width * 3]
            for i in range(0, len(row), 3):
                pixels.append((row[i], row[i + 1], row[i + 2]))
        return width, height, pixels


def is_non_blank(pixels: list[tuple[int, int, int]]) -> bool:
    """Reject empty GL readbacks (all zero). Stand-in flat fills are allowed."""
    for r, g, b in pixels:
        if r or g or b:
            return True
    return False


def distinct_color_count(pixels: list[tuple[int, int, int]]) -> int:
    return len(set(pixels))


def rgb_variance(pixels: list[tuple[int, int, int]]) -> float:
    if not pixels:
        return 0.0
    vals = [float(c) for px in pixels for c in px]
    mean = sum(vals) / len(vals)
    return sum((v - mean) ** 2 for v in vals) / len(vals)


def compare_golden(
    golden: list[tuple[int, int, int]],
    actual: list[tuple[int, int, int]],
    *,
    tolerance: int,
    max_diff_ratio: float,
) -> tuple[bool, str]:
    if len(golden) != len(actual):
        return False, f"pixel count {len(actual)} != golden {len(golden)}"
    diff = 0
    for (gr, gg, gb), (ar, ag, ab) in zip(golden, actual):
        if (
            abs(gr - ar) > tolerance
            or abs(gg - ag) > tolerance
            or abs(gb - ab) > tolerance
        ):
            diff += 1
    ratio = diff / len(golden)
    if ratio > max_diff_ratio:
        return False, f"{diff}/{len(golden)} pixels differ ({ratio * 100:.2f}%), max {max_diff_ratio * 100:.2f}%"
    return True, f"within tolerance ({diff} differing pixels, {ratio * 100:.2f}%)"


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("png")
    p.add_argument("--expect-width", type=int, required=True)
    p.add_argument("--expect-height", type=int, required=True)
    p.add_argument("--require-non-blank", action="store_true")
    p.add_argument(
        "--require-missing",
        action="store_true",
        help="pass only if the file does not exist (negative control)",
    )
    p.add_argument(
        "--min-distinct-colors",
        type=int,
        default=0,
        help="fail if the image has fewer distinct RGB values (near-uniform guard)",
    )
    p.add_argument(
        "--min-variance",
        type=float,
        default=0.0,
        help="fail if per-channel RGB variance is below this (blank-frame guard)",
    )
    p.add_argument(
        "--golden",
        help="reference PNG for fuzzy compare (same dimensions)",
    )
    p.add_argument(
        "--golden-tolerance",
        type=int,
        default=8,
        help="per-channel absolute delta allowed vs --golden",
    )
    p.add_argument(
        "--golden-max-diff-ratio",
        type=float,
        default=0.02,
        help="max fraction of pixels that may exceed --golden-tolerance",
    )
    args = p.parse_args()
    import os

    exists = os.path.isfile(args.png)
    if args.require_missing:
        if exists:
            print(f"expected no file, found {args.png}", file=sys.stderr)
            return 1
        return 0
    if not exists:
        print(f"missing {args.png}", file=sys.stderr)
        return 1
    try:
        w, h, pixels = read_png_rgb(args.png)
    except ValueError as e:
        print(f"{args.png}: {e}", file=sys.stderr)
        return 1
    if w != args.expect_width or h != args.expect_height:
        print(f"size {w}x{h}, expected {args.expect_width}x{args.expect_height}", file=sys.stderr)
        return 1
    if args.require_non_blank and not is_non_blank(pixels):
        print("image is all black (likely failed GL readback)", file=sys.stderr)
        return 1
    if args.min_distinct_colors > 0:
        n = distinct_color_count(pixels)
        if n < args.min_distinct_colors:
            print(
                f"only {n} distinct colours (need >= {args.min_distinct_colors}); likely blank or GX-size mismatch",
                file=sys.stderr,
            )
            return 1
    if args.min_variance > 0:
        var = rgb_variance(pixels)
        if var < args.min_variance:
            print(
                f"RGB variance {var:.2f} below {args.min_variance}; likely blank or uniform frame",
                file=sys.stderr,
            )
            return 1
    if args.golden:
        try:
            gw, gh, gpixels = read_png_rgb(args.golden)
        except ValueError as e:
            print(f"golden {args.golden}: {e}", file=sys.stderr)
            return 1
        if gw != w or gh != h:
            print(f"golden size {gw}x{gh} != capture {w}x{h}", file=sys.stderr)
            return 1
        ok, msg = compare_golden(
            gpixels,
            pixels,
            tolerance=args.golden_tolerance,
            max_diff_ratio=args.golden_max_diff_ratio,
        )
        if not ok:
            print(f"golden compare failed: {msg}", file=sys.stderr)
            return 1
        print(f"golden ok: {msg}")
    print(f"ok {args.png} {w}x{h}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
