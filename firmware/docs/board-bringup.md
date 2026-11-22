# ESP32-P4 開發板：新板上線流程（可重複使用）

每買一塊同型號的板子，照這份文件從頭做一次。第一塊板子（序號 `80:F1:B2:D3:B4:D2`）在 2026-11-22 做到階段 4：程式已燒進 P4、開機正常、看得到 C6；WiFi、裝置代碼、馬達腳位還沒設定。

## 0. 原則：怎樣才不會把板子弄到無法更新

板子上有兩顆晶片，風險不同。

| 晶片 | 怎麼燒 | 會不會救不回來 | 救援方法 |
|---|---|---|---|
| ESP32-P4（主晶片） | USB-C 接 UART 孔 | 不會。下載程式在晶片的 ROM 裡，flash 寫壞也能重燒 | 按住 `BOOT`、按一下 `RST`、放開 `BOOT`，再用 esptool 燒回備份 |
| ESP32-C6（WiFi） | 只能由 P4 經 SDIO 傳（OTA），或用 `PROG_C6` 排針 | 會。新韌體如果無法和 P4 通訊，P4 就再也傳不進去 | 只剩 `PROG_C6` 排針＋3.3V USB-UART 轉接器 |

五條規則：

1. **不得動 eFuse。** 不得執行 `espefuse.py burn_*`；不得在 menuconfig 開 Secure Boot、Flash Encryption。eFuse 寫了不能改，這是 P4 唯一救不回來的路。
2. **寫入 flash 前，必須有備份，而且備份比對過。** 備份必須放在兩個地方（見階段 2）。
3. **先做能還原的事，再做不能還原的事。** 順序固定：備份 → 燒 P4 → 最後才考慮 C6。
4. **升級 C6 前，救援工具必須在手上**（階段 5 的檢查表）。沒有就不升級。
5. **燒錄或 OTA 進行中，不得拔線、斷電、關筆電螢幕。**

esptool 拒絕燒錄時（例：`requires chip revision in range ...`），不得加 `--force`。先找出設定哪裡不符。

## 1. 連線與辨識（不寫入）

1. 用能傳資料的 USB-C 線，接板子的 **UART 孔**，直接插 Mac 機身。
2. Mac 跳出「允許配件連線？」時按「允許」。沒按之前，USB 清單裡完全看不到板子。
3. 確認序列埠：`ls /dev/cu.usbmodem*`（第一塊是 `/dev/cu.usbmodem101`）。
4. 讀開機 log，存檔留底：
   ```sh
   . ~/esp/idf-env.sh
   idf.py -p /dev/cu.usbmodem101 monitor   # Ctrl+] 離開；另存成 bootlog_<日期>.txt
   ```
5. 對照下表。**任何一項不同，就停下來**：表示不是同一批板子，後面的設定不能直接套用。

| 項目 | 第一塊的值 |
|---|---|
| 晶片 | `ESP-ROM:esp32p4-eco2`，chip revision v1.3（v3.x 的板子必須拿掉 `sdkconfig.defaults` 的 `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`） |
| Flash／PSRAM | 16MB／32MB |
| 螢幕／觸控 | EK79007 1024×600／GT911 |
| C6 | `Identified slave [esp32c6]`，`Reset slave using GPIO[54]` |
| C6 韌體版本 | 2.3.0（log 的 `Co-proc [x.y.z]`） |
| 出廠程式 | `esp_brookesia_demo`，ESP-IDF v5.5.4 |

不要接網路孔來開發：出廠程式不使用乙太網路，只會看到線路接通。

## 2. 備份出廠程式（只讀取）

```sh
. ~/esp/idf-env.sh
SN=<序號去掉冒號，小寫>            # 例：80f1b2d3b4d2
D=~/esp/backup/esp32p4-$SN && mkdir -p $D && cd $D
esptool.py --chip esp32p4 -p /dev/cu.usbmodem101 -b 921600 read_flash 0 0x1000000 factory_16MB_$(date +%F).bin
esptool.py --chip esp32p4 -p /dev/cu.usbmodem101 -b 921600 verify_flash 0 factory_16MB_*.bin   # 必須出現 verify OK
shasum -a 256 factory_16MB_*.bin > factory_16MB.sha256
```

- 通過條件：`verify OK (digest matched)`。
- 第二份備份：把整個 `~/esp/backup/esp32p4-$SN/` 複製到 Mac 以外的地方（雲端硬碟或 NAS）。
- 這份備份只包含 P4 的 flash。**C6 的出廠韌體無法經由 P4 備份。**

還原 P4（隨時可做）：

```sh
esptool.py --chip esp32p4 -p /dev/cu.usbmodem101 -b 921600 write_flash 0 ~/esp/backup/esp32p4-$SN/factory_16MB_*.bin
```

## 3. 開發環境（每台 Mac 做一次）

```sh
mkdir -p ~/esp && cd ~/esp
git clone -b v5.5.6 --depth 1 --recursive --shallow-submodules https://github.com/espressif/esp-idf.git esp-idf-v5.5.6
# Apple 晶片的 Mac：必須用 arm64 的 Python（3.9 以上）。先把它放到 PATH 最前面再安裝
cd esp-idf-v5.5.6 && ./install.sh esp32p4
```

- `~/esp/idf-env.sh` 負責載入環境（設定 Python 路徑，再 `source export.sh`）。
- 需要約 3.5GB 硬碟空間（ESP-IDF 0.7GB＋工具 2.6GB），建置輸出另計。
- 驗證：
  ```sh
  cd firmware && idf.py set-target esp32p4 && idf.py build   # 0 error
  make -C test_host                                           # 全部通過
  ```
- 元件版本以 `dependencies.lock` 為準。`esp_hosted` 鎖在 2.11.x；不得改成 `"*"`（會抓到 3.x）。

## 4. 把我們的程式燒進 P4（可還原）

前置：階段 2 的備份已比對、已有第二份。

1. `idf.py menuconfig` 設定 WiFi SSID、密碼、裝置代碼、`GOLF_MOTOR_GPIO`。這些值只存在 `sdkconfig`，不進 git。
2. **第一次燒錄時，馬達腳位先不接繼電器。**
3. `idf.py -p /dev/cu.usbmodem101 flash monitor`
4. 通過條件：
   - 開機 log 沒有 panic、沒有重開機循環（觀察 40 秒，`ESP-ROM:` 只出現 1 次）。
   - 看得到 `Identified slave [esp32c6]`。**WiFi SSID 空白時程式不啟動 C6，看不到這行**；只想檢查 C6 時，可以先填一個假的 SSID。
   - 記下 WiFi 是否連得上、有沒有 `Version mismatch` 與 RPC 逾時訊息。

menuconfig 的項目都在「Golfriend Ball Dispenser」選單：

| 項目 | 說明 |
|---|---|
| `GOLF_DEVICE_CODE` | 裝置代碼，同 Pi 版 `Desktop/DeviceName`（例：`TGSGF_1F_M1`）。空白時 API 停用 |
| `GOLF_API_BASE_URL` | 預設 `http://golfpoint.milkidea.com/api` |
| `GOLF_MOTOR_GPIO` | 繼電器訊號腳位。預設 -1 = 不設，馬達停用，客人不會被扣點。不得用 strapping 腳位 |
| `GOLF_WIFI1_SSID`／`GOLF_WIFI1_PASSWORD` | 主要 WiFi |
| `GOLF_WIFI2_SSID`／`GOLF_WIFI2_PASSWORD` | 備援 WiFi，可不填 |

第一塊板子 2026-11-22 的結果：開機正常；`Identified slave [esp32c6]`；`Version mismatch: Host [2.11.0] > Co-proc [2.3.0]`；出現一次 `Hosted RPC_Resp [0x21a] ... resp code [12298]`（出廠程式也有同一行）。
5. 失敗時：照階段 2 還原出廠程式，板子回到原狀。

**決策點**：C6 還是 2.3.0 時，如果 WiFi 連線、斷線重連、切換備援都正常，就不升級 C6。能不動 C6 就不動。

### 4.1 第一塊板子的 WiFi 實測（2026-11-22，C6 韌體 2.3.0、host esp_hosted 2.11.7）

| 測試 | 結果 |
|---|---|
| WiFi 連線 | 通過。開機後約 8–15 秒拿到 IP（`sta ip: 10.108.1.161`）。有一次第一次嘗試失敗，2 秒後重試成功 |
| SNTP 對時 | 通過。`time synced: 2026-11-22 05:18:39 UTC`，和 Mac 相差 1 秒內。拿到 IP 後 2–34 秒完成（3 次開機：34、4、2 秒） |
| 斷線重連（關分享器） | 通過。3 次斷線都自己連回來，各花 13.8 秒、約 26 秒、7 秒；每次都是 2 秒後重試、1 次就連上；板子沒有重開機 |
| 30 分鐘閒置監看 | RPC 逾時 0 次、重開機 0 次、panic 0 次；斷線 1 次（上表第 3 次） |
| `Version mismatch` 警告 | 每次開機都出現，沒有造成逾時 |

依 4 的決策點：**C6 先不升級。** 範圍限制：監看時板子閒置，沒有 API 流量。

現場注意：測試場地有 2 台同名（同 SSID）的基地台，網段不同（`10.108.1.x` 與 `192.168.1.x`）。
關掉一台時，板子 5 秒內連到另一台，IP 換網段。主要／備援的切換規則依 SSID 分，
同名的基地台程式分不出來。裝機時必須確認同名的基地台都能連到 API 伺服器。

斷線時 log 會印原因碼：`wifi_mgr: disconnected from [0] <SSID>: reason=<碼> rssi=<訊號>`。
常見的碼（`wifi_err_reason_t`）：8 = 自己離線、15 = 密碼錯（4-way handshake 逾時）、
200 = beacon 逾時（基地台不見了）、201 = 找不到 AP。

## 5. 升級 C6 韌體（不能保證還原，最後才做）

### 5.1 動手前的檢查表（全部打勾才做）

- [ ] 階段 4 確認過：不升級真的不行（寫下看到的錯誤訊息）。
- [ ] 板子上找得到 `PROG_C6` 排針（EN、TXD、RXD、GND、IO0），已拍照。
- [ ] 手上有 3.3V 的 USB-UART 轉接器或 ESP-Prog，Mac 認得到。
- [ ] C6 韌體版本和 `dependencies.lock` 裡的 `esp_hosted` 版本完全相同。
- [ ] C6 韌體用預設設定建置：target = esp32c6、Transport = SDIO。沒有改其他選項。
- [ ] P4 的備份在兩個地方。
- [ ] 筆電接電源，USB 線不會被碰到。

### 5.2 方法 A：由 P4 傳給 C6（OTA）

```sh
. ~/esp/idf-env.sh && mkdir -p ~/esp/c6-upgrade && cd ~/esp/c6-upgrade
V=2.11.7                                  # 必須等於 dependencies.lock 的 esp_hosted 版本
idf.py create-project-from-example "espressif/esp_hosted=$V:slave"
(cd slave && idf.py set-target esp32c6 && idf.py build)          # 產生 build/network_adapter.bin
idf.py create-project-from-example "espressif/esp_hosted=$V:host_performs_slave_ota"
cp slave/build/network_adapter.bin host_performs_slave_ota/components/ota_littlefs/slave_fw_bin/
cd host_performs_slave_ota && idf.py set-target esp32p4
# sdkconfig 加上 firmware/sdkconfig.defaults 的 C6／SDIO 設定；OTA Method = LittleFS
idf.py -p /dev/cu.usbmodem101 flash monitor
```

- 保留 `slave/build/network_adapter.bin` 到 `~/esp/backup/esp32p4-$SN/c6_slave_$V.bin`，下一塊板子直接用。
- 通過條件：重開後 log 的 `Co-proc` 版本等於 `$V`，沒有 `Version mismatch`。
- 之後照階段 4 燒回我們的程式。

OTA 的保護與缺口：

- 傳送中斷：新韌體寫在 C6 的另一個分割區，沒寫完就不會切換，舊韌體還在。重開板子再做一次。
- 傳完、但新韌體無法和 P4 通訊：P4 沒辦法再傳，必須用方法 B。這就是檢查表要求先備好轉接器的原因。

### 5.3 方法 B：用 `PROG_C6` 排針直接燒（救援）

1. 接線：轉接器 EN→EN、TXD→TXD、RXD→RXD、GND→GND、IO0→IO0。**VDD 不接。**
2. 讓 P4 停在下載模式，避免干擾：按住 `BOOT`、按一下 `RST`、放開 `BOOT`。
3. 在 `slave/` 目錄：`idf.py -p <轉接器的序列埠> flash monitor`。

## 6. 復盤：第一塊板子（2026-11-22）學到的事

| 發生的事 | 原因 | 已寫進流程的對策 |
|---|---|---|
| 換線、換孔都偵測不到板子 | Mac 的「允許配件連線」沒按 | 階段 1 步驟 2 |
| 接網路孔，以為連上了 | 網路孔只有線路接通，不能燒錄 | 階段 1 末段 |
| 備份一開始放在暫存資料夾 | 暫存資料夾重開機會清掉 | 階段 2：固定放 `~/esp/backup/<序號>/`，再放第二份 |
| 預設 Python 是 x86_64 | Mac 上裝了兩套 Homebrew | 階段 3：改用 arm64 Python |
| 元件版本寫 `"*"`，會抓到 esp_hosted 3.1.0 | 沒鎖版本 | 鎖 `~2.11.0`，`dependencies.lock` 進 git |
| 一開始說 C6 要升到 2.11.0 | 正確目標是「和 host 元件同版」＝2.11.7 | 5.1 檢查表 |
| 主機端測試跑到舊的執行檔 | Makefile 沒追蹤原始碼 | 已修（commit 3a14ae3） |
| 硬碟只剩 12GB | — | 階段 3 註明所需空間 |
| 第一次燒錄 esptool 拒絕：`requires chip revision in range v3.1 - v3.99 (this chip is revision v1.3)` | ESP-IDF v5.5.6 預設給 v3.x 晶片 | `sdkconfig.defaults` 加 `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`；規則：不得用 `--force` |
| 燒完看不到 C6 | WiFi SSID 空白時程式不啟動 WiFi，C6 就不會初始化 | 階段 4 通過條件 |

## 7. 尚未確認

- 這塊板子有沒有 `PROG_C6` 排針（要看實物）。
- C6 2.3.0 能不能走完 OTA（官方說明：低於 2.6.0 的版本略過「啟用」指令，靠重開完成）。
- C6 2.3.0 搭配 host 2.11.7，有 API 流量、長時間（數小時）執行時是否穩定。閒置 30 分鐘已測，見 4.1。
- 打 API、出球還沒測。
- `resp code [12298]` 的原因。
