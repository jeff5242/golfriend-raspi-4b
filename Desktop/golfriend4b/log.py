# log.py

import platform
import os
import logging
from logging.handlers import RotatingFileHandler

# 自動判斷平台
if platform.system() == "Darwin":
    BASE_PATH = os.path.expanduser("~/Desktop/golfsrc")
    DEVICE_PATH = os.path.expanduser("~/Desktop")

elif platform.system() == "Linux":
    username = os.popen("whoami").read().strip()
    
    if username == "linaro":
        BASE_PATH = f"/home/linaro/Desktop/golfsrc"
        DEVICE_PATH = f"/home/linaro/Desktop"
    elif username == "pi":
        BASE_PATH = f"/home/pi/Desktop/.Project/MediaPlayer"
        DEVICE_PATH = f"/home/pi/Desktop"
    else:
        BASE_PATH = f"/home/{username}/Desktop/golfsrc"
        DEVICE_PATH = f"/home/{username}/Desktop"
else:
    BASE_PATH = "."
    DEVICE_PATH = "."

print("BASE_PATH:", BASE_PATH)
print("DEVICE_PATH:", DEVICE_PATH)

class mLog:
    def __init__(self):
        os.makedirs(DEVICE_PATH, exist_ok=True)
        filePath = os.path.join(DEVICE_PATH, "golfTest.log")

        self.logger = logging.getLogger('golfLogger')
        self.logger.setLevel(logging.DEBUG)

        # 避免重複加入 handler（會導致 log 重複）
        if not self.logger.handlers:
            # 自動分割檔案大小的 handler
            handler = RotatingFileHandler(filePath, maxBytes=2*1024*1024, backupCount=1)
            handler.setLevel(logging.DEBUG)

            formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
            handler.setFormatter(formatter)

            self.logger.addHandler(handler)

    def d(self, Msg):
        self.logger.debug(Msg)

    def i(self, Msg):
        self.logger.info(Msg)

    def w(self, Msg):
        self.logger.warning(Msg)

    def e(self, Msg):
        self.logger.error(Msg)

    def c(self, Msg):
        self.logger.critical(Msg)

if __name__ == '__main__':
    log = mLog()
    log.d("Debug message")
    log.i("Info message")
    log.w("Warning message")
    log.e("Error message")
    log.c("Critical message")
