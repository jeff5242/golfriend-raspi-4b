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
| `test_scanner_line` | Enter／Tab／`%` 結束一行、1 秒按鍵保護、超長整行丟棄、回待機清掉沒打完的字（掃碼進行中的不清） |
| `test_scanner_devs` | 經 USB Hub 同時接多台輸入裝置時，每台的按鍵狀態與沒打完的字分開存，拔掉一台只清那台 |
| `test_motor_plan` | 計時模式 `ceil(take/5)*timer`；計盤模式盤數、通電、間隔（含鐵砧山設定）；擋下四捨五入成 0 ms、`inf`／`nan` |
| `test_motor` | 用 `idf_stub/` 替身編譯 `motor.c`：計時器啟動失敗時腳位拉 LOW，並回報出球中斷 |
| `test_api_sign` | 簽章字串串接順序、表單編碼與 Python `urlencode` 一致 |
| `test_api_errors` | 後台錯誤代碼 → 中文訊息；網路錯誤不顯示 `Unknow Error` |
| `test_wifi_policy` | 主要／備援 SSID 切換；回頭試主要 SSID 前先掃描，看不到就留在備援 |
| `test_app_fsm` | 出球流程狀態轉換、忙碌中不收第二筆 |
| `test_app_dispense` | 送確認（扣點）前先檢查馬達與出球參數，出不了球就不扣點（Pi 版是先確認再出球） |
| `test_wifi_scan_list` | WiFi 設定畫面的掃描結果：去掉隱藏 SSID、同名只留最強、依訊號排序；斷線原因分成密碼錯誤／找不到網路 |

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

## WiFi 設定（畫面）

有線與 WiFi 都沒連上時，待機畫面右上角會出現「WiFi 設定」鍵（領球流程中不顯示）。

1. 按下後掃描附近的 WiFi，依訊號由強到弱列出（同名的 AP 只列一次）。
2. 點需要密碼的網路 → 鍵盤輸入密碼 → 按「連線」或鍵盤上的 ✓；開放網路點了直接連。
3. 20 秒內拿到 IP 算成功：新網路存成 `wifi1`，原本的 `wifi1` 退成 `wifi2`（備援），存進 NVS，重開機後照用。
4. 失敗時畫面顯示原因（密碼錯誤／找不到這個 WiFi／連線失敗），WiFi 換回原本存的設定，可以重選或重打密碼。
5. 設定畫面 60 秒沒有操作就回待機；設定中不收掃碼與維修碼，韌體更新會延後。

限制：

- 字型只收錄韌體字串用到的中文字，SSID 裡的中文字顯示不出來（英數字正常）。
- 只支援密碼（WPA/WPA2/WPA3-Personal）與開放網路；企業版認證（需要帳號）不列出。
- 隱藏 SSID 不列出，要用 TF 卡的 `config.txt` 設定。
- TF 卡留在板子上、而且 `config.txt` 有寫 WiFi 時，每次開機匯入都會蓋掉畫面上設定的 WiFi。

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
| 純邏輯模組＋主機端測試 | 已完成，265 項檢查通過 |
| ESP-IDF 接線層（wifi_mgr、scanner、api、motor、app_main） | 已寫，**未用 `idf.py build` 編譯過** |
| ui | 骨架，只印 log |
| OTA、遠端 log、WiFi 設定頁 | 未開始 |
| 維修碼（Pi 版 `7777777` 關機） | 未決定 MCU 上的對應動作 |

## 待確認

- 板子廠牌、型號、腳位圖（有沒有 5V、USB Host 接頭型式、哪些 GPIO 可用）。
- ESP32-C6 出廠的 esp_hosted 韌體版本，是否和 `esp_hosted` 元件相容。
- `json`（cJSON）元件在所用 ESP-IDF 版本是內建還是要從 Component Registry 加。
- 後台是否支援用 ID 卡號領球（目前 API 收的是 QR Code 的 `transaction_code`）。
