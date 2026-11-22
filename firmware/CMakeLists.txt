# Golfriend 出球機 ESP32-P4 原型（POC）
# 建置：idf.py set-target esp32p4 && idf.py menuconfig && idf.py build
cmake_minimum_required(VERSION 3.16)
# 韌體版本：定期回報會送到後台（note 欄位），每次出版本時更新
set(PROJECT_VER "P4-0.1.0")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(golfriend_p4)
