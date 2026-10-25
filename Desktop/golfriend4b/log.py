# log.py

import platform
import os
import logging
import json
from logging.handlers import RotatingFileHandler
import threading
import time
from queue import Queue, Empty

if platform.system() == "Darwin":
    BASE_PATH = os.path.expanduser("~/Desktop/golfsrc")
    DEVICE_PATH = os.path.expanduser("~/Desktop")
elif platform.system() == "Linux":
    username = os.popen("whoami").read().strip()
    if username == "linaro":
        BASE_PATH = "/home/linaro/Desktop/golfsrc"
        DEVICE_PATH = "/home/linaro/Desktop"
    elif username == "pi":
        BASE_PATH = "/home/pi/Desktop/.Project/MediaPlayer"
        DEVICE_PATH = "/home/pi/Desktop"
    else:
        BASE_PATH = f"/home/{username}/Desktop/golfsrc"
        DEVICE_PATH = f"/home/{username}/Desktop"
else:
    BASE_PATH = "."
    DEVICE_PATH = "."

print("BASE_PATH:", BASE_PATH)
print("DEVICE_PATH:", DEVICE_PATH)

LOGTAIL_TOKEN_FILE = os.path.join(BASE_PATH, "logtail_token")

# 本機檔案 log 放 /tmp：Pi 上 /tmp 是 tmpfs，斷電不會留下寫到一半的檔案，
# 也不會磨損硬碟；歷史紀錄由 Logtail 保存，本機只留這次開機的除錯用。
LOG_DIR = "/tmp"
LOG_FILE = os.path.join(LOG_DIR, "golfTest.log")
LOG_MAX_BYTES = 1 * 1024 * 1024   # 單檔 1MB
LOG_BACKUP_COUNT = 2              # 最多保留 golfTest.log.1 / .2，合計約 3MB
DEVICE_NAME_FILE = os.path.join(DEVICE_PATH, "DeviceName")

# Log 分類標籤
TAG_SYSTEM = "system"       # 系統性：啟動、初始化、網路、設定
TAG_OPERATION = "operation"  # 操作性：掃碼、取球、確認、馬達控制
TAG_EXCEPTION = "exception"  # 例外狀況：錯誤、異常、逾時
TAG_INFO = "info"            # 一般資訊類


def _read_file_strip(path):
    try:
        with open(path) as f:
            return f.readline().strip()
    except Exception:
        return ""


def create_file_handler(path=LOG_FILE, max_bytes=LOG_MAX_BYTES, backup_count=LOG_BACKUP_COUNT):
    """建立會自動輪替的檔案 handler；路徑不可寫時回傳 None，不中斷主程式"""
    try:
        handler = RotatingFileHandler(
            path, maxBytes=max_bytes, backupCount=backup_count, encoding="utf-8"
        )
    except OSError as e:
        print(f"File log disabled ({path}): {e}")
        return None
    handler.setLevel(logging.DEBUG)
    handler.setFormatter(
        logging.Formatter('%(asctime)s %(levelname)s [%(tag)s] %(message)s')
    )
    return handler


class LogtailHandler(logging.Handler):
    """批次送 log 到 Better Stack Logtail，背景執行不阻塞主程式"""

    ENDPOINT = "https://in.logs.betterstack.com"
    FLUSH_INTERVAL = 5
    BATCH_SIZE = 20

    def __init__(self, token, device_name):
        super().__init__()
        self.token = token
        self.device_name = device_name
        self._queue = Queue()
        self._stop_event = threading.Event()
        self._thread = threading.Thread(target=self._worker, daemon=True)
        self._thread.start()

    def emit(self, record):
        tag = getattr(record, 'tag', TAG_INFO)
        TAG_LABELS = {TAG_SYSTEM: "系統", TAG_OPERATION: "操作", TAG_EXCEPTION: "例外", TAG_INFO: "資訊"}
        tag_label = TAG_LABELS.get(tag, tag)
        msg = f"[{self.device_name}] [{tag_label}] {record.getMessage()}"
        entry = {
            "dt": time.strftime("%Y-%m-%dT%H:%M:%S%z", time.localtime(record.created)),
            "level": record.levelname,
            "message": msg,
            "device": self.device_name,
            "tag": tag,
        }
        self._queue.put(entry)

    def _worker(self):
        import requests
        while not self._stop_event.is_set():
            batch = []
            try:
                deadline = time.time() + self.FLUSH_INTERVAL
                while len(batch) < self.BATCH_SIZE:
                    remaining = deadline - time.time()
                    if remaining <= 0:
                        break
                    item = self._queue.get(timeout=remaining)
                    batch.append(item)
            except Empty:
                pass

            if batch:
                self._send(requests, batch)

    def _send(self, requests, batch):
        try:
            requests.post(
                self.ENDPOINT,
                headers={
                    "Authorization": f"Bearer {self.token}",
                    "Content-Type": "application/json",
                },
                data=json.dumps(batch),
                timeout=10,
            )
        except Exception as e:
            print(f"Logtail send error: {e}")


class mLog:
    def __init__(self):
        self.logger = logging.getLogger('golfLogger')
        self.logger.setLevel(logging.DEBUG)

        if not self.logger.handlers:
            device_name = _read_file_strip(DEVICE_NAME_FILE) or "UNKNOWN"
            token = _read_file_strip(LOGTAIL_TOKEN_FILE)

            if token:
                handler = LogtailHandler(token, device_name)
                handler.setLevel(logging.DEBUG)
                self.logger.addHandler(handler)
                print(f"Logtail enabled, device={device_name}")
            else:
                print("No logtail_token found, logging to stdout only")

            file_handler = create_file_handler()
            if file_handler:
                self.logger.addHandler(file_handler)
                print(f"File log: {LOG_FILE}")

            console = logging.StreamHandler()
            console.setLevel(logging.DEBUG)
            console.setFormatter(
                logging.Formatter(f'[{device_name}] %(levelname)s [%(tag)s] %(message)s')
            )
            self.logger.addHandler(console)

    def _log(self, level_func, msg, tag):
        level_func(msg, extra={'tag': tag})

    # --- 帶標籤的 log 方法 ---
    def d(self, Msg, tag=TAG_INFO):
        self._log(self.logger.debug, Msg, tag)

    def i(self, Msg, tag=TAG_INFO):
        self._log(self.logger.info, Msg, tag)

    def w(self, Msg, tag=TAG_EXCEPTION):
        self._log(self.logger.warning, Msg, tag)

    def e(self, Msg, tag=TAG_EXCEPTION):
        self._log(self.logger.error, Msg, tag)

    def c(self, Msg, tag=TAG_EXCEPTION):
        self._log(self.logger.critical, Msg, tag)

    # --- 快捷方法：明確指定分類 ---
    def system(self, Msg):
        self._log(self.logger.info, Msg, TAG_SYSTEM)

    def operation(self, Msg):
        self._log(self.logger.info, Msg, TAG_OPERATION)

    def exception(self, Msg):
        self._log(self.logger.error, Msg, TAG_EXCEPTION)


if __name__ == '__main__':
    log = mLog()
    log.system("程式啟動")
    log.operation("掃碼取球: code=12345")
    log.i("一般訊息")
    log.e("發生錯誤")
    log.d("除錯資訊", tag=TAG_SYSTEM)
