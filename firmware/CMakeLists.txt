# Golfriend 出球機 ESP32-P4 原型（POC）
# 建置：idf.py set-target esp32p4 && idf.py menuconfig && idf.py build
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(golfriend_p4)
