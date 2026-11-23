#!/usr/bin/env python3
"""把板上鏡頭的灰階影像存成 PGM 檔（除錯用）。

用法：python3 tools/cam_capture.py /dev/cu.usbmodem101 out.pgm

韌體必須開啟 GOLF_UI_DEBUG_CONSOLE。影像是 QR 辨識用的灰階圖再縮一半。
不用 pyserial：pyserial 開埠會重置板子。
"""
import os
import select
import sys
import termios
import time

TIMEOUT_S = 20
MAX_MISSING_ROWS = 4


def open_port(path):
    fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    attrs = termios.tcgetattr(fd)
    attrs[0] = 0
    attrs[1] = 0
    attrs[2] = (attrs[2] & ~termios.HUPCL) | termios.CLOCAL | termios.CREAD
    attrs[3] = 0
    termios.tcsetattr(fd, termios.TCSANOW, attrs)
    return fd


def read_dump(fd):
    buf = b""
    end = time.time() + TIMEOUT_S
    while time.time() < end and b"CAMDUMP-END" not in buf:
        ready, _, _ = select.select([fd], [], [], 0.5)
        if not ready:
            continue
        try:
            buf += os.read(fd, 65536)
        except BlockingIOError:
            pass
    if b"CAMDUMP-END" not in buf:
        raise SystemExit("逾時：沒有收到 CAMDUMP-END")
    return buf


def parse(buf):
    lines = buf.decode("ascii", "replace").splitlines()
    start = next(i for i, line in enumerate(lines) if line.startswith("CAMDUMP "))
    width, height = (int(v) for v in lines[start].split()[1:3])
    rows = []
    for line in lines[start + 1:]:
        if line.startswith("CAMDUMP-END"):
            break
        line = line.strip()
        if len(line) == width * 2:  # 夾在中間的 log 行長度不符，略過
            try:
                rows.append(bytes.fromhex(line))
            except ValueError:
                pass
    # 板子的 log 偶爾會插進某一列，那一列就收不到。少幾列時用最後一列補齊，只影響除錯圖。
    if not rows or height - len(rows) > MAX_MISSING_ROWS or len(rows) > height:
        raise SystemExit(f"列數錯誤：預期 {height}，收到 {len(rows)}")
    rows += [rows[-1]] * (height - len(rows))
    return width, height, b"".join(rows)


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    fd = open_port(sys.argv[1])
    time.sleep(0.3)
    os.write(fd, b"v")
    width, height, pixels = parse(read_dump(fd))
    with open(sys.argv[2], "wb") as out:
        out.write(f"P5\n{width} {height}\n255\n".encode() + pixels)
    print(f"{sys.argv[2]}: {width}x{height}")


if __name__ == "__main__":
    main()
