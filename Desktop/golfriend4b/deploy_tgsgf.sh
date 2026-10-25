#!/bin/bash
# 鐵砧山高球中心 Pi 4B 新碟部署（image clone 後的 Clone-1 ~ Clone-5）
# 用法: bash deploy_tgsgf.sh <PI_IP> <機號>   例: bash deploy_tgsgf.sh 192.168.2.3 1F_M1
set -euo pipefail

PI_IP="${1:?用法: deploy_tgsgf.sh <PI_IP> <樓層_機號 例 1F_M1>}"
UNIT="${2:?用法: deploy_tgsgf.sh <PI_IP> <樓層_機號 例 1F_M1>}"
DEVICE_NAME="TGSGF_${UNIT}"
HOSTNAME="${DEVICE_NAME//_/-}"
USER=milkidea
PASS='0penBmc'
SRC=/Users/jef/CodeRepository/golfriend-raspi-4b/Desktop/golfriend4b
BUILD="$(date +%Y%m%d%H)"
TMP="$(mktemp -d)"

run()  { sshpass -p "$PASS" ssh -o StrictHostKeyChecking=no "$USER@$PI_IP" "$@"; }
sudo_run() { run "echo '$PASS' | sudo -S bash -c $(printf %q "$1")"; }
push() { sshpass -p "$PASS" scp -o StrictHostKeyChecking=no "$1" "$USER@$PI_IP:$2"; }

echo "== 0. 連線測試 =="; run 'hostname; uptime'

echo "== 1. NetworkManager enable + WiFi radio 持久化 =="
sudo_run 'systemctl enable NetworkManager; systemctl start NetworkManager'
sudo_run 'printf "[main]\nwifi.enabled=true\n" > /etc/NetworkManager/conf.d/wifi-enable.conf'
sudo_run 'nmcli radio wifi on'

echo "== 2. WiFi 連線設定 =="
sudo_run "nmcli con add type wifi ifname wlan0 con-name 'HUAWEI-B315-65D9' ssid 'HUAWEI-B315-65D9' wifi-sec.key-mgmt wpa-psk wifi-sec.psk 'BEE2E20D3MM' connection.autoconnect yes || true"
sudo_run "nmcli con add type wifi ifname wlan0 con-name '5242' ssid '5242' wifi-sec.key-mgmt wpa-psk wifi-sec.psk '0936923912' connection.autoconnect yes || true"
sudo_run "nmcli con add type wifi ifname wlan0 con-name '潔芙的i16' ssid '潔芙的i16' wifi-sec.key-mgmt wpa-psk wifi-sec.psk '0936923912' connection.autoconnect yes || true"

echo "== 3. 部署程式碼 (build:$BUILD) =="
sed "s/build:[0-9]*'/build:$BUILD'/" "$SRC/MediaPlayer.py" > "$TMP/MediaPlayer.py"
push "$TMP/MediaPlayer.py" '~/Desktop/golfsrc/MediaPlayer.py'
for f in HttpCmdLibrary.py BallControl.py log.py run_golfsrc.sh; do push "$SRC/$f" "~/Desktop/golfsrc/$f"; done
sshpass -p "$PASS" scp -o StrictHostKeyChecking=no -r "$SRC/image/"* "$USER@$PI_IP:~/Desktop/golfsrc/image/"

echo "== 4. DeviceName=$DEVICE_NAME  hostname=$HOSTNAME =="
run "echo -n '$DEVICE_NAME' > ~/Desktop/DeviceName"
sudo_run "hostnamectl set-hostname $HOSTNAME; echo $HOSTNAME > /etc/hostname; sed -i 's/127.0.1.1.*/127.0.1.1 $HOSTNAME/' /etc/hosts; grep -q '127.0.1.1' /etc/hosts || echo '127.0.1.1 $HOSTNAME' >> /etc/hosts"

echo "== 5. 待機畫面 =="
push "$SRC/standby_鐵砧山高球中心.jpg" '~/Desktop/golfsrc/standby.jpg'

echo "== 6. 驗證 =="
run 'echo DeviceName=$(cat ~/Desktop/DeviceName); hostname; nmcli dev status; nmcli -g NAME con show'
rm -rf "$TMP"
echo
echo "完成。接著手動做："
echo "  rpi-connect：sshpass -p '$PASS' ssh $USER@$PI_IP 'rpi-connect signout; rpi-connect signin'  → 瀏覽器開 URL 綁定"
echo "  重開機：    sshpass -p '$PASS' ssh $USER@$PI_IP \"echo '$PASS' | sudo -S reboot\""
