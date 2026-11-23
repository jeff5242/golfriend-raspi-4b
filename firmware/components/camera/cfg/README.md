# 鏡頭曝光設定

`ov02c10_scan.json` 是掃碼用的影像處理設定（esp_ipa）。
從 `components/ov02c10/cfg/ov02c10_default_p4_eco4.json` 複製，只改 `agc.luma_adjust`：

| 欄位 | 原值 | 新值 | 原因 |
|---|---|---|---|
| `weight`（5x5 測光權重） | 中央 5、外圈 1 | 中央 8、中間一圈 2–4、最外圈 0 | 手機螢幕在畫面中央，比周圍亮很多。以中央為準，條碼的細線才不會被白色蓋掉 |
| `target_low`／`target`／`target_high` | 48／75／85 | 40／60／72 | 整體曝光調暗一點，保留亮部的細節 |

這個檔案適用晶片版本低於 v3 的 ESP32-P4（板上是 v1.3）。`tools/check_ipa_json.py` 會檢查權重。
