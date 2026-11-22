#!/usr/bin/env python3
"""從板子抓目前的畫面，存成 PNG（寬高各縮小一半）。

韌體必須開啟 menuconfig 的 GOLF_UI_DEBUG_CONSOLE，電腦接板子的 USB Serial/JTAG 埠。
只用 Python 標準函式庫（macOS／Linux）：

    python3 tools/ui_capture.py <序列埠> <輸出.png> [示範畫面字元]

示範畫面字元：s 待機、i 輸入、q 查詢中、f 確認、h 確認（點數不公開）、d 出球中、e 錯誤。
不給字元時抓目前的畫面。抓到的是程式畫的內容；面板本身的偏色或條紋抓不到。
"""
import os
import re
import select
import struct
import sys
import termios
import time
import tty
import zlib

TIMEOUT_S = 90
ROW_PATTERN = re.compile(r"([0-9a-f]{4}(\*\d+)? ?)+")


def open_port(name):
    """開啟序列埠但不重置板子：不用 pyserial（它開埠時會動 DTR/RTS），關閉 HUPCL。"""
    fd = os.open(name, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    tty.setraw(fd)
    attrs = termios.tcgetattr(fd)
    attrs[2] &= ~termios.HUPCL
    attrs[2] |= termios.CLOCAL
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    return fd


def read_until(fd, marker, fail=None):
    deadline = time.time() + TIMEOUT_S
    buf = b""
    while time.time() < deadline:
        if select.select([fd], [], [], 0.5)[0]:
            buf += os.read(fd, 65536)
        if marker in buf:
            return buf
        if fail is not None and fail in buf:
            sys.exit("板子回報抓畫面失敗")
    sys.exit(f"逾時：沒有收到 {marker.decode()}（韌體有開 GOLF_UI_DEBUG_CONSOLE 嗎？）")


def decode_rows(text, width):
    rows = []
    for line in text.splitlines():
        if not ROW_PATTERN.fullmatch(line.strip()):
            continue  # 混進來的 log 行
        pixels = []
        for token in line.split():
            value, _, count = token.partition("*")
            v = int(value, 16)
            rgb = bytes(((v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31))
            pixels.append(rgb * (int(count) if count else 1))
        row = b"".join(pixels)
        if len(row) != width * 3:
            sys.exit(f"第 {len(rows)} 行長度錯誤（序列埠資料毀損），請重抓")
        rows.append(row)
    return rows


def write_png(path, width, rows):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    raw = b"".join(b"\0" + row for row in rows)
    header = struct.pack(">IIBBBBB", width, len(rows), 8, 2, 0, 0, 0)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw))
                + chunk(b"IEND", b""))


def main():
    if len(sys.argv) not in (3, 4):
        sys.exit(__doc__)
    fd = open_port(sys.argv[1])
    if len(sys.argv) == 4:
        key = sys.argv[3].encode()
        os.write(fd, key)
        read_until(fd, b"UIDEBUG-SHOWN " + key)
    os.write(fd, b"c")
    buf = read_until(fd, b"FBDUMP-END", b"FBDUMP-FAIL")
    match = re.search(rb"FBDUMP-BEGIN (\d+) (\d+)\r?\n(.*?)FBDUMP-END", buf, re.S)
    if match is None:
        sys.exit("資料格式錯誤，請重抓")
    width, height = int(match.group(1)), int(match.group(2))
    rows = decode_rows(match.group(3).decode("ascii", "replace"), width)
    if len(rows) != height:
        sys.exit(f"只收到 {len(rows)}/{height} 行，請重抓")
    write_png(sys.argv[2], width, rows)
    print(f"{sys.argv[2]}: {width}x{height}")


if __name__ == "__main__":
    main()
