#pragma once
/*
 * 待機畫面右下角的鏡頭預覽與掃描狀態。
 * ui_scan_build、ui_scan_set_visible 必須在持有 lvgl_port_lock 時呼叫。
 */
#include <stdbool.h>

#include "lvgl.h"

/* 建立預覽區（預設隱藏） */
void ui_scan_build(lv_obj_t *parent);

void ui_scan_set_visible(bool visible);
