#!/bin/bash

# 防止螢幕休眠
export DISPLAY=:0
xset s off
xset -dpms
xset s noblank

# 啟動 Python 播放程式
cd /home/linaro/Desktop/golfsrc/
python3 MediaPlayer.py