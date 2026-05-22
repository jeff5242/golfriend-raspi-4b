import os
import platform
import getpass
from PyQt5 import QtCore, QtGui, QtWidgets

if platform.system() == "Darwin":
    BASE_PATH = os.path.expanduser("~/Desktop/golfsrc")
else:
    username = getpass.getuser()
    BASE_PATH = f"/home/{username}/Desktop/golfsrc"

class MediaPlayer(QtWidgets.QWidget):
    def __init__(self, app, parent=None):
        super(MediaPlayer, self).__init__(parent)
        self.app = app
        self.setWindowTitle("Standby MediaPlayer")
        self.resize(1920, 1080)

        self.bgLabel = QtWidgets.QLabel(self)
        self.bgLabel.setAlignment(QtCore.Qt.AlignCenter)
        self.bgLabel.setScaledContents(True)
        self.bgLabel.resize(self.size())

        self.showStandbyImage()

    def resizeEvent(self, event):
        self.bgLabel.resize(self.size())
        self.showStandbyImage()

    def showStandbyImage(self):
        standby_path = os.path.join(BASE_PATH, "standby.jpg")
        if os.path.exists(standby_path):
            pixmap = QtGui.QPixmap(standby_path).scaled(
                self.size(), QtCore.Qt.KeepAspectRatio, QtCore.Qt.SmoothTransformation
            )
            self.bgLabel.setPixmap(pixmap)

    def keyPressEvent(self, event):
        print("Key pressed:", event.key())
        # 範例：按任意鍵進入領球畫面
        self.startDailog()

    def startDailog(self):
        print("▶️ Trigger TakeBall dialog")
        # 這裡可替換為實際跳出 QRCode 掃描或領球畫面
        # e.g., TakeBallDialog(self, httpR, log).exec_()

if __name__ == '__main__':
    import sys
    app = QtWidgets.QApplication(sys.argv)
    mp = MediaPlayer(app)
    mp.move(0, -90)
    mp.show()
    sys.exit(app.exec_())
