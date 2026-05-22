#coding:utf-8
import hashlib, datetime, time
import requests
import json
from PyQt5 import QtCore, QtGui, QtWidgets
from PyQt5.QtCore import Qt
import subprocess
import os
import platform
import socket


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

import _thread
import time
import HttpCmdLibrary
from log import mLog
import os
from BallControl import *

settings={'AudioMode':'hdmi','ScaleToScreen':'True',}
#settings={'AudioMode':'local','ScaleToScreen':'True',}
mediaLocation='/Users/jef/Desktop/golfsrc/noUSBVideo/01.mp4'
MediaStatus = True
GlobalVideoCounter = 0
GlobalPlayItem = list()
GlobalPlayItemCounter = 0
GlobalUSBFoler = '/media/pi/'
GlobalNoUSBFolder = os.path.join(BASE_PATH, "noUSBVideo/")
appVersion = 'V2.5 build:2026052174127'

class MediaPlayer(QtWidgets.QWidget):
    def __init__(self, app, parent):
        super().__init__(parent)
        self.app = app
        self.setCursor(QtGui.QCursor(QtCore.Qt.BlankCursor))
        # self.setWindowTitle('Golf Standby Screen')

        self.focusLock = False

        # 保持視窗在最前 + 全螢幕
        self.setWindowFlags(
            QtCore.Qt.FramelessWindowHint |
            QtCore.Qt.WindowStaysOnTopHint 
        )
        self.showFullScreen()
        self.raise_()
        self.activateWindow()
        self.setFocus()




        # 啟動定時器，每 2 秒強制拉回前景焦點
        self.focusTimer = QtCore.QTimer()
        self.focusTimer.timeout.connect(self.enforceFocus)
        self.focusTimer.start(3000)



        #self.labelHeight = 100


        # 取得螢幕大小並調整視窗尺寸
        screen = QtWidgets.QApplication.primaryScreen()
        screen_size = screen.size()
        self.resize(screen_size.width(), screen_size.height())

        # 載入 standby 圖片
        standbyImagePath = os.path.join(BASE_PATH, "standby.jpg")
        self.standbyLabel = QtWidgets.QLabel(self)
        pixmap = QtGui.QPixmap(standbyImagePath).scaled(
            self.size(), QtCore.Qt.KeepAspectRatio, QtCore.Qt.SmoothTransformation)
        self.standbyLabel.setPixmap(pixmap)
        self.standbyLabel.setAlignment(QtCore.Qt.AlignCenter)
        self.standbyLabel.resize(self.width(), self.height())
        self.standbyLabel.show()

        # 初始化 log 與控制邏輯
        self.Log = mLog()
        self.Control = BallControl(self.Log)

        # 讀取裝置名稱
        self.DeviceName = self.fileGetContents(os.path.join(DEVICE_PATH, "DeviceName"))

        # PingTimer（例如背景通訊用途）
        self.PingTimer = 5

        # 嘗試印出 IP 與裝置識別
        try:
            print('IP:{}'.format(self.get_ip()))
            print('Device Name:{}'.format(self.DeviceName))
        except:
            print('⚠️ 無法取得網路資訊')

        # 設定鍵盤輸入處理
        print("設定鍵盤輸入處理")

        self.setFocusPolicy(QtCore.Qt.StrongFocus)

        # 初始化控制狀態
        self.timeout = None
        self.Lock = False

        # 確保 standbyLabel 不搶事件
        self.standbyLabel.setAttribute(QtCore.Qt.WA_TransparentForMouseEvents)
        self.standbyLabel.setFocusPolicy(QtCore.Qt.NoFocus)

        # 初始延遲搶焦點
        QtCore.QTimer.singleShot(500, self.forceInitialFocus)

        # 持續搶焦點（防止後來被別人蓋掉）
        self.focusTimer = QtCore.QTimer()
        self.focusTimer.timeout.connect(self.enforceFocus)
        self.focusTimer.start(2000)
        self.showStandbyImage()

        # === 右上角狀態資訊（畫在 standby 圖片上）===
        self._cachedNetQuality = "?"  # 網路品質燈號快取
        self._cachedPingMs = -1
        self._updateStatusOverlay()

        # 每 30 秒更新一次狀態資訊（時間、IP、連線類型）
        self.statusTimer = QtCore.QTimer()
        self.statusTimer.timeout.connect(self._updateStatusOverlay)
        self.statusTimer.start(30000)

        # 網路品質燈號 — 測試階段每 5 秒，正式改為 60000
        self._netQualityTimer = QtCore.QTimer()
        self._netQualityTimer.timeout.connect(self._updateNetQuality)
        self._netQualityTimer.start(5000)
        _thread.start_new_thread(self._measurePingLatency, ())



    def _getNetworkInfo(self):
        """取得網路連線類型與 IP"""
        net_type = "offline"
        ip_addr = "N/A"
        try:
            # 檢查有線 eth0
            if platform.system() == "Linux":
                import subprocess
                # 判斷連線類型
                eth_result = subprocess.run(
                    ['ip', 'addr', 'show', 'eth0'],
                    capture_output=True, text=True, timeout=3
                )
                wlan_result = subprocess.run(
                    ['ip', 'addr', 'show', 'wlan0'],
                    capture_output=True, text=True, timeout=3
                )
                if 'inet ' in eth_result.stdout and 'state UP' in eth_result.stdout:
                    net_type = "有線 (eth0)"
                elif 'inet ' in wlan_result.stdout:
                    net_type = "無線 (wlan0)"
            # 取得對外 IP
            ip_addr = self.get_ip() or "N/A"
        except Exception as e:
            print(f"_getNetworkInfo error: {e}")
        return net_type, ip_addr

    def _updateStatusOverlay(self):
        """重新繪製 standby 圖片 + 右上角狀態文字"""
        try:
            standby_path = os.path.join(BASE_PATH, "standby.jpg")
            if not os.path.exists(standby_path):
                return

            pixmap = QtGui.QPixmap(standby_path).scaled(
                self.size(), QtCore.Qt.KeepAspectRatio, QtCore.Qt.SmoothTransformation
            )

            # 準備狀態文字（一行，含版本號）
            now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            net_type, ip_addr = self._getNetworkInfo()
            ping_text = f"{self._cachedPingMs:.0f}ms" if self._cachedPingMs >= 0 else "N/A"
            status_line = f"{now}   {net_type}   IP: {ip_addr}   ping: {ping_text}   {appVersion}"

            # 決定燈號顏色
            if self._cachedPingMs < 0:
                dot_color = QtGui.QColor(100, 100, 100)
            elif self._cachedPingMs < 50:
                dot_color = QtGui.QColor(0, 200, 0)
            elif self._cachedPingMs < 150:
                dot_color = QtGui.QColor(255, 200, 0)
            else:
                dot_color = QtGui.QColor(220, 0, 0)

            painter = QtGui.QPainter(pixmap)
            painter.setRenderHint(QtGui.QPainter.Antialiasing)

            font = QtGui.QFont()
            font.setPointSize(8)
            painter.setFont(font)
            fm = QtGui.QFontMetrics(font)

            padding = 5
            dot_size = 8
            status_w = fm.horizontalAdvance(status_line)
            line_h = fm.height()
            bar_w = dot_size + 6 + status_w + padding * 2
            bar_h = line_h + padding * 2

            # 貼底居中
            bar_x = (pixmap.width() - bar_w) / 2
            bar_y = pixmap.height() - bar_h - 2

            # 畫背景
            painter.setBrush(QtGui.QColor(0, 0, 0, 150))
            painter.setPen(QtCore.Qt.NoPen)
            painter.drawRoundedRect(QtCore.QRectF(bar_x, bar_y, bar_w, bar_h), 4, 4)

            # 畫燈號圓點
            dot_x = bar_x + padding
            dot_y = bar_y + (bar_h - dot_size) / 2
            painter.setBrush(dot_color)
            painter.drawEllipse(QtCore.QRectF(dot_x, dot_y, dot_size, dot_size))

            # 畫文字
            painter.setPen(QtGui.QColor(255, 255, 255))
            painter.drawText(int(dot_x + dot_size + 6), int(bar_y + padding + fm.ascent()), status_line)

            painter.end()

            self.standbyLabel.setPixmap(pixmap)
        except Exception as e:
            print(f"_updateStatusOverlay error: {e}")

    def _measurePingLatency(self):
        """在背景執行緒測量 ping 延遲，更新燈號快取"""
        try:
            import subprocess
            result = subprocess.run(
                ['ping', '-c', '1', '-W', '2', '8.8.8.8'],
                capture_output=True, text=True, timeout=5
            )
            if result.returncode == 0:
                output = result.stdout
                idx = output.find('time=')
                if idx >= 0:
                    ms_str = output[idx+5:].split(' ')[0].split('\n')[0]
                    self._cachedPingMs = float(ms_str)
                    print(f"Ping latency: {self._cachedPingMs}ms")
                else:
                    self._cachedPingMs = -1
            else:
                self._cachedPingMs = -1
        except Exception as e:
            print(f"_measurePingLatency error: {e}")
            self._cachedPingMs = -1

    def _updateNetQuality(self):
        """定時觸發背景 ping 測量，並刷新狀態顯示"""
        _thread.start_new_thread(self._measurePingLatency, ())
        self._updateStatusOverlay()

    def _showExitMenu(self):
        """按 Escape 時顯示退出選單"""
        self.focusLock = True
        dialog = QtWidgets.QDialog(self, QtCore.Qt.FramelessWindowHint)
        dialog.setAttribute(QtCore.Qt.WA_DeleteOnClose)
        w = self.width()
        h = self.height()

        # 半透明黑色背景
        dialog.setStyleSheet("background-color: rgba(0,0,0,200);")
        dialog.setGeometry(0, 0, w, h)

        # 按鈕共用樣式
        btn_style = (
            "QPushButton {"
            "  background-color: rgba(60,60,60,220);"
            "  color: white; font-size: 20px; font-weight: bold;"
            "  border: 2px solid #888; border-radius: 12px;"
            "  padding: 12px;"
            "}"
            "QPushButton:pressed { background-color: rgba(100,100,100,255); }"
        )
        btn_w = int(w * 0.35)
        btn_h = int(h * 0.09)
        btn_x = int((w - btn_w) / 2)
        start_y = int(h * 0.25)
        gap = int(h * 0.13)

        # 標題
        title = QtWidgets.QLabel(dialog)
        title.setText("系統選單")
        title.setStyleSheet("color: white; font-size: 28px; font-weight: bold; background: transparent;")
        title.setAlignment(QtCore.Qt.AlignCenter)
        title.resize(btn_w, int(h * 0.08))
        title.move(btn_x, int(h * 0.12))

        # 回到桌面
        btn_desktop = QtWidgets.QPushButton("回到桌面", dialog)
        btn_desktop.setStyleSheet(btn_style)
        btn_desktop.resize(btn_w, btn_h)
        btn_desktop.move(btn_x, start_y)
        btn_desktop.clicked.connect(lambda: self._exitAction('desktop', dialog))

        # 重開機
        btn_reboot = QtWidgets.QPushButton("重新開機", dialog)
        btn_reboot.setStyleSheet(btn_style)
        btn_reboot.resize(btn_w, btn_h)
        btn_reboot.move(btn_x, start_y + gap)
        btn_reboot.clicked.connect(lambda: self._exitAction('reboot', dialog))

        # 關機
        btn_shutdown = QtWidgets.QPushButton("關機", dialog)
        btn_shutdown.setStyleSheet(btn_style)
        btn_shutdown.resize(btn_w, btn_h)
        btn_shutdown.move(btn_x, start_y + gap * 2)
        btn_shutdown.clicked.connect(lambda: self._exitAction('shutdown', dialog))

        # 取消
        btn_cancel = QtWidgets.QPushButton("取消", dialog)
        btn_cancel.setStyleSheet(
            "QPushButton {"
            "  background-color: rgba(40,40,40,180);"
            "  color: #aaa; font-size: 18px;"
            "  border: 1px solid #555; border-radius: 10px;"
            "  padding: 10px;"
            "}"
            "QPushButton:pressed { background-color: rgba(80,80,80,255); }"
        )
        btn_cancel.resize(btn_w, int(btn_h * 0.8))
        btn_cancel.move(btn_x, start_y + gap * 3)
        btn_cancel.clicked.connect(lambda: self._exitAction('cancel', dialog))

        dialog.exec_()
        self.focusLock = False

    def _exitAction(self, action, dialog):
        dialog.close()
        if action == 'desktop':
            print("Exit to desktop, restoring panel")
            subprocess.Popen(['wf-panel-pi'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            from PyQt5.QtWidgets import QApplication
            QApplication.quit()
        elif action == 'reboot':
            print("Rebooting...")
            os.system("sudo reboot")
        elif action == 'shutdown':
            print("Shutting down...")
            os.system("sudo shutdown -h now")
        # cancel: do nothing, return to standby

    def deleteEmpty(self, IP):
        result = ''
        for i in IP:
            if(i>='0' and i<='9') or i == '.':
                result += i
        return result

    def getIP(self):
        p=os.popen('ifconfig')
        data = p.readlines()
        eth0StartPoint = self.findWifiIndex(data, 0, 'eth0')
        wlan0StartPoint = self.findWifiIndex(data, 0, 'wlan0')
        if data[wlan0StartPoint+1].find('inet ') >=0:
            tmpStr = data[wlan0StartPoint+1]
            inetIndex = tmpStr.find('inet ')+len('inet ')
            endIndex = tmpStr.find(' ', inetIndex)
            return self.deleteEmpty(tmpStr[inetIndex:endIndex])
        if data[eth0StartPoint+1].find('inet ')>=0:
            tmpStr = data[eth0StartPoint+1]
            inetIndex = tmpStr.find('inet ')+len('inet ')
            endIndex = tmpStr.find(' ', inetIndex)
            return self.deleteEmpty(tmpStr[inetIndex:endIndex])

    def enforceFocus(self):
        if not self.focusLock:
            self.raise_()
            self.activateWindow()
            self.setFocus()

    def forceInitialFocus(self):
        self.raise_()
        self.activateWindow()
        self.setFocus()


    def get_ip(self):
        try:
            # 嘗試透過連線外部地址來獲取本地IP（不會實際傳資料出去）
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect(('8.8.8.8', 80))  # Google DNS，只用來取得本地網卡的IP
            ip = s.getsockname()[0]
            s.close()
            return ip
        except Exception as e:
            print("Failed to get IP:", e)
            return None
        


    def getAnyDeskID(self):
        try:
            p = os.popen('anydesk --get-id')
            data = p.read().strip()
            if data:
                return self.deleteEmpty(data)
            else:
                return "No ID"
        except Exception as e:
            print(f"⚠️ Failed to get AnyDesk ID: {e}")
            return "Unavailable"

            
        
        

    def findWifiIndex(self, data, startPoint, matchString):
        for i in range(len(data)):
            tmp = data[i]
##            print(tmp)
            if tmp.find(matchString)>=0 and i>=startPoint:
                return i
        return -1
        

    def timeoutFunc(self):
        print("Feed Back Status)!")
        self.Log.i("MediaPlayer_timeoutFunc============Feed Back Status!")
##        httpR = HttpCmdLibrary.build('12345678', self.DeviceName, self.Log)
##        httpR.SendPingCmd()
        #httpR.showPingMessage()
        self.start_ping_timer(self.PingTimer)

    def start_ping_timer(self, second):
        if self.timeout:
            self.timeout.stop()
            self.timeout.deleteLater()
        self.timeout = QtCore.QTimer()
        self.timeout.timeout.connect(self.timeoutFunc)
        self.timeout.setSingleShot(True) 
        self.timeout.start(int(1000 * second))


    def fileGetContents(self, filename):
        with open(filename) as f:
            content = f.readline()
            content = content.strip()
            print("Device Name): "+ content)
            self.Log.i("MediaPlayer_fileGetContents============Device Name: "+ content)
            return content

    def fileGetPingTimerContents(self, filename):
        with open(filename) as f:
            content = f.readline()
            content = content.strip()
            print("Ping Timer): "+ content)
            self.Log.i("MediaPlayer_fileGetPingTimerContents============Ping Timer: "+ content)
            return content
        


        
            
    def DoNext(self):
        global GlobalPlayItem
        global GlobalPlayItemCounter
        global mediaLocation
        global GlobalUSBFoler
        GlobalPlayItemCounter = GlobalPlayItemCounter + 1
        
        if(GlobalPlayItemCounter >= len(GlobalPlayItem)):
            GlobalPlayItemCounter = 0
        print("GlobalPlayItemCounter:"+str(GlobalPlayItemCounter))
        print("GlobalPlayItem len:"+str(len(GlobalPlayItem)))
        mediaLocation = GlobalUSBFoler + GlobalPlayItem[GlobalPlayItemCounter]
        print("DoNext:"+mediaLocation)


    
  
    def keyPressEvent(self, event):  #MediaPlayer Keydown
        print(f"MediaPlay, key down: {event.key()} text='{event.text()}'")
        global MediaStatus

        if event.key() == Qt.Key_Space and MediaStatus is True:
            print("pause")
            MediaStatus = False
        elif event.key() == Qt.Key_Space and MediaStatus is False:
            print("play")
            MediaStatus = True
            self.showStandbyImage()
        elif event.key() == Qt.Key_Escape:
            self._showExitMenu()
        else:
            # === 掃碼器緩衝機制 ===
            # 掃碼器快速送出: [前綴符號] + 數字 + [Enter/Tab]
            # 先緩衝所有按鍵，等結束符號或 timeout 後一次處理
            if not hasattr(self, '_scanBuffer'):
                self._scanBuffer = ''
                self._scanBufTimer = None

            EK = event.key()
            is_terminator = EK in (Qt.Key_Return, Qt.Key_Enter, Qt.Key_Tab)
            is_trigger = EK == 16777248 or EK == Qt.Key_Percent or event.text() == '%'
            is_digit = event.text().isdigit()

            if is_digit:
                # 數字 → 加入緩衝
                self._scanBuffer += event.text()
                self._resetScanBufTimer()
                print(f"  buffer: '{self._scanBuffer}'")
            elif is_terminator:
                # 結束符號 → 立即處理緩衝
                print(f"  terminator, buffer: '{self._scanBuffer}'")
                self._processScanBuffer(auto_submit=True)
            elif is_trigger:
                # 前綴觸發符號（%, Shift）→ 開始緩衝，不加入內容
                print(f"  trigger key, start buffering")
                self._resetScanBufTimer()
            else:
                # 其他按鍵 → 如果有字元就加入緩衝
                if event.text():
                    self._scanBuffer += event.text()
                    self._resetScanBufTimer()

    def _resetScanBufTimer(self):
        """重設掃碼緩衝 timer（500ms 沒有新按鍵就處理）"""
        if hasattr(self, '_scanBufTimer') and self._scanBufTimer:
            self._scanBufTimer.stop()
            self._scanBufTimer.deleteLater()
        self._scanBufTimer = QtCore.QTimer()
        self._scanBufTimer.setSingleShot(True)
        self._scanBufTimer.timeout.connect(lambda: self._processScanBuffer(auto_submit=True))
        self._scanBufTimer.start(500)

    def _processScanBuffer(self, auto_submit=False):
        """處理緩衝的掃碼內容"""
        if hasattr(self, '_scanBufTimer') and self._scanBufTimer:
            self._scanBufTimer.stop()
        code = self._scanBuffer.strip()
        self._scanBuffer = ''
        print(f"_processScanBuffer: code='{code}', auto_submit={auto_submit}")
        self.startQRCodeDailog(code, auto_submit)

    def startQRCodeDailog(self, initial_text='', auto_submit=False):
        global MediaStatus
        MediaStatus=False

        self.focusLock = True  # 🔒 停止主畫面搶焦點

        try:
            print(f"startQRCodeDailog: initial_text='{initial_text}', auto_submit={auto_submit}")
            httpR = HttpCmdLibrary.build('', self.DeviceName, self.Log)

            if auto_submit and initial_text:
                print(f"  直接送出交易碼: '{initial_text}'")
                self.Log.i(f"QRScan auto-submit, code: {initial_text}")
                httpR.setTransaction_code(initial_text)
                httpR.SendTakeBallCmd()
                httpR.showTakeBallMessage()
            else:
                TakeBalldialog = TakeBallDialog(self, httpR, self.Log)
                if initial_text:
                    TakeBalldialog.txtTransaction_value.setText(initial_text)
                TakeBalldialog.exec_()
            if(httpR.error_code):
                if(httpR.error_description != "NoWireless"):
                    ErrorTakeBalldialog = ErrorDialog(self, httpR.error_description, httpR.transaction_code, self.Log)
                    ErrorTakeBalldialog.exec_()
            elif(httpR.ProcessTag == "TakeBall"):
                TakeBallConfirmdialog = TakeBallConfirmDialog(self, httpR, self.Log, 2)
                TakeBallConfirmdialog.exec_()
                if(httpR.error_code):
                    print("TakeBallConfirmdialog, httpR:ErrorDialog")
                    ErrorTakeBallConfirmdialog = ErrorDialog(self, httpR.error_description, httpR.transaction_code, self.Log)
                    ErrorTakeBallConfirmdialog.exec_()
                elif(httpR.ProcessTag == "TakeBallConfirm"):
                    print("TakeBallConfirmdialog, httpR:Successdialog")
                    Successdialog = SuccessDialog(self, httpR, self.Log, self.Control)
                    Successdialog.exec_()
        except Exception as e:
            print(f"startQRCodeDailog 發生異常: {e}")
            self.Log.e(f"startQRCodeDailog 異常: {e}")
            try:
                errdlg = ErrorDialog(self, "系統異常", str(e)[:20], self.Log)
                errdlg.exec_()
            except:
                pass

        print("startQRCodeDailog============endTrade")
        self.focusLock = False  # ✅ 解鎖，恢復強制前景
        self.showStandbyImage()
        MediaStatus = True


    def startDailog(self):
        global MediaStatus
        MediaStatus=False

        self.focusLock = True  # 🔒 停止主畫面搶焦點
        print("MediaPlayer_startDailog============startTrade")
        httpR = HttpCmdLibrary.build('', self.DeviceName, self.Log)
        TakeBalldialog = TakeBallDialog(self, httpR, self.Log)
        TakeBalldialog.exec_()
        if(httpR.error_code):
            if(httpR.error_description != "NoWireless"):
                ErrorTakeBalldialog = ErrorDialog(self, httpR.error_description, httpR.transaction_code, self.Log)
                ErrorTakeBalldialog.exec_()
        elif(httpR.ProcessTag == "TakeBall"):
            TakeBallConfirmdialog = TakeBallConfirmDialog(self, httpR, self.Log)
            TakeBallConfirmdialog.exec_()
            if(httpR.error_code):
                ErrorTakeBallConfirmdialog = ErrorDialog(self, httpR.error_description, httpR.transaction_code, self.Log)
                ErrorTakeBallConfirmdialog.exec_()
            elif(httpR.ProcessTag == "TakeBallConfirm"):
                Successdialog = SuccessDialog(self, httpR, self.Log, self.Control)
                Successdialog.exec_()
        print("MediaPlayer_startDailog============endTrade")

        self.focusLock = False  # ✅ 解鎖，恢復強制前景
        self.showStandbyImage()
        MediaStatus = True

    def resizeEvent(self, event):
        if hasattr(self, 'standbyLabel'):
            self.standbyLabel.resize(self.size())
            self.showStandbyImage()


    def showStandbyImage(self):
        # 直接呼叫 overlay 方法，它會載入 standby.jpg 並疊加狀態文字
        if hasattr(self, '_cachedNetQuality'):
            self._updateStatusOverlay()
        else:
            standby_path = os.path.join(BASE_PATH, "standby.jpg")
            if os.path.exists(standby_path):
                pixmap = QtGui.QPixmap(standby_path).scaled(
                    self.size(), QtCore.Qt.KeepAspectRatio, QtCore.Qt.SmoothTransformation
                )
                self.standbyLabel.setPixmap(pixmap)
    
        

    def controlPlayer(self,type):
        if self.process is not None and self.process.poll() is None:
            self.process.stdin.write(self.commands[type])

    def updateClock(self):

        global GlobalVideoCounter
        GlobalVideoCounter = GlobalVideoCounter + 1
        if(GlobalVideoCounter+1 == self.duration):
            print("updateClock")
            print("play finish!")
            print("stop")
            try:
                httpR = HttpCmdLibrary.build('12345678', self.DeviceName, self.Log)
                tmpIp = self.get_ip()
                
                
                print('Current IP:{}'.format(tmpIp))
                httpR.SendPingCmd(tmpIp + '_' + appVersion)
                
            except:
                print('ping error')
            MediaStatus=False
            self.DoNext()
            self.showStandbyImage()

        try:
            if self.process is not None and self.process.poll() is None:
                timepos = subprocess.check_output([os.path.abspath('dbuscontrol.sh'),'pos'])
                if timepos=='':
                    return
                self.timeElapsed = (int(timepos.strip().split(' ')[1])/1000000)
                progress=10+(self.progressBar.size().width()-self.progressCircle.size().width())*self.timeElapsed/self.duration
                self.progressCircle.move(int(progress),int(self.gap/2-self.progressBarHeight/2))
                self.progressText.setText(str(self.getClockString(self.timeElapsed))+
                    str(self.progressText.text())[8:])
                print(self.progressText.getText())
        except:
# print("error")
            nstate = 0


    def getClockString(self,n):
        return str(n/3600).zfill(2)+':'+str((n%3600)/60).zfill(2)+':'+str(n%60).zfill(2)


    def chooseSettings(self,choices):
        global settings
        self.buttons[8].setPixmap(self.buttonPressedPic)
        self.app.processEvents()
        for k in choices:
            v,ok=QtGui.QInputDialog.getItem(self,'',
                k+' (Currently '+settings[k]+')',choices[k],0,False)
            if ok:
                settings[k]=str(v)
        self.buttons[8].setPixmap(self.buttonDepressedPic)

    def saveSettings(self):
        settingsString="settings={"
        for k in settings:
            settingsString+="'"+k+"':'"+settings[k]+"',"
        settingsString+='}\n'
        mediaLocationString="mediaLocation='"+mediaLocation+"'\n"
        f=open('MediaPlayer.py','r')
        buffr=''
        for line in f:
            if line.startswith('settings={'):
                buffr+=settingsString
            elif line.startswith('mediaLocation='):
                buffr+=mediaLocationString
            else:
                buffr+=line
        f.close()
        f=open('MediaPlayer.py','w')
        f.write(buffr)
        f.close()



    def closeEvent(self,e):
        self.saveSettings()
        if self.process is not None and self.process.poll() is None:
            self.process.stdin.write('q')
            self.process.terminate()
        e.accept()

class TakeBallDialog(QtWidgets.QDialog):
    def __init__(self, parent, httpR, tmplog):
        QtWidgets.QDialog.__init__(self, parent, QtCore.Qt.FramelessWindowHint)
        self.setAttribute(QtCore.Qt.WA_DeleteOnClose)
        self.setCursor(QtGui.QCursor(QtCore.Qt.BlankCursor))

        self.isConnect = self.checkWifi()
        print('System connection status: ', self.isConnect)

        # === 使用父視窗大小（自適應不同螢幕）===
        w = parent.width()
        h = parent.height()
        self.setGeometry(0, 0, w, h)

        ## === 設定背景圖根據連線狀態 ===
        # palette = QtGui.QPalette()
        # if self.isConnect:
        #     palette.setBrush(QtGui.QPalette.Background, QtGui.QBrush(QtGui.QPixmap("image/2_1wireless.png")))
        # else:
        #     palette.setBrush(QtGui.QPalette.Background, QtGui.QBrush(QtGui.QPixmap("image/2_1NoWireless.png")))
        # self.setPalette(palette)

        print("關閉 IME 輸入法功能")
        self.setAttribute(QtCore.Qt.WA_InputMethodEnabled, False)
        # ImhPreferLatin：偏好英文（Latin）輸入
        # ImhHiddenText：隱藏輸入文字（像密碼輸入框用）
        print("ImhPreferLatin：偏好英文（Latin）輸入")
        self.setInputMethodHints(QtCore.Qt.ImhPreferLatin | QtCore.Qt.ImhHiddenText)


        # === 建立全螢幕 QLabel 當作背景圖層 ===
        self.bgLabel = QtWidgets.QLabel(self)
        bg_path = "image/2_1wireless.png" if self.isConnect else "image/2_1NoWireless.png"
        # pixmap = QtGui.QPixmap(bg_path).scaled(
        #     w, h, QtCore.Qt.KeepAspectRatioByExpanding, QtCore.Qt.SmoothTransformation
        # )
        pixmap = QtGui.QPixmap(bg_path).scaled(
            w, h, QtCore.Qt.IgnoreAspectRatio, QtCore.Qt.FastTransformation
        )
        self.bgLabel.setPixmap(pixmap)
        self.bgLabel.resize(w, h)
        self.bgLabel.move(0, 0)
        self.bgLabel.lower()  # 確保圖片不會蓋住前景元件


        self.Log = tmplog
        self.Log.i("TakeBallDialog_Initial")
        self.setWindowTitle("")

        # === 建立顯示文字用的 Label ===
        brush = QtGui.QBrush(QtGui.QColor(255, 153, 71))
        brush.setStyle(QtCore.Qt.SolidPattern)

        palette = QtGui.QPalette()
        palette.setBrush(QtGui.QPalette.Active, QtGui.QPalette.WindowText, brush)

        font = QtGui.QFont()
        font.setPointSize(int(h * 0.046))  # 約等於 1080 下的 50px 字
        font.setBold(True)

        self.txtTransaction_value = QtWidgets.QLabel(self)
        self.txtTransaction_value.setText("")
        self.txtTransaction_value.setPalette(palette)
        self.txtTransaction_value.move(int(w * 0.36), int(h * 0.46))  # 原本 660,500 的比例位置
        self.txtTransaction_value.resize(int(w * 0.27), int(h * 0.065))  # 原本 490x70 大小
        self.txtTransaction_value.setFont(font)
        self.txtTransaction_value.setAlignment(QtCore.Qt.AlignCenter)

        print(f"TakeBallDialog : size={w}x{h}")

        self.tmpHttpR = httpR
        self.Lock = False
        self.timeoutFlag = False
        self.timeout = None

        if self.isConnect:
            self.start_timer(10)    #show 有網路等候輸入畫面 7秒
        else:
            self.start_timer2(27)    #show no wireless畫面 7秒

        #保證能取得焦點    
        self.setFocusPolicy(QtCore.Qt.StrongFocus)
        self.setFocus()



    
    def checkWifi(self):
        import platform
        import subprocess
        import socket

        try:
            # Try socket connection first (universal method)
            socket.create_connection(("8.8.8.8", 53), timeout=3)
            print("✅ Network reachable via socket")
            return True
        except Exception as e:
            print("⚠️ Socket connection failed:", e)

        # macOS fallback: use system command
        if platform.system() == "Darwin":
            try:
                output = subprocess.check_output(['ping', '-c', '1', '8.8.8.8'], stderr=subprocess.DEVNULL)
                print("✅ Network reachable via ping")
                return True
            except subprocess.CalledProcessError:
                print("❌ Ping failed")
                return False

        return False


    def findWifiIndex(self, data, startPoint, matchString):
        for i in range(len(data)):
            tmp = data[i]
    ##            print(tmp)
            if tmp.find(matchString)>=0 and i>=startPoint:
                return i
        return -1


    def keyPressEvent(self, event):
        print("MimeDialog,  key down")
        print(event.key())
        self.timeoutFlag = False
        EK = event.key()

        if self.txtTransaction_value.text() == "7777777":
            print("Shutdown")
            import os
            os.system("sudo shutdown -h now")

        if self.isConnect:
            key_to_digit = {
                Qt.Key_0: "0",
                Qt.Key_1: "1",
                Qt.Key_2: "2",
                Qt.Key_3: "3",
                Qt.Key_4: "4",
                Qt.Key_5: "5",
                Qt.Key_6: "6",
                Qt.Key_7: "7",
                Qt.Key_8: "8",
                Qt.Key_9: "9",
            }

            if EK in key_to_digit:
                self.txtTransaction_value.setText(self.txtTransaction_value.text() + key_to_digit[EK])
            elif EK in (Qt.Key_Return, Qt.Key_Enter,Qt.Key_Tab):
                print("SendHttpTakeBallRequest")
                self.SendHttpRequest()
    
            elif EK == Qt.Key_Backspace:
                print("delete")
                tmpString = self.txtTransaction_value.text()
                self.txtTransaction_value.setText(tmpString[:-1])

        if EK == Qt.Key_Escape:
            self.showMinimized()
            print("close application")
            import sys
            from PyQt5.QtWidgets import QApplication
            print("Escape pressed, exiting program.")
            QApplication.quit()
            # self.CloseDialog()


    def SendHttpRequest(self):
        print("self.Lock: " + str(self.Lock))  # ✅ 修正型
        if(self.Lock==False):
            self.Lock = True
            self.Log.i("TakeBallDialog_SendHttpRequest,    transaction is:"+str(self.txtTransaction_value.text()))
            print(")================SendTakeBallCmd====================")
            self.tmpHttpR.setTransaction_code(str(self.txtTransaction_value.text()))
            self.tmpHttpR.SendTakeBallCmd()
            self.tmpHttpR.showTakeBallMessage()
            self.CloseDialog()


    def handleButtonClose(self):
        print("handleButtonClose")
        self.CloseDialog()

    def timeoutFunc(self):
        print("TakeBallDialog timeout notice)!")
        if(self.timeoutFlag):
            self.tmpHttpR.error_description = "輸入超時"
            self.CloseDialog()
            self.Log.i("TakeBallDialog_timeoutFunc_CloseDialog")
        else:
            self.timeoutFlag = True
            self.start_timer(5)

    def timeoutFunc2(self):
        print("TakeBallDialog NoWireless)!")
        if(self.timeoutFlag):
            self.tmpHttpR.error_description = "NoWireless"
            self.CloseDialog()
            self.Log.i("TakeBallDialog_NoWireless")
        else:
            self.timeoutFlag = True
            self.start_timer2(5)

    def CloseDialog(self):
        if self.timeout:
            self.timeout.stop()
            self.timeout.deleteLater()
        self.close()
        
    def start_timer(self, second):
        if self.timeout:
            self.timeout.stop()
            self.timeout.deleteLater()
        self.timeout = QtCore.QTimer()
        self.timeout.timeout.connect(self.timeoutFunc)
        self.timeout.setSingleShot(True) 
        self.timeout.start(int(1000 * second))

        
    def start_timer2(self, second):
        if self.timeout:
            self.timeout.stop()
            self.timeout.deleteLater()
        self.timeout = QtCore.QTimer()
        self.timeout.timeout.connect(self.timeoutFunc2)
        self.timeout.setSingleShot(True) 
        self.timeout.start(int(1000 * second))

        


class TakeBallConfirmDialog(QtWidgets.QDialog):
    def __init__(self, parent, httpR, tmplog, autoconfirm_second=0):
        # 建立無邊框全螢幕對話框
        QtWidgets.QDialog.__init__(self, parent, QtCore.Qt.FramelessWindowHint)
        self.setAttribute(QtCore.Qt.WA_DeleteOnClose)
        # 不隱藏游標，讓觸控螢幕的取消按鈕可以正常使用

        # === 使用父視窗大小（自適應不同螢幕）===
        w = parent.width()
        h = parent.height()
        self.setGeometry(0, 0, w, h)
        print(f"TakeBallConfirmDialog size = {w}x{h}")


        # === 設定背景圖片，使用 QLabel 方式避免圖片超框 ===
        self.bgLabel = QtWidgets.QLabel(self)
        bg_path = "image/2_3.jpg"
        pixmap = QtGui.QPixmap(bg_path).scaled(
            w, h,
            QtCore.Qt.IgnoreAspectRatio,    # KeepAspectRatioByExpanding,   # 或改成 Qt.KeepAspectRatio 視需求
            QtCore.Qt.FastTransformation    # SmoothTransformation
        )
        self.bgLabel.setPixmap(pixmap)
        self.bgLabel.resize(w, h)
        self.bgLabel.move(0, 0)
        self.bgLabel.lower()  # 放在最下層，不擋其他元件

        # === 字型設定 ===
        font = QtGui.QFont()
        font.setPointSize(int(h * 0.046))  # 根據螢幕高度調整字體大小
        font.setBold(True)

        # === 紀錄 Log 資訊 ===
        self.Log = tmplog
        self.Log.i("TakeBallConfirmDialog_Initial,    " +
                   "交易號碼: " + httpR.response_id +
                   ", 取球數量: " + str(httpR.response_take) +
                   ", 剩餘球數: " + str(httpR.response_ball_before))

        # === 顯示取球數量（白色字）===
        txtTake_palette = QtGui.QPalette()
        txtTake_palette.setColor(QtGui.QPalette.WindowText, QtGui.QColor(255, 255, 255))

        self.txtTake = QtWidgets.QLabel(self)
        self.txtTake.setText(str(httpR.response_take))
        self.txtTake.setPalette(txtTake_palette)
        self.txtTake.move(int(w * 0.39), int(h * 0.32))   # 原本 750/1920, 350/1080 的位置
        self.txtTake.resize(int(w * 0.25), int(h * 0.065))
        self.txtTake.setFont(font)
        self.txtTake.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)

        # === 顯示扣球數量（橘色字）===
        txtBall_palette = QtGui.QPalette()
        txtBall_palette.setColor(QtGui.QPalette.WindowText, QtGui.QColor(255, 153, 71))

        self.txtBall = QtWidgets.QLabel(self)
        self.txtBall.setText(str(httpR.response_use))
        self.txtBall.setPalette(txtBall_palette)
        self.txtBall.move(int(w * 0.39), int(h * 0.48))  # 原本 750/1920, 520/1080
        self.txtBall.resize(int(w * 0.25), int(h * 0.065))
        self.txtBall.setFont(font)
        self.txtBall.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)

        # === 取消按鈕（觸控螢幕用）===
        self.btnCancel = QtWidgets.QPushButton("取消", self)
        cancel_font = QtGui.QFont()
        cancel_font.setPointSize(int(h * 0.035))
        cancel_font.setBold(True)
        self.btnCancel.setFont(cancel_font)
        self.btnCancel.setStyleSheet(
            "QPushButton {"
            "  background-color: rgba(200, 50, 50, 200);"
            "  color: white;"
            "  border: 2px solid white;"
            "  border-radius: 10px;"
            "  padding: 10px;"
            "}"
            "QPushButton:pressed {"
            "  background-color: rgba(255, 80, 80, 255);"
            "}"
        )
        btn_w = int(w * 0.25)
        btn_h = int(h * 0.10)
        self.btnCancel.resize(btn_w, btn_h)
        self.btnCancel.move(int((w - btn_w) / 2), int(h * 0.90))
        self.btnCancel.setCursor(QtGui.QCursor(QtCore.Qt.PointingHandCursor))
        self.btnCancel.raise_()
        self.btnCancel.clicked.connect(self.handleCancel)



        # === 控制邏輯 ===
        self.tmpHttpR = httpR
        self.Lock = False
        self.timeout = None

        # === 自動確認機制（預設為等待 2 秒自動執行）===
        if autoconfirm_second > 0:
            QtCore.QTimer.singleShot(autoconfirm_second * 1000, self.autoConfirm)
        else:
            self.start_timer(10)  # 如果未設定則等 10 秒


    def autoConfirm(self):
        self.ConfirmOk()

    def handleCancel(self):
        print("TakeBallConfirmDialog: cancel button pressed")
        self.Log.i("TakeBallConfirmDialog_Cancel by touch button")
        self.CloseDialog()

    def keyPressEvent(self, event):
        print("MimeDialog,  key down")
        print(event.key())
        self.timeoutFlag = False
        EK = event.key()
        if EK == Qt.Key_0:
            self.CloseDialog()
        elif EK in (Qt.Key_Return, Qt.Key_Enter):  # 同時攔截主鍵盤與數字區的 Enter
            self.ConfirmOk()
        elif EK == Qt.Key_Escape:
            self.CloseDialog()
        # if(EK==48):
        #     self.CloseDialog()
        # elif(EK==16777221):
        #     self.ConfirmOk()
        # elif(EK == 16777216):
        #     self.CloseDialog()

    def ConfirmOk(self):
        if(self.Lock==False):
            self.Lock = True
            self.tmpHttpR.SendTakeBallConfirmCmd('takeBall'+str(self.tmpHttpR.response_take), '1')
            self.tmpHttpR.showTakeBallConfirmMessage()
            self.CloseDialog()

    def timeoutFunc(self):
        print("TakeBallConfirmDialog timeout)!")
        self.Log.i("TakeBallDialog_timeoutFunc")
        self.tmpHttpR.error_description = "TimeOut"
        self.CloseDialog()

    def CloseDialog(self):
        if self.timeout:
            self.timeout.stop()
            self.timeout.deleteLater()
        self.close()

    def start_timer(self, second):
        if self.timeout:
            self.timeout.stop()
            self.timeout.deleteLater()
        self.timeout = QtCore.QTimer()
        self.timeout.timeout.connect(self.timeoutFunc)
        self.timeout.setSingleShot(True) 
        self.timeout.start(int(1000 * second))  # ← 修正這裡

class ErrorDialog(QtWidgets.QDialog):
    def __init__(self, parent, ErrorTitle, ErrorMsg, tmplog):
        # === 初始化 Dialog 視窗，設定為無邊框全螢幕 ===
        QtWidgets.QDialog.__init__(self, parent, QtCore.Qt.FramelessWindowHint)
        self.setAttribute(QtCore.Qt.WA_DeleteOnClose)
        self.setCursor(QtGui.QCursor(QtCore.Qt.BlankCursor))

        # === 使用父視窗大小（自適應不同螢幕）===
        w = parent.width()
        h = parent.height()
        self.setGeometry(0, 0, w, h)
        print(f"ErrorDialog size = {w}x{h}")

        # === 設定背景圖片，使用 QLabel 方式避免超框 ===
        self.bgLabel = QtWidgets.QLabel(self)
        bg_path = "image/2_4.jpg"
        pixmap = QtGui.QPixmap(bg_path).scaled(
            w, h,
            QtCore.Qt.IgnoreAspectRatio,      # 圖片強制縮放填滿整個螢幕，會變形但不裁切
            QtCore.Qt.FastTransformation      # 較快速度的縮放（畫質略低但足夠）
        )
        self.bgLabel.setPixmap(pixmap)
        self.bgLabel.resize(w, h)
        self.bgLabel.move(0, 0)
        self.bgLabel.lower()  # 確保圖片顯示在最底層

        # === 設定字型大小（根據螢幕解析度縮放）===
        code_font = QtGui.QFont()
        code_font.setPointSize(int(h * 0.046))  # 原本約 50pt
        code_font.setBold(True)

        value_font = QtGui.QFont()
        value_font.setPointSize(int(h * 0.074))  # 原本約 80pt
        value_font.setBold(True)

        # === Log 錯誤紀錄（標題與訊息）===
        self.Log = tmplog
        self.Log.e(f"ErrorDialog_Initial,    Title: {ErrorTitle}, Msg: {ErrorMsg}")

        # === 錯誤訊息（橘色）Label ===
        value_palette = QtGui.QPalette()
        value_palette.setColor(QtGui.QPalette.WindowText, QtGui.QColor(255, 153, 71))

        self.txtTransaction_value = QtWidgets.QLabel(self)
        self.txtTransaction_value.setText(ErrorMsg)
        self.txtTransaction_value.setPalette(value_palette)
        self.txtTransaction_value.move(int(w * 0.10), int(h * 0.46))
        self.txtTransaction_value.resize(int(w * 0.80), int(h * 0.092))
        self.txtTransaction_value.setFont(value_font)
        self.txtTransaction_value.setAlignment(QtCore.Qt.AlignCenter)

        # === 錯誤標題（白色）Label ===
        code_palette = QtGui.QPalette()
        code_palette.setColor(QtGui.QPalette.WindowText, QtGui.QColor(255, 255, 255))

        self.txtTransaction_code = QtWidgets.QLabel(self)
        self.txtTransaction_code.setText(ErrorTitle)
        self.txtTransaction_code.setPalette(code_palette)
        self.txtTransaction_code.move(int(w * 0.10), int(h * 0.027))
        self.txtTransaction_code.resize(int(w * 0.80), int(h * 0.065))
        self.txtTransaction_code.setFont(code_font)
        self.txtTransaction_code.setAlignment(QtCore.Qt.AlignCenter)

        # === 自動關閉時間（預設 5 秒）===
        self.timeout = None
        self.start_timer(5)

    def keyPressEvent(self, event):
        print("ErrorDialog, close dialog")
        self.CloseDialog()

    def timeoutFunc(self):
        self.CloseDialog()

    def CloseDialog(self):
        if self.timeout:
            self.timeout.stop()
            self.timeout.deleteLater()
        self.close()

    def start_timer(self, second):
        if self.timeout:
            self.timeout.stop()
            self.timeout.deleteLater()
        self.timeout = QtCore.QTimer()
        self.timeout.timeout.connect(self.timeoutFunc)
        self.timeout.setSingleShot(True)
        self.timeout.start(int(1000 * second))



# class SuccessDialog(QtWidgets.QDialog):
#     def __init__(self, parent, httpR, tmplog, tmpBallControl):
#         # 初始化無邊框全螢幕 Dialog
#         QtWidgets.QDialog.__init__(self, parent, QtCore.Qt.FramelessWindowHint)
#         self.setAttribute(QtCore.Qt.WA_DeleteOnClose)
#         self.setCursor(QtGui.QCursor(QtCore.Qt.BlankCursor))
#         self.setWindowTitle("")

class SuccessDialog(QtWidgets.QDialog):
    def __init__(self, parent, httpR, tmplog, tmpBallControl):
        super().__init__(parent, flags=QtCore.Qt.FramelessWindowHint)
        self.setAttribute(QtCore.Qt.WA_DeleteOnClose)
        self.setCursor(QtGui.QCursor(QtCore.Qt.BlankCursor))

        # === 使用父視窗大小（自適應不同螢幕）===
        w = parent.width()
        h = parent.height()
        self.setGeometry(0, 0, w, h)
        print(f"SuccessDialog size = {w}x{h}")

        print("SuccessDialog 設定背景圖片")

        # === 設定背景圖片（使用 QLabel 替代 setPalette，避免圖片出框） ===
        self.bgLabel = QtWidgets.QLabel(self)
        bg_path = "image/3_1.jpg"
        pixmap = QtGui.QPixmap(bg_path).scaled(
            w, h,
            QtCore.Qt.IgnoreAspectRatio,       # 或使用 KeepAspectRatio/ByExpanding 依你需求
            QtCore.Qt.FastTransformation
        )
        self.bgLabel.setPixmap(pixmap)
        self.bgLabel.resize(w, h)
        self.bgLabel.move(0, 0)
        self.bgLabel.lower()  # 確保背景圖在最底層

        # === 字型設定 ===
        font = QtGui.QFont()
        font.setPointSize(int(h * 0.028))  # 約等於原本 30pt
        font.setBold(True)

        txtTake_font = QtGui.QFont()
        txtTake_font.setPointSize(int(h * 0.185))  # 約 200pt（顯示大字數字）
        txtTake_font.setBold(True)

        # === 記錄交易成功事件 ===
        self.TotalTime = 5 - httpR.response_timer
        self.Log = tmplog
        print("SuccessDialog_Initial, 交易號碼: " +
                   httpR.response_id + ", 剩餘球數: " + str(httpR.response_ball_after))

        # === 顯示：取球數量（大字、白色）===
        take_palette = QtGui.QPalette()
        take_palette.setColor(QtGui.QPalette.WindowText, QtGui.QColor(255, 255, 255))
        self.txtTake = QtWidgets.QLabel(self)
        self.txtTake.setText(str(httpR.response_take))
        self.txtTake.setPalette(take_palette)
        self.txtTake.move(int(w * 0.495), int(h * 0.35))     # 約 950, 380
        self.txtTake.resize(int(w * 0.32), int(h * 0.22))    # 約 620x240
        self.txtTake.setFont(txtTake_font)
        self.txtTake.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)

        # === 顯示：剩餘球數（白色）===
        after_palette = QtGui.QPalette()
        after_palette.setColor(QtGui.QPalette.WindowText, QtGui.QColor(255, 255, 255))
        self.txtBall_after = QtWidgets.QLabel(self)
        self.txtBall_after.setText(str(httpR.response_ball_after))
        self.txtBall_after.setPalette(after_palette)
        self.txtBall_after.move(int(w * 0.14), int(h * 0.42))  # 約 270, 450
        self.txtBall_after.resize(int(w * 0.25), int(h * 0.22))
        self.txtBall_after.setFont(font)
        self.txtBall_after.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)

        # === 顯示：扣球數量（橘色）===
        diff_palette = QtGui.QPalette()
        diff_palette.setColor(QtGui.QPalette.WindowText, QtGui.QColor(255, 153, 71))
        self.txtBall_diff = QtWidgets.QLabel(self)
        self.txtBall_diff.setText(str(int(httpR.response_use)))
        self.txtBall_diff.setPalette(diff_palette)
        self.txtBall_diff.move(int(w * 0.14), int(h * 0.30))  # 約 270, 320
        self.txtBall_diff.resize(int(w * 0.25), int(h * 0.22))
        self.txtBall_diff.setFont(font)
        self.txtBall_diff.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)

        # === 顯示：原本球數（綠色）===
        before_palette = QtGui.QPalette()
        before_palette.setColor(QtGui.QPalette.WindowText, QtGui.QColor(108, 204, 108))
        self.txtBall_before = QtWidgets.QLabel(self)
        self.txtBall_before.setText(str(httpR.response_ball_before))
        self.txtBall_before.setPalette(before_palette)
        self.txtBall_before.move(int(w * 0.14), int(h * 0.21))  # 約 270, 225
        self.txtBall_before.resize(int(w * 0.25), int(h * 0.22))
        self.txtBall_before.setFont(font)
        self.txtBall_before.setAlignment(QtCore.Qt.AlignRight | QtCore.Qt.AlignVCenter)

        # === 進度條（底部，在背景文字下方）===
        bar_y = int(h * 0.88)
        bar_h_px = int(h * 0.02)
        self.progressBarBg = QtWidgets.QLabel(self)
        self.progressBarBg.setStyleSheet("background-color: rgba(0,0,0,120); border-radius: 3px;")
        self.progressBarBg.move(int(w * 0.15), bar_y)
        self.progressBarBg.resize(int(w * 0.70), bar_h_px)

        self.progressBarFill = QtWidgets.QLabel(self)
        self.progressBarFill.setStyleSheet("background-color: rgba(80,200,80,220); border-radius: 3px;")
        self.progressBarFill.move(int(w * 0.15), bar_y)
        self.progressBarFill.resize(0, bar_h_px)

        self._progressBarFullWidth = int(w * 0.70)

        # === 出盤進度文字（進度條下方）===
        progress_font = QtGui.QFont()
        progress_font.setPointSize(int(h * 0.028))
        progress_font.setBold(True)

        progress_palette = QtGui.QPalette()
        progress_palette.setColor(QtGui.QPalette.WindowText, QtGui.QColor(255, 255, 100))

        self.txtProgress = QtWidgets.QLabel(self)
        self.txtProgress.setPalette(progress_palette)
        self.txtProgress.setFont(progress_font)
        self.txtProgress.setStyleSheet("background: transparent;")
        self.txtProgress.move(int(w * 0.15), bar_y + bar_h_px + 2)
        self.txtProgress.resize(int(w * 0.70), int(h * 0.06))
        self.txtProgress.setAlignment(QtCore.Qt.AlignCenter)
        self.txtProgress.setText("")

        # === 球機控制參數初始化 ===
        self.Control = tmpBallControl
        self.Control.setSetting(httpR.response_type, httpR.response_timer)
        self.ReverseTime = self.Control.getReverseTime(httpR.response_take)
        self.timeout = None

        # === 控制流程：按 plate 模式與否執行不同的馬達控制流程 ===
        self.tmpHttpR = httpR
        if httpR.response_type == 2:  #計盤
            self.plateNum = math.ceil(float(httpR.response_take) / float(httpR.response_ball_per_plate))
            self.plateCounter = 0
            self.plateDelayDic = {
                'StopInterval': float(httpR.response_interval_timer),
                'StartInterval': float(httpR.response_start_timer)
            }
            print("SuccessDialog_plateDelayDic:"+str(self.plateDelayDic['StartInterval']))
            self.txtProgress.setText(f"出球中... 第 1 盤 / 共 {self.plateNum} 盤")
            self._updateProgressBar(0, self.plateNum)
            self.start_timer(self.plateDelayDic['StartInterval'])
            self.Control.StartMotor()
        else:
            print("SuccessDialog_ReverseTime:"+str(self.ReverseTime))
            self.start_timer(self.ReverseTime)
            self.Control.StartMotor()


        
    def CloseDialog(self):
        if self.timeout:
            self.timeout.stop()
            self.timeout.deleteLater()
        self.close()

    def start_timer(self, second):
        if self.timeout:
            self.timeout.stop()
            self.timeout.deleteLater()
        self.timeout = QtCore.QTimer()
        self.timeout.timeout.connect(self.timeoutFunc)
        self.timeout.setSingleShot(True) 

        # 強制秒數最小為 3 秒
        #delay_sec = max(second, 3.0)
        # 沒有最小秒數
        delay_sec=second
        self.timeout.start(int(1000 * delay_sec))



    def _updateProgressBar(self, completed, total):
        """更新進度條填充比例"""
        if total > 0:
            ratio = completed / total
        else:
            ratio = 0
        fill_w = int(self._progressBarFullWidth * ratio)
        self.progressBarFill.resize(fill_w, self.progressBarFill.height())

    def timeoutFunc(self):
        if self.tmpHttpR.response_type == 2:   #計盤
            if self.Control.motorFlag == 1:  #作動中 -> 關掉
                self.Control.StopMotor()
                self.plateCounter = self.plateCounter + 1
                self._updateProgressBar(self.plateCounter, self.plateNum)
                self.txtProgress.setText(
                    f"第 {self.plateCounter} 盤完成 / 共 {self.plateNum} 盤")
                print(f"Plate {self.plateCounter}/{self.plateNum} done")
                self.start_timer(self.plateDelayDic['StopInterval'])
            else:         #非作動中,檢查是否到達次數，是->關頁面，否->繼續出下一盤
                if self.plateNum == self.plateCounter:
                    self._updateProgressBar(self.plateNum, self.plateNum)
                    self.txtProgress.setText(
                        f"全部完成！共 {self.plateNum} 盤")
                    self.CloseDialog()
                else:
                    self.txtProgress.setText(
                        f"出球中... 第 {self.plateCounter + 1} 盤 / 共 {self.plateNum} 盤")
                    print(f"Starting plate {self.plateCounter + 1}/{self.plateNum}")
                    self.start_timer(self.plateDelayDic['StartInterval'])
                    self.Control.StartMotor()
        else:  #計時
            self.Control.checkIsFinish()
            self.Control.StopMotor()
            #self.CloseDialog()
            if(self.TotalTime>0.5):
                self.start_timer_after_Control(self.TotalTime)
            else: # 最少秀0.5秒
                self.start_timer_after_Control(0.5)

    def timeoutFunc_after_Control(self):
        self.CloseDialog()
        
    def start_timer_after_Control(self, second):
        if self.timeout:
            self.timeout.stop()
            self.timeout.deleteLater()
        self.timeout = QtCore.QTimer()
        self.timeout.timeout.connect(self.timeoutFunc_after_Control)
        self.timeout.setSingleShot(True) 
        self.timeout.start(int(1000 * second))



if __name__=='__main__':
    app = QtWidgets.QApplication([])
    mp = MediaPlayer(app,None)
    # mp.move(int(0),int(-90))
    mp.move(0,0)
    # mp.resize(int(1930),int(1280))
    mp.show()
    app.exec_()
