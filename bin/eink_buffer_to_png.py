#!/usr/bin/env python3
"""Convert an E-Ink OLEDDisplay buffer dump into a PNG.

Firmware layout (EInkDisplay::forceDisplay):
  byte = buffer[x + (y // 8) * width]
  bit  = 1 << (y & 7)
  bit set → black on panel, unset → white

Dump format (enabled with -DEINK_DUMP_BUFFER):
  EINK_BUFFER_BEGIN w=250 h=122 size=4000
  0000:aabbcc...   # 32 bytes per line
  EINK_BUFFER_END

Examples:
  python3 bin/eink_buffer_to_png.py serial.log -o eink.png
  python3 bin/eink_buffer_to_png.py --port /dev/tty.usbmodemXXXX -o eink.png
  python3 bin/eink_buffer_to_png.py --bin buffer.bin --width 250 --height 122 -o eink.png
"""

from __future__ import annotations

import argparse
import re
import struct
import sys
import time
import zlib
from pathlib import Path

ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")
BEGIN_RE = re.compile(r"EINK_BUFFER_BEGIN w=(\d+) h=(\d+) size=(\d+)(?: nz=(\d+))?")
# Offset must be 4 hex digits; payload is contiguous hex (64 bytes → 128 chars).
LINE_RE = re.compile(r"(?<![0-9a-fA-F])([0-9a-fA-F]{4}):([0-9a-fA-F]{4,})")
END_RE = re.compile(r"EINK_BUFFER_END")
DUMP_RE = re.compile(
    r"EINK_BUFFER_BEGIN w=(\d+) h=(\d+) size=(\d+)(?: nz=(\d+))?(.*?)EINK_BUFFER_END",
    re.DOTALL,
)


def strip_ansi(text: str) -> str:
    return ANSI_RE.sub("", text)


def buffer_to_pixels(buf: bytes, width: int, height: int) -> list[int]:
    """Return row-major 0/1 pixels (1 = black)."""
    needed = width * ((height + 7) // 8)
    if len(buf) < needed:
        raise ValueError(f"buffer too short: got {len(buf)} bytes, need {needed} for {width}x{height}")
    pixels = [0] * (width * height)
    for y in range(height):
        row_base = (y // 8) * width
        bit = 1 << (y & 7)
        for x in range(width):
            if buf[row_base + x] & bit:
                pixels[y * width + x] = 1
    return pixels


def write_png(path: Path, pixels: list[int], width: int, height: int, scale: int) -> None:
    w = width * scale
    h = height * scale
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        src_y = y // scale
        row = src_y * width
        for x in range(w):
            raw.append(0 if pixels[row + (x // scale)] else 255)

    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 0, 0, 0, 0)
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")
    path.write_bytes(png)


def _parse_hex_payload(hex_str: str) -> bytes | None:
    if len(hex_str) % 2:
        hex_str = hex_str[:-1]
    if len(hex_str) < 4:
        return None
    try:
        return bytes.fromhex(hex_str)
    except ValueError:
        return None


def parse_dumps(text: str) -> list[tuple[int, int, bytes]]:
    dumps: list[tuple[int, int, bytes]] = []
    for match in DUMP_RE.finditer(strip_ansi(text)):
        width = int(match.group(1))
        height = int(match.group(2))
        size = int(match.group(3))
        body = match.group(5) or ""
        buf = bytearray(size)
        for hex_match in LINE_RE.finditer(body):
            offset = int(hex_match.group(1), 16)
            data = _parse_hex_payload(hex_match.group(2))
            if data is None or offset >= size:
                continue
            end = min(offset + len(data), size)
            buf[offset:end] = data[: end - offset]
        dumps.append((width, height, bytes(buf)))
    return dumps


def nonzero_count(buf: bytes) -> int:
    return sum(1 for b in buf if b)


def pick_dump(
    dumps: list[tuple[int, int, bytes]], index: int | None, skip_empty: bool
) -> tuple[int, int, bytes]:
    if not dumps:
        raise SystemExit("no EINK_BUFFER dump found")
    if index is not None:
        idx = index if index >= 0 else len(dumps) + index
        if idx < 0 or idx >= len(dumps):
            raise SystemExit(f"dump index {index} out of range (found {len(dumps)})")
        width, height, buf = dumps[idx]
        nz = nonzero_count(buf)
        print(f"using dump {idx + 1}/{len(dumps)} ({width}x{height}, {len(buf)} bytes, {nz} nonzero)", file=sys.stderr)
        if skip_empty and nz == 0:
            raise SystemExit("selected dump is all zeros (empty OLED buffer). Refresh the screen and capture again.")
        return width, height, buf

    chosen = None
    chosen_i = -1
    for i, dump in enumerate(dumps):
        if skip_empty and nonzero_count(dump[2]) == 0:
            continue
        chosen = dump
        chosen_i = i
    if chosen is None:
        raise SystemExit(
            f"all {len(dumps)} dump(s) are empty. Wait until the UI has drawn, then refresh once more."
        )
    width, height, buf = chosen
    nz = nonzero_count(buf)
    print(f"using dump {chosen_i + 1}/{len(dumps)} ({width}x{height}, {len(buf)} bytes, {nz} nonzero)", file=sys.stderr)
    return width, height, buf


def _open_serial(port: str, baud: int):
    try:
        import serial  # type: ignore
    except ImportError as exc:
        raise SystemExit("live capture needs pyserial: python3 -m pip install pyserial") from exc

    # Same open path that previously received EINK_BUFFER_* text.
    # Do not send protobuf: that would switch the device to usingProtobufs
    # and hide raw log lines unless debug_log_api_enabled is on.
    ser = serial.Serial(port, baud, timeout=0.5)
    return ser


def read_serial(port: str, baud: int, timeout: float, skip_empty: bool) -> str:
    print(
        f"waiting for EINK_BUFFER dump on {port} (do not send protobuf; press a key to refresh)...",
        file=sys.stderr,
        flush=True,
    )
    ser = _open_serial(port, baud)
    acc = bytearray()
    seen_begin = 0
    skipped = 0
    last_report = time.time()
    try:
        start = time.time()
        while True:
            now = time.time()
            if timeout and (now - start) > timeout:
                extra = f", skipped {skipped} empty" if skipped else ""
                raise SystemExit(
                    f"timed out after {timeout}s ({len(acc)} bytes received, {seen_begin} BEGIN{extra}). "
                    "If bytes stay 0, another process may still own the port, or this firmware "
                    "was not built with -DEINK_DUMP_BUFFER."
                )
            chunk = ser.read(4096)
            if chunk:
                acc.extend(chunk)
                if len(acc) > 512 * 1024:
                    acc = acc[-256 * 1024 :]
            if now - last_report >= 5:
                begins = acc.decode("latin-1", errors="ignore").count("EINK_BUFFER_BEGIN")
                print(
                    f"... {len(acc)} bytes received, {begins} BEGIN marker(s) so far",
                    file=sys.stderr,
                    flush=True,
                )
                last_report = now
            if not chunk:
                continue
            text = acc.decode("latin-1", errors="ignore")
            seen_begin = text.count("EINK_BUFFER_BEGIN")
            dumps = parse_dumps(text)
            if not dumps:
                continue
            width, height, buf = dumps[-1]
            nz = nonzero_count(buf)
            if skip_empty and nz == 0:
                skipped += 1
                print(
                    f"got dump {len(dumps)} ({width}x{height}, {len(buf)} bytes, 0 nonzero); "
                    "waiting for a drawn frame...",
                    file=sys.stderr,
                    flush=True,
                )
                # Drop completed dumps so we don't keep re-parsing the empty one.
                last_end = text.rfind("EINK_BUFFER_END")
                if last_end >= 0:
                    acc = bytearray(acc[last_end + len("EINK_BUFFER_END") :])
                continue
            print(
                f"got dump {len(dumps)} ({width}x{height}, {len(buf)} bytes, {nz} nonzero)",
                file=sys.stderr,
                flush=True,
            )
            return text
    finally:
        ser.close()


def self_test() -> None:
    width, height = 16, 10
    buf = bytearray(width * ((height + 7) // 8))
    for y in range(height):
        for x in range(width):
            if (x + y) & 1:
                buf[x + (y // 8) * width] |= 1 << (y & 7)
    pixels = buffer_to_pixels(bytes(buf), width, height)
    assert pixels[0] == 0
    assert pixels[1] == 1
    assert pixels[width] == 1

    hex_body = "\n".join(
        f"\x1b[32mINFO \x1b[0m| 11:07:33 12 [Screen] {i:04x}:{bytes(buf[i : i + 32]).hex()}" for i in range(0, 32, 32)
    )
    empty = "EINK_BUFFER_BEGIN w=16 h=10 size=32 nz=0\n0000:" + ("00" * 32) + "\nEINK_BUFFER_END\n"
    real = f"EINK_BUFFER_BEGIN w=16 h=10 size=32 nz=32\n{hex_body}\nEINK_BUFFER_END\n"
    dumps = parse_dumps(empty + real)
    assert len(dumps) == 2
    assert nonzero_count(dumps[0][2]) == 0
    assert dumps[1][2] == bytes(buf)
    picked = pick_dump(dumps, None, skip_empty=True)
    assert picked[2] == bytes(buf)
    mashed = b"\x94\xc3junk" + real.encode() + b"\x00"
    mashed_dumps = parse_dumps(mashed.decode("latin-1"))
    assert mashed_dumps[-1][2] == bytes(buf)
    print("self-test ok")


def main() -> int:
    parser = argparse.ArgumentParser(description="Convert an E-Ink buffer dump to PNG")
    parser.add_argument("log", nargs="?", help="serial log containing EINK_BUFFER_* markers")
    parser.add_argument("--bin", dest="bin_path", help="raw buffer file (needs --width/--height)")
    parser.add_argument("--port", help="wait for the next dump on this serial port")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=120, help="seconds to wait on --port")
    parser.add_argument("--width", type=int, default=250, help="used with --bin (nodara default 250)")
    parser.add_argument("--height", type=int, default=122, help="used with --bin (nodara default 122)")
    parser.add_argument("-o", "--output", default="eink_buffer.png")
    parser.add_argument("--save-bin", help="also write the raw buffer bytes")
    parser.add_argument("--scale", type=int, default=4)
    parser.add_argument("--flip", action="store_true", help="mirror both axes (config.display.flip_screen)")
    parser.add_argument("--index", type=int, default=None, help="which dump in a log to use (default: last non-empty)")
    parser.add_argument("--accept-empty", action="store_true", help="allow an all-zero dump")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    skip_empty = not args.accept_empty

    if args.self_test:
        self_test()
        return 0

    width = height = None
    buf = None

    if args.bin_path:
        data = Path(args.bin_path).read_bytes()
        width, height, buf = args.width, args.height, data
        nz = nonzero_count(buf)
        print(f"raw bin {width}x{height}, {len(buf)} bytes, {nz} nonzero", file=sys.stderr)
        if skip_empty and nz == 0:
            raise SystemExit("bin file is all zeros; pass --accept-empty to render it anyway")
    elif args.port:
        text = read_serial(args.port, args.baud, args.timeout, skip_empty)
        dumps = parse_dumps(text)
        width, height, buf = pick_dump(dumps, None, skip_empty)
    elif args.log:
        dumps = parse_dumps(Path(args.log).read_text(errors="replace"))
        width, height, buf = pick_dump(dumps, args.index, skip_empty)
    else:
        parser.error("provide a log file, --bin, or --port")

    pixels = buffer_to_pixels(buf, width, height)
    if args.flip:
        flipped = [0] * len(pixels)
        for y in range(height):
            for x in range(width):
                flipped[(height - 1 - y) * width + (width - 1 - x)] = pixels[y * width + x]
        pixels = flipped

    out = Path(args.output)
    write_png(out, pixels, width, height, max(1, args.scale))
    print(f"wrote {out} ({width}x{height} @ {args.scale}x)")
    if args.save_bin:
        Path(args.save_bin).write_bytes(buf)
        print(f"wrote {args.save_bin} ({len(buf)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
