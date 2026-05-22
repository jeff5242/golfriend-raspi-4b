import os
import platform
from PyQt5 import QtCore, QtGui, QtWidgets

if platform.system() == "Darwin":
    BASE_PATH = os.path.expanduser("~/Desktop/golfsrc")
else:
    import getpass
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
        self.showStandbyImage()
        self.bgLabel.resize(self.size())

    def resizeEvent(self, event):
        self.showStandbyImage()

    def showStandbyImage(self):
        standby_path = os.path.join(BASE_PATH, "standby.jpg")
        if os.path.exists(standby_path):
            pixmap = QtGui.QPixmap(standby_path).scaled(self.size(), QtCore.Qt.KeepAspectRatio, QtCore.Qt.SmoothTransformation)
            self.bgLabel.setPixmap(pixmap)

if __name__ == '__main__':
    import sys
    app = QtWidgets.QApplication(sys.argv)
    mp = MediaPlayer(app)
    mp.show()
    sys.exit(app.exec_())