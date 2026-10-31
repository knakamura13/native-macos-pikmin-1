#!/usr/bin/env python3
"""Verify a PNG written by --scenario-screenshot (CTest helper)."""
from __future__ import annotations

import argparse
import struct
import sys
import zlib

PNG_SIG = b"\x89PNG\r\n\x1a\n"


def _paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def _unfilter_scanline(filter_type: int, row: bytes, prev: bytes | None, bpp: int) -> bytes:
    out = bytearray(len(row))
    for i, x in enumerate(row):
        if filter_type == 0:
            raw = x
        elif filter_type == 1:
            left = out[i - bpp] if i >= bpp else 0
            raw = (x + left) & 0xFF
        elif filter_type == 2:
            up = prev[i] if prev is not None else 0
            raw = (x + up) & 0xFF
        elif filter_type == 3:
            left = out[i - bpp] if i >= bpp else 0
            up = prev[i] if prev is not None else 0
            raw = (x + ((left + up) // 2)) & 0xFF
        elif filter_type == 4:
            left = out[i - bpp] if i >= bpp else 0
            up = prev[i] if prev is not None else 0
            upper_left = prev[i - bpp] if prev is not None and i >= bpp else 0
            raw = (x + _paeth(left, up, upper_left)) & 0xFF
        else:
            raise ValueError(f"unknown PNG filter type {filter_type}")
        out[i] = raw
    return bytes(out)


def read_png_rgb(path: str) -> tuple[int, int, list[tuple[int, int, int]]]:
    """Decode an 8-bit RGB or RGBA PNG into RGB pixels (alpha dropped)."""
    with open(path, "rb") as f:
        if f.read(8) != PNG_SIG:
            raise ValueError("not a PNG (bad signature)")
        width = height = 0
        bit_depth = 0
        color_type = 0
        interlace = 0
        idat = bytearray()
        while True:
            chunk = f.read(8)
            if len(chunk) < 8:
                break
            length, ctype = struct.unpack(">I4s", chunk)
            data = f.read(length)
            f.read(4)  # crc
            if ctype == b"IHDR":
                if length < 13:
                    raise ValueError("truncated IHDR")
                width, height, bit_depth, color_type, _, _, interlace = struct.unpack(
                    ">IIBBBBB", data[:13]
                )
            elif ctype == b"IDAT":
                idat.extend(data)
            elif ctype == b"IEND":
                break
        if width <= 0 or height <= 0:
            raise ValueError("missing IHDR")
        if bit_depth != 8:
            raise ValueError(f"unsupported bit depth {bit_depth} (need 8)")
        if color_type not in (2, 6):
            raise ValueError(f"unsupported colour type {color_type} (need RGB or RGBA)")
        if interlace != 0:
            raise ValueError("interlaced PNG not supported")
        bpp = 3 if color_type == 2 else 4
        stride = 1 + width * bpp
        raw = zlib.decompress(bytes(idat))
        expected = height * stride
        if len(raw) != expected:
            raise ValueError(f"decompressed length {len(raw)} != {expected}")

        pixels: list[tuple[int, int, int]] = []
        prev: bytes | None = None
        for y in range(height):
            row_off = y * stride
            filter_type = raw[row_off]
            if filter_type > 4:
                raise ValueError(f"invalid filter type {filter_type} on row {y}")
            filtered = raw[row_off + 1 : row_off + stride]
            recon = _unfilter_scanline(filter_type, filtered, prev, bpp)
            prev = recon
            for x in range(width):
                i = x * bpp
                pixels.append((recon[i], recon[i + 1], recon[i + 2]))
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


def near_black_ratio(pixels: list[tuple[int, int, int]], *, threshold: int = 15) -> float:
    if not pixels:
        return 1.0
    dark = sum(1 for r, g, b in pixels if r < threshold and g < threshold and b < threshold)
    return dark / len(pixels)


def is_grass_pixel(r: int, g: int, b: int) -> bool:
    return g > 70 and g > r + 8 and g > b + 8 and r < 130


def showcase_composition_stats(
    width: int,
    height: int,
    pixels: list[tuple[int, int, int]],
    *,
    world_y_max: int = 400,
) -> tuple[float, int, int]:
    """Return (world near-black ratio, center non-grass count, center distinct colours)."""
    world: list[tuple[int, int, int]] = []
    for y in range(min(world_y_max, height)):
        for x in range(width):
            world.append(pixels[y * width + x])
    world_dark = near_black_ratio(world)

    cx0, cx1 = width // 4, (3 * width) // 4
    cy0, cy1 = height // 5, (2 * height) // 3
    center: list[tuple[int, int, int]] = []
    for y in range(cy0, cy1):
        for x in range(cx0, cx1):
            center.append(pixels[y * width + x])
    non_grass = 0
    for r, g, b in center:
        if r < 15 and g < 15 and b < 15:
            continue
        if is_grass_pixel(r, g, b):
            continue
        non_grass += 1
    return world_dark, non_grass, len(set(center))


def verify_showcase_composition(
    width: int,
    height: int,
    pixels: list[tuple[int, int, int]],
    *,
    max_near_black_ratio: float,
    min_center_non_grass: int,
    min_center_distinct: int,
) -> tuple[bool, str]:
    world_dark, non_grass, distinct_c = showcase_composition_stats(width, height, pixels)
    if world_dark > max_near_black_ratio:
        return (
            False,
            f"world band (y<{400}) is {world_dark * 100:.1f}% near-black (max {max_near_black_ratio * 100:.1f}%)",
        )
    if non_grass < min_center_non_grass:
        return (
            False,
            f"center has {non_grass} non-grass pixels (need >= {min_center_non_grass})",
        )
    if distinct_c < min_center_distinct:
        return (
            False,
            f"center has {distinct_c} distinct colours (need >= {min_center_distinct})",
        )
    return True, f"showcase composition ok (world dark {world_dark * 100:.1f}%, center non-grass {non_grass})"

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
    p.add_argument(
        "--max-near-black-ratio",
        type=float,
        default=-1.0,
        help="standin_showcase guard: fail if more than this fraction of world-band pixels are near-black",
    )
    p.add_argument(
        "--min-center-non-grass",
        type=int,
        default=0,
        help="standin_showcase guard: min non-grass pixels in the central region",
    )
    p.add_argument(
        "--min-center-distinct",
        type=int,
        default=0,
        help="standin_showcase guard: min distinct colours in the central region",
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
    if args.max_near_black_ratio >= 0.0 or args.min_center_non_grass > 0 or args.min_center_distinct > 0:
        ok, msg = verify_showcase_composition(
            w,
            h,
            pixels,
            max_near_black_ratio=args.max_near_black_ratio if args.max_near_black_ratio >= 0.0 else 1.0,
            min_center_non_grass=args.min_center_non_grass,
            min_center_distinct=args.min_center_distinct,
        )
        if not ok:
            print(f"showcase composition failed: {msg}", file=sys.stderr)
            return 1
        if args.max_near_black_ratio >= 0.0:
            print(msg)
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
