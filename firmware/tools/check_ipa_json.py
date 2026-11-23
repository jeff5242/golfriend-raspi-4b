#!/usr/bin/env python3
"""檢查鏡頭的曝光設定檔（make -C test_host 會執行）。

掃碼時手機螢幕比周圍亮很多。曝光必須以畫面中央為準，否則條碼的細線會被白色蓋掉。
"""
import json
import sys
from pathlib import Path

PATH = Path(__file__).resolve().parent.parent / "components/camera/cfg/ov02c10_scan.json"
GRID = 5  # 測光分成 5x5 格


def check(path):
    errors = []
    if not path.exists():
        return [f"找不到 {path}"]
    agc = json.loads(path.read_text())["OV02C10"]["agc"]
    luma = agc["luma_adjust"]
    weight = luma["weight"]
    if len(weight) != GRID * GRID:
        return [f"weight 必須有 {GRID * GRID} 格，現在是 {len(weight)}"]
    centre = weight[(GRID * GRID) // 2]
    ring = [w for i, w in enumerate(weight) if i // GRID in (0, GRID - 1) or i % GRID in (0, GRID - 1)]
    inner = sum(weight) - sum(ring)
    if any(w < 0 for w in weight):
        errors.append("weight 不得是負數")
    if centre != max(weight):
        errors.append("正中央那一格的權重必須最大")
    if sum(ring) * 4 > inner:
        errors.append("最外圈的權重合計不得超過中間 3x3 的四分之一")
    if not luma["target_low"] < luma["target"] < luma["target_high"]:
        errors.append("target 必須在 target_low 和 target_high 之間")
    if agc.get("mode") != "high_light_priority":
        errors.append("mode 必須是 high_light_priority（亮的地方不得過曝）")
    return errors


def main():
    errors = check(PATH)
    for e in errors:
        print(f"曝光設定檔：{e}")
    if not errors:
        print("曝光設定檔：以畫面中央為準")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
