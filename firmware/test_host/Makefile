# 主機端單元測試：只編譯不依賴 ESP-IDF 的純邏輯模組。
# 用法：make -C firmware/test_host
CC      ?= clang
CFLAGS  := -std=c11 -Wall -Wextra -Werror -g
COMP    := ../components
BIN     := bin

INC := -I$(COMP)/scanner/include -I$(COMP)/motor/include -I$(COMP)/api/include \
       -I$(COMP)/wifi_mgr/include -I../main

TESTS := test_hid_keymap test_scanner_line test_motor_plan test_api_sign \
         test_api_errors test_wifi_policy test_app_fsm test_app_dispense

SRC_test_hid_keymap    := $(COMP)/scanner/hid_keymap.c
SRC_test_scanner_line  := $(COMP)/scanner/scanner_line.c
SRC_test_motor_plan    := $(COMP)/motor/motor_plan.c
SRC_test_api_sign      := $(COMP)/api/api_sign.c
SRC_test_api_errors    := $(COMP)/api/api_errors.c
SRC_test_wifi_policy   := $(COMP)/wifi_mgr/wifi_policy.c
SRC_test_app_fsm       := ../main/app_fsm.c
SRC_test_app_dispense  := ../main/app_dispense.c $(COMP)/motor/motor_plan.c

LIBS_test_motor_plan   := -lm
LIBS_test_app_dispense := -lm

.PHONY: all test clean
all: test

$(BIN):
	mkdir -p $(BIN)

$(BIN)/%: %.c test_util.h | $(BIN)
	$(CC) $(CFLAGS) $(INC) -o $@ $< $(SRC_$*) $(LIBS_$*)

test: $(addprefix $(BIN)/,$(TESTS))
	@fail=0; for t in $(TESTS); do ./$(BIN)/$$t || fail=1; done; exit $$fail

clean:
	rm -rf $(BIN)
