#!/usr/bin/env python3
"""產生畫面用的中文字型（LVGL C 檔）。

只收錄韌體字串裡實際用到的字，所以每次新增或修改畫面上的中文字串後必須重跑：

    python3 tools/gen_ui_font.py <NotoSansTC-Medium.ttf 的路徑>

字型來源：Noto Sans TC（SIL Open Font License 1.1），從 Google Fonts 下載。
需要 Node.js；轉檔工具是 LVGL 官方的 lv_font_conv（用 npx 執行）。
加上 --check 只檢查現有字型有沒有缺字，不轉檔（不需要字型檔與 Node.js）。
"""
import pathlib
import re
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "components" / "ui" / "fonts"
SOURCE_GLOBS = ("main/*.c", "main/*.h", "components/*/*.c", "components/*/*.h",
                "components/*/include/*.h")
SIZES = (24, 32, 56)
ASCII_RANGE = "0x20-0x7E"
STRING_LITERAL = re.compile(r'"(?:[^"\\\n]|\\.)*"')
COMMENT = re.compile(r"/\*.*?\*/|//[^\n]*", re.S)  # 註解裡的字不會顯示在畫面上
CHARS_MARK = "/* chars: "


def used_chars():
    """回傳所有字串常數裡的非 ASCII 字（排序後）。"""
    chars = set()
    for pattern in SOURCE_GLOBS:
        for path in ROOT.glob(pattern):
            if OUT_DIR in path.parents:
                continue
            for literal in STRING_LITERAL.findall(COMMENT.sub("", path.read_text(encoding="utf-8"))):
                chars.update(c for c in literal if ord(c) > 0x7E)
    return "".join(sorted(chars))


def font_path(size):
    return OUT_DIR / f"ui_font_tc_{size}.c"


def check(chars):
    """現有字型檔記錄的字集必須包含目前用到的每個字。"""
    ok = True
    for size in SIZES:
        path = font_path(size)
        text = path.read_text(encoding="utf-8") if path.exists() else ""
        line = next((l for l in text.splitlines() if l.startswith(CHARS_MARK)), "")
        missing = [c for c in chars if c not in line]
        if missing:
            ok = False
            print(f"{path.name}: 缺字 {''.join(missing)}")
    return ok


def generate(ttf, chars):
    for size in SIZES:
        path = font_path(size)
        subprocess.run(
            ["npx", "--yes", "lv_font_conv@1.5.3", "--font", ttf, "--size", str(size),
             "--bpp", "4", "--format", "lvgl", "--no-compress", "--lv-include", "lvgl.h",
             "-r", ASCII_RANGE, "--symbols", chars, "-o", str(path)],
            check=True)
        # 在檔尾記錄字集，給 --check 用
        with path.open("a", encoding="utf-8") as f:
            f.write(f"\n{CHARS_MARK}{chars} */\n")
        print(f"{path.name}: {len(chars)} 個中文字與符號，{path.stat().st_size} bytes")


def main():
    chars = used_chars()
    if len(sys.argv) == 2 and sys.argv[1] == "--check":
        if not check(chars):
            sys.exit("字型缺字：重跑 tools/gen_ui_font.py <字型檔>")
        print(f"字型涵蓋全部 {len(chars)} 個字")
        return
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    generate(sys.argv[1], chars)


if __name__ == "__main__":
    main()
