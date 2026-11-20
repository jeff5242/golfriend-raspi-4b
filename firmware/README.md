# Golfriend 出球機 — ESP32-P4 原型（POC）

用 ESP32-P4 7 吋觸控板取代 Raspberry Pi 的出球機原型。只做 5 件事：
掃碼／讀卡、打 API、出球、中文對話框、WiFi 重連。OTA 和遠端 log 在原型通過後再做。

評估與計畫：https://claude.ai/artifact/DvKb1rG21JiwKam4dgC8MG

## 結構

```
firmware/
├── main/
│   ├── app_main.c        # 事件迴圈：所有輸入進同一個佇列
│   ├── app_fsm.c/.h      # 狀態機（純邏輯，有測試）
│   └── Kconfig.projbuild # 裝置代碼、API 網址、馬達腳位、WiFi
├── components/
│   ├── wifi_mgr/         # wifi_policy.c（純邏輯）＋ wifi_mgr.c（esp_wifi via ESP32-C6）
│   ├── scanner/          # hid_keymap.c、scanner_line.c（純邏輯）＋ scanner.c（USB Host HID）
│   ├── api/              # api_sign.c、api_errors.c（純邏輯）＋ api.c（HTTP、SHA1、cJSON、SNTP）
│   ├── motor/            # motor_plan.c（純邏輯）＋ motor.c（GPIO、esp_timer）
│   └── ui/               # 骨架：只印 log，等板子型號確認後接 BSP 與 LVGL
└── test_host/            # 主機端單元測試（macOS／Linux，不需要 ESP-IDF）
```

## 主機端測試

純邏輯模組不依賴 ESP-IDF，可以直接在 Mac 上測：

```bash
make -C firmware/test_host
```

| 測試 | 對應 Pi 版行為 |
|---|---|
| `test_hid_keymap` | 掃碼器／鍵盤／讀卡機的 HID 鍵碼轉字元，含 Shift+5 = `%`、數字鍵盤 |
| `test_scanner_line` | Enter／Tab／`%` 結束一行、1 秒按鍵保護、超長整行丟棄 |
| `test_motor_plan` | 計時模式 `ceil(take/5)*timer`；計盤模式盤數、通電、間隔（含鐵砧山設定） |
| `test_api_sign` | 簽章字串串接順序、表單編碼與 Python `urlencode` 一致 |
| `test_api_errors` | 後台錯誤代碼 → 中文訊息 |
| `test_wifi_policy` | 主要／備援 SSID 切換、回頭試主要 SSID |
| `test_app_fsm` | 出球流程狀態轉換、忙碌中不收第二筆 |

## 裝置建置

需要 ESP-IDF v5.4 以上（ESP32-P4 支援）。

```bash
cd firmware
idf.py set-target esp32p4
idf.py menuconfig      # Golfriend Ball Dispenser：裝置代碼、馬達腳位、WiFi
idf.py build flash monitor
```

- 不得把 WiFi 密碼或裝置代碼寫進 `sdkconfig.defaults`。它們只存在 `sdkconfig`，`sdkconfig` 不進 git。
- 第一次建置成功後，`dependencies.lock` 會記下元件版本，必須 commit。

## 硬體接線（繼電器）

不得把馬達直接接到 GPIO。GPIO 只有 3.3V、幾十 mA。

| HW-482 | 接到 |
|---|---|
| S | `CONFIG_GOLF_MOTOR_GPIO` 指定的腳位（不得用 strapping 腳） |
| + | 5V |
| − | GND（必須和板子共地） |

S 腳建議再加一顆 10 kΩ 下拉電阻到 GND，開機期間 GPIO 浮接時繼電器也不會吸合。

## USB 輸入裝置

| 裝置 | 狀態 |
|---|---|
| 掃碼器、USB 鍵盤、ID 讀卡機（HID 鍵盤模式） | 已寫，未實機驗證 |
| ID 讀卡機（序列埠模式：CDC、CH340） | 預留 `scanner_handle_serial_bytes()`，驅動未加 |
| ID 讀卡機（廠商自訂 HID） | 不支援，會在 log 印出 `not a boot keyboard` |

同時接多個裝置時，必須用外接電源的 USB Hub。

## 狀態

| 項目 | 狀態 |
|---|---|
| 純邏輯模組＋主機端測試 | 已完成，133 項檢查通過 |
| ESP-IDF 接線層（wifi_mgr、scanner、api、motor、app_main） | 已寫，**未用 `idf.py build` 編譯過** |
| ui | 骨架，只印 log |
| OTA、遠端 log、WiFi 設定頁 | 未開始 |
| 維修碼（Pi 版 `7777777` 關機） | 未決定 MCU 上的對應動作 |

## 待確認

- 板子廠牌、型號、腳位圖（有沒有 5V、USB Host 接頭型式、哪些 GPIO 可用）。
- ESP32-C6 出廠的 esp_hosted 韌體版本，是否和 `esp_hosted` 元件相容。
- `json`（cJSON）元件在所用 ESP-IDF 版本是內建還是要從 Component Registry 加。
- 後台是否支援用 ID 卡號領球（目前 API 收的是 QR Code 的 `transaction_code`）。
