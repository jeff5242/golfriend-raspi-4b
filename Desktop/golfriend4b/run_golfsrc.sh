#!/bin/bash

# 防止螢幕休眠
export DISPLAY=:0
xset s off
xset -dpms
xset s noblank

# 關閉桌面面板（避免白色橫條遮擋 kiosk 畫面）
killall lxpanel 2>/dev/null
killall wf-panel-pi 2>/dev/null

# 啟動 Python 播放程式
cd "$(dirname "$0")"
python3 MediaPlayer.py