#!/usr/bin/env python3
"""Verify a PNG written by --scenario-screenshot (CTest helper)."""
from __future__ import annotations

import argparse
import struct
import sys
import zlib


def read_png(path: str) -> tuple[int, int, bytes]:
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
        if len(raw) < height * (1 + width * 3):
            raise ValueError("truncated image data")
        return width, height, raw


def is_non_blank(raw: bytes, width: int, height: int) -> bool:
    """Reject empty GL readbacks (all zero). Stand-in flat fills are allowed."""
    stride = 1 + width * 3
    for y in range(height):
        row = raw[y * stride + 1 : y * stride + 1 + width * 3]
        for i in range(0, len(row), 3):
            if row[i] or row[i + 1] or row[i + 2]:
                return True
    return False


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("png")
    p.add_argument("--expect-width", type=int, required=True)
    p.add_argument("--expect-height", type=int, required=True)
    p.add_argument("--require-non-blank", action="store_true")
    p.add_argument("--require-missing", action="store_true",
                   help="pass only if the file does not exist (negative control)")
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
        w, h, raw = read_png(args.png)
    except ValueError as e:
        print(f"{args.png}: {e}", file=sys.stderr)
        return 1
    if w != args.expect_width or h != args.expect_height:
        print(f"size {w}x{h}, expected {args.expect_width}x{args.expect_height}", file=sys.stderr)
        return 1
    if args.require_non_blank and not is_non_blank(raw, w, h):
        print("image is all black (likely failed GL readback)", file=sys.stderr)
        return 1
    print(f"ok {args.png} {w}x{h}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
