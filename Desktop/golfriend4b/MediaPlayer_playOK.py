#coding:utf-8
import hashlib, datetime, time
import requests
import json
from PyQt5 import QtCore, QtGui, QtWidgets
import subprocess
import os


import platform
if platform.system() == "Darwin":
    BASE_PATH = os.path.expanduser("~/Desktop/golfsrc")
    DEVICE_PATH = os.path.expanduser("~/Desktop")
else:
    BASE_PATH = "/home/pi/Desktop/.Project/MediaPlayer"
    DEVICE_PATH = "/home/pi/Desktop"
import _thread
import time
import HttpCmdLibrary
from log import mLog
import os
from BallControl import *

settings={'AudioMode':'hdmi','ScaleToScreen':'True',}
#settings={'AudioMode':'local','ScaleToScreen':'True',}
mediaLocation=os.path.join(BASE_PATH, "noUSBVideo/01.mp4")
MediaStatus = True
GlobalVideoCounter = 0
GlobalPlayItem = list()
GlobalPlayItemCounter = 0
GlobalUSBFoler = '/media/pi/'
GlobalNoUSBFolder = os.path.join(BASE_PATH, "noUSBVideo/")
appVersion = 'V1.6'     #2020/05/06

class MediaPlayer(QtWidgets.QWidget):
    def __init__(self,app,parent):
        QtWidgets.QWidget.__init__(self,parent)
        self.setCursor(QtGui.QCursor(QtCore.Qt.BlankCursor))
        self.app=app
        self.playerWidth=500
        self.labelHeight=100
        self.playerHeight=500
        self.iconWidth=50
        self.iconHeight=50
        self.progressBarHeight=10
        self.volumeBarWidth=15
        self.bgPic=QtGui.QPixmap(str(os.path.join(BASE_PATH, "mediaplayer/panel.png"))).scaled(QtCore.QSize(int(int(self.playerWidth)),int(self.playerHeight)))
        self.buttonDepressedPic=QtGui.QPixmap(str(os.path.join(BASE_PATH, "mediaplayer/depressed.png"))).scaled(QtCore.QSize(int(int(self.iconWidth)),int(self.iconHeight)))
        self.buttonPressedPic=QtGui.QPixmap(str(os.path.join(BASE_PATH, "mediaplayer/pressed.png"))).scaled(QtCore.QSize(int(int(self.iconWidth)),int(self.iconHeight)))
        self.iconPics=[QtGui.QPixmap(str(os.path.join(BASE_PATH, "mediaplayer/new.png"))).scaled(QtCore.QSize(int(int(self.iconWidth)),int(self.iconHeight))),
            QtGui.QPixmap(str(os.path.join(BASE_PATH, "mediaplayer/play.png"))).scaled(QtCore.QSize(int(int(self.iconWidth)),int(self.iconHeight))),
            QtGui.QPixmap(str(os.path.join(BASE_PATH, "mediaplayer/pause.png"))).scaled(QtCore.QSize(int(int(self.iconWidth)),int(self.iconHeight))),
            QtGui.QPixmap(str(os.path.join(BASE_PATH, "mediaplayer/stop.png"))).scaled(QtCore.QSize(int(int(self.iconWidth)),int(self.iconHeight))),
            QtGui.QPixmap(str(os.path.join(BASE_PATH, "mediaplayer/settings.png"))).scaled(QtCore.QSize(int(int(self.iconWidth)),int(self.iconHeight))),]
        self.progressBarPic=QtGui.QPixmap(str(os.path.join(BASE_PATH, "mediaplayer/depressed.png"))).scaled(QtCore.QSize(int(int(self.playerWidth-20-self.iconWidth*2)),int(self.progressBarHeight)))
        self.progressCirclePic=QtGui.QPixmap(str(os.path.join(BASE_PATH, "mediaplayer/circle.png"))).scaled(QtCore.QSize(int(int(self.progressBarHeight*2)),int(self.progressBarHeight*2)))
        self.setWindowTitle('OMXPlayer')
        self.process=None
        self.volume=3
        self.label=QtWidgets.QLabel(self)
        self.label.setPixmap(self.bgPic)
        self.label.resize(int(self.playerWidth),int(self.labelHeight))
        self.label.move(int(0),int(self.playerHeight))
        self.player=QtWidgets.QLabel(self)
        self.player.resize(int(self.playerWidth),int(self.playerHeight))
        self.player.setStyleSheet('background-color: black')
        self.player.move(int(0),int(0))
        self.buttons=[]
        self.volumeBars=[]
        self.volumeBarPressed=[]
        self.volumeBarDepressed=[]
        self.volumeFunctions=[]
        self.numButtons=len(self.iconPics)
        self.spacing=(self.playerWidth-self.numButtons*self.iconWidth-self.volumeBarWidth*10)/(self.numButtons+1)
        self.gap=(self.labelHeight-self.iconHeight)/2
        for i in range(0,self.numButtons):
            self.buttons.append(QtWidgets.QLabel(self.label))
            self.buttons.append(QtWidgets.QLabel(self.label))
            self.buttons[i*2].setPixmap(self.buttonDepressedPic)
            self.buttons[i*2+1].setPixmap(self.iconPics[i])
            self.buttons[i*2].resize(int(self.iconWidth),int(self.iconHeight))
            self.buttons[i*2+1].resize(int(self.iconWidth),int(self.iconHeight))
            self.buttons[i*2].move(int(int(i*self.iconWidth+(i+1))*self.spacing), int(self.gap+self.progressBarHeight))
            self.buttons[i*2+1].move(int(int(i*self.iconWidth+(i+1))*self.spacing), int(self.gap+self.progressBarHeight))
        for i in range(0,5):
            self.volumeBars.append(QtWidgets.QLabel(self.label))
            volumeBarHeight=self.iconHeight/6*(i+1)
            self.volumeBarDepressed.append(QtGui.QPixmap(str(os.path.join(BASE_PATH, "mediaplayer/depressed.png"))).scaled(QtCore.QSize(int(int(self.volumeBarWidth)),int(volumeBarHeight))))
            self.volumeBarPressed.append(QtGui.QPixmap(str(os.path.join(BASE_PATH, "mediaplayer/pressed.png"))).scaled(QtCore.QSize(int(int(self.volumeBarWidth)),int(volumeBarHeight))))
            if i<self.volume:
                self.volumeBars[i].setPixmap(self.volumeBarPressed[i])
            else:
                self.volumeBars[i].setPixmap(self.volumeBarDepressed[i])
            self.volumeBars[i].resize(int(int(self.volumeBarWidth)), int(volumeBarHeight))
            self.volumeBars[i].move(int(self.playerWidth+(2*i-10))*self.volumeBarWidth,
                    self.gap+self.progressBarHeight+self.iconHeight-volumeBarHeight)
        self.progressBar=QtWidgets.QLabel(self.label)
        self.progressBar.setPixmap(self.progressBarPic)
        self.progressBar.resize(int(self.playerWidth-20-self.iconWidth*4),int(self.progressBarHeight))
        self.progressBar.move(int(10),int(int(self.gap/2)))
        self.progressCircle=QtWidgets.QLabel(self.label)
        self.progressCircle.setPixmap(self.progressCirclePic)
        self.progressCircle.resize(int(self.progressBarHeight*2),int(self.progressBarHeight*2))
        self.progressCircle.move(int(10),int(int(self.gap/2-self.progressBarHeight/2)))
        self.progressText=QtWidgets.QLabel(self.label)
        self.progressText.setFont(QtGui.QFont('Ubuntu Mono',12))
        self.progressText.resize(int(self.iconWidth*4),int(self.progressBarHeight*2))
        self.progressText.move(int(int(self.playerWidth-10-self.iconWidth*4)), int(self.gap/2-self.progressBarHeight/2))
        self.progressText.setText("00:00:00/00:00:00")
        self.clock = QtCore.QTimer()
        self.clock.timeout.connect(self.updateClock)

        self.commands={"play":"p","pause":"p","stop":"q","volup":"+","voldown":"-",
                    "seekLeft":"^[[D","seekRight":"^[[C","seekUp":"^[[A","seekDown":"^[[B"}
        self.options={'AudioMode':['local','hdmi'],'ScaleToScreen':['False','True']}
        self.buttons[1].mouseReleaseEvent=self.chooseFile
        self.buttons[3].mouseReleaseEvent=self.playVideo
        self.buttons[5].mouseReleaseEvent=self.pauseVideo
        self.buttons[7].mouseReleaseEvent=self.stopVideo
        self.buttons[9].mouseReleaseEvent=lambda e: self.chooseSettings(self.options)
        self.volumeBars[0].mouseReleaseEvent=lambda e: self.setVolume(1)
        self.volumeBars[1].mouseReleaseEvent=lambda e: self.setVolume(2)
        self.volumeBars[2].mouseReleaseEvent=lambda e: self.setVolume(3)
        self.volumeBars[3].mouseReleaseEvent=lambda e: self.setVolume(4)
        self.volumeBars[4].mouseReleaseEvent=lambda e: self.setVolume(5)
        self.progressBar.mouseReleaseEvent=self.setSeek
        self.Log = mLog()
        self.logSize = os.path.getsize(os.path.join(DEVICE_PATH, "golfTest.log"))
        self.Control = BallControl(self.Log)
        self.CheckFileFolerIfRoot()
        self.GetFileFolder()
        self.timeout = None
        self.start_autoPlay_timer(0.5)
        self.DeviceName = self.fileGetContents(os.path.join(DEVICE_PATH, "DeviceName"))
        #self.PingTimer = int(self.fileGetPingTimerContents(os.path.join(DEVICE_PATH, "DevicePingTimer")))
        self.PingTimer = 5
        self.CheckLogSize()
        #self.AnyDeskID = self.getAnyDeskID()
        try:
            print('IP:{}'.format(self.getIP()))
            print('AnyDesk ID:{}'.format(self.DeviceName))
            
            #self.getTeamViewerInfo()
        except:
            print('ping error')

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

    def getTeamViewerInfo(self):
        p=os.popen('teamviewer info')
        data = p.readlines()
        TeamViewerIDIndex = self.findWifiIndex(data, 0, 'TeamViewer ID:')
        if TeamViewerIDIndex != -1:
            tmpStr = data[TeamViewerIDIndex]
            print("getTeamViewerInfo,    ID:{}".format(self.deleteEmpty(tmpStr[47:])))
            return self.deleteEmpty(tmpStr[47:])

    def getAnyDeskID(self):
        p=os.popen('anydesk --get-id')
        data = p.read()
        if len(data)  !=  0:
            #print("AnyDesk ID:{}".format(self.deleteEmpty(data)))
            return self.deleteEmpty(data)
            
        
        

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
        self.timeout.start(1000 * second)

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
        
    def start_autoPlay_timer(self, second):
        if self.timeout:
            self.timeout.stop()
            self.timeout.deleteLater()
        self.timeout = QtCore.QTimer()
        self.timeout.timeout.connect(self.autoPlay_func)
        self.timeout.setSingleShot(True) 
        self.timeout.start(1000 * second)

    def CheckFileFolerIfRoot(self):
        import os, shutil
        from os import stat
        from pwd import getpwuid
        from distutils.dir_util import copy_tree
        
        FolderList = os.listdir(os.path.join(BASE_PATH, "media", "pi")) if os.path.exists(os.path.join(BASE_PATH, "media", "pi")) else []
        if(len(FolderList)>0):
            for i in range(len(FolderList)):
                if(getpwuid(stat("/media/pi/"+str(FolderList[i])).st_uid).pw_name == 'root'):
                    print(str(FolderList[i]) + " is root")
                    #os.remove("/media/pi/"+str(FolderList[i]))
                    shutil.rmtree(os.path.join("/media/pi/", str(FolderList[i])))
    #2017/11/09 Start
    def checkExistIdentifyFile(self, tmpFolderList):
        for i in range(len(tmpFolderList)):
            tmpUSBfolder = "/media/pi/"+str(tmpFolderList[i])+"/"
            tmpUSBcontent = os.listdir(tmpUSBfolder)
            print('USB content:'+str(tmpUSBcontent))
            try:
                tmpIndex = tmpUSBcontent.index('Medien.golf')
            except:
                tmpIndex = -1
            if tmpIndex>=0:
                return i
        return -1
    #2017/11/09 End
        

    def GetFileFolder(self):
                #2017/11/08
        global GlobalPlayItem
        global GlobalPlayItemCounter
        global mediaLocation
        global GlobalUSBFoler
        
        import os, shutil
        from distutils.dir_util import copy_tree
        FolderList = os.listdir(os.path.join(BASE_PATH, "media", "pi")) if os.path.exists(os.path.join(BASE_PATH, "media", "pi")) else []
        if (len(FolderList)>0):
            usbIndex = self.checkExistIdentifyFile(FolderList )        #2017/11/09
            print('usbIndex:'+str(usbIndex))
            if usbIndex != -1:
                GlobalUSBFoler = "/media/pi/"+str(FolderList[0])+"/"
                print("USB be Plug in")
                print(FolderList)
                PlayerItemList = os.listdir(GlobalUSBFoler)
                if (len(PlayerItemList) > 0):
                    Mp4ItemList = list()
                    for i in range(len(PlayerItemList)):
                        try:
                            if(PlayerItemList[i].index('.mp4', len(PlayerItemList[i])-4)>=0):
                                Mp4ItemList.append(PlayerItemList[i])
                        except:
                            print("error")
                    print(sorted(Mp4ItemList))
                    
                    GlobalPlayItemCounter = 0
                    GlobalPlayItem = sorted(Mp4ItemList)
                    mediaLocation = GlobalUSBFoler + GlobalPlayItem[0]
                else:
                    GlobalPlayItemCounter = 0
                    GlobalUSBFoler = GlobalNoUSBFolder
                    GlobalPlayItem = ['01.mp4']
                    mediaLocation = GlobalUSBFoler + GlobalPlayItem[0]
                    print("Not Find USB")
            else:
                GlobalPlayItemCounter = 0
                GlobalUSBFoler = GlobalNoUSBFolder
                GlobalPlayItem = ['01.mp4']
                mediaLocation = GlobalUSBFoler + GlobalPlayItem[0]
                print("Not Find USB")
        else:
            GlobalPlayItemCounter = 0
            GlobalUSBFoler = GlobalNoUSBFolder
            GlobalPlayItem = ['01.mp4']
            mediaLocation = GlobalUSBFoler + GlobalPlayItem[0]
            print("Not Find USB")
            
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

    def autoPlay_func(self):
        self.playFile()
        #self.start_ping_timer(self.PingTimer)
    
    def keyPressEvent(self, event):
        print("key down");
        print(event.key())
        global MediaStatus
        print(MediaStatus)
##                MediaStatus=False
##                self.stopVideo("e")
##                dialog = MimeDialog(self)
##                dialog.exec_()
##                self.playFile()
##                MediaStatus = True
        
        if(event.key() == 32 and MediaStatus==True):
            print("pause")
            MediaStatus=False
            self.pauseVideo("e")
        elif(event.key() == 32 and MediaStatus==False):
            print("play")
            MediaStatus=True
            self.playVideo("e")
        elif(event.key() == 16777216):
            self.showMinimized()
            print("stop")
            MediaStatus=False
            self.stopVideo("e")
        elif(event.key() == 16777220):
            print("Enter")
            self.playFile()
            MediaStatus = True
        else:
            self.startDailog()
##        elif(event.key()==16777222 or event.key()== 48):
##            print("key 0")
##            self.startDailog()
##        elif(event.key()==16777233 or event.key()== 49):
##            print("key 1")
##            self.startDailog()
##        elif(event.key()==16777237 or event.key()== 50):
##            print("key 2")
##            self.startDailog()
##        elif(event.key()==16777239 or event.key()==51):
##            print("key 3")
##            self.startDailog()
##        elif(event.key()==16777234 or event.key()==52):
##            print("key 4")
##            self.startDailog()
##        elif(event.key()==16777227 or event.key()==53):
##            print("key 5")
##            self.startDailog()
##        elif(event.key()==16777236 or event.key()==54):
##            print("key 6")
##            self.startDailog()
##        elif(event.key()==16777232 or event.key()==55):
##            print("key 7")
##            self.startDailog()
##        elif(event.key()==16777235 or event.key()==56):
##            print("key 8")
##            self.startDailog()
##        elif(event.key()==16777238 or event.key()==57):
##            print("key 9")
##            self.startDailog()

##        def startDailog(self):
##                global MediaStatus
##                MediaStatus=False
##                self.stopVideo("e")
##                dialog = MimeDialog(self)
##                dialog.exec_()
##                self.playFile()
##                MediaStatus = True
    def CheckLogSize(self):
        import os
        from shutil import copyfile
        tmplogSize = os.path.getsize(os.path.join(DEVICE_PATH, "golfTest.log"))
        print("CheckLogSize")
        print(self.logSize)
        print(tmplogSize)
        if (tmplogSize>(1024*1024 * 2)) or (tmplogSize==self.logSize):
            copyfile(os.path.join(DEVICE_PATH, "golfTest.log"), 'os.path.join(DEVICE_PATH, "golfTest.log").1')
            os.remove(os.path.join(DEVICE_PATH, "golfTest.log"))
            self.Log = mLog()

    def startDailog(self):
        global MediaStatus
        MediaStatus=False
        self.stopVideo("e")
        self.Log.i("MediaPlayer_startDailog============startTrade")
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
        self.Log.i("MediaPlayer_startDailog============endTrade")
        self.playFile()
        MediaStatus = True
        self.CheckLogSize()
        

    def playFile(self):
        self.buttons[0].setPixmap(self.buttonPressedPic)
        self.app.processEvents()
        self.stopVideo()
        global mediaLocation
        global GlobalVideoCounter
        global MediaStatus
        self.Log.i("MediaPlayer_playFile============playFile:" + mediaLocation)
        GlobalVideoCounter = 0
        MediaStatus==True
        self.file = mediaLocation
        self.buttons[0].setPixmap(self.buttonDepressedPic)
        if self.file=='':
            return
        self.buttons[2].setPixmap(self.buttonPressedPic)
        self.buttons[4].setPixmap(self.buttonDepressedPic)
        self.buttons[6].setPixmap(self.buttonDepressedPic)
        self.app.processEvents()
        if platform.system() == "Darwin":
            print("✅ Playing with macOS 'open'")
            subprocess.call(["open", self.file])
        else:
            self.process = subprocess.Popen(["/usr/bin/omxplayer", "-o", settings["AudioMode"], self.file], stdin=subprocess.PIPE)
        s=p.communicate('')[1]
        
        for line in s.split('\n'):
            l=line.lstrip()
            if l.startswith('Duration: '):
                self.duration=int(l[10:12] * 3600)+int(l[13:15] * 60)+int(l[16:18])
            elif not l.find('Video: ')==-1:
                resolution=l.split(',')[2].split(' ')[1]
                ind=resolution.find('x')
                print("resolution")
                print(resolution)
                try:
                    self.videoWidth=int(resolution[0:ind])
                    self.videoHeight=int(resolution[ind+1:])
                except:
                    self.videoWidth=int(1280)
                    self.videoHeight=int(1024)
        self.progressText.setText("00:00:00/"+self.getClockString(self.duration))
        if self.videoWidth==0: #sound file
            self.process=subprocess.Popen(['/usr/bin/omxplayer','-o',settings['AudioMode'],
                self.file],stdin=subprocess.PIPE)
        else: #video file
            if settings['ScaleToScreen']=='True':
                position=str(self.geometry().left())+' '+str(self.geometry().top())+' '+str(self.geometry().left()+self.playerWidth)+' '+str(self.geometry().top()+self.playerHeight)
            else:
                newWidth=min(self.playerWidth,self.videoWidth)
                newHeight=min(self.playerHeight,self.videoHeight)
                position=str(self.geometry().left()+(self.playerWidth-newWidth)/2)+' '+\
                            str(self.geometry().top()+(self.playerHeight-newHeight)/2)+' '+\
                            str(self.geometry().left()+(self.playerWidth-newWidth)/2+newWidth)+' '+\
                            str(self.geometry().top()+(self.playerHeight-newHeight)/2+newHeight)
            self.process=subprocess.Popen(['/usr/bin/omxplayer','-o',settings['AudioMode'],
                '--win',position,self.file],stdin=subprocess.PIPE)
        QtCore.QThread.sleep(2) #wait for video to start
        self.clock.start(1000)
        oldVolume=self.volume
        self.volume=3
        self.setVolume(oldVolume)
        

    def resizeEvent(self,e):
        #print('relocate to '+str(x)+' '+str(y)+' '+str(width)+' '+str(height))
        width=e.size().width()
        height=e.size().height()
        self.playerWidth=width
        self.playerHeight=height-self.labelHeight
        self.label.resize(int(self.playerWidth),int(self.labelHeight))
        self.label.move(int(0),int(self.playerHeight))
        bgPic=QtGui.QPixmap(str('mediaplayer/panel.png')).scaled(QtCore.QSize(int(int(self.playerWidth)),int(self.labelHeight)))
        self.label.setPixmap(bgPic)
        self.player.resize(int(self.playerWidth),int(self.playerHeight))
        self.spacing=(self.playerWidth-self.numButtons*self.iconWidth-self.volumeBarWidth*10)/(self.numButtons+1)
        self.gap=(self.labelHeight-self.iconHeight)/2
        for i in range(0,self.numButtons):
            self.buttons[i*2].move(int(int(i*self.iconWidth+(i+1))*self.spacing), int(self.gap+self.progressBarHeight))
            self.buttons[i*2+1].move(int(int(i*self.iconWidth+(i+1))*self.spacing), int(self.gap+self.progressBarHeight))
        for i in range(0,5):
            self.volumeBars[i].move(int(self.playerWidth+(2*i-10))*self.volumeBarWidth,
                    self.gap+self.progressBarHeight+self.iconHeight-self.iconHeight/6*(i+1))
        self.progressBar.resize(int(self.playerWidth-20-self.iconWidth*4),int(self.progressBarHeight))
        self.progressBar.move(int(10),int(int(self.gap/2)))
        progressBarPic=QtGui.QPixmap(str(os.path.join(BASE_PATH, "mediaplayer/depressed.png"))).scaled(QtCore.QSize(int(int(self.playerWidth-20-self.iconWidth*4)),int(self.progressBarHeight)))
        self.progressBar.setPixmap(progressBarPic)
        self.progressCircle.move(int(10),int(int(self.gap/2-self.progressBarHeight/2)))
        self.progressText.move(int(int(self.playerWidth-10-self.iconWidth*4)), int(self.gap/2-self.progressBarHeight/2))
        if self.process is not None and self.process.poll() is None:
            if settings['ScaleToScreen']=='True':
                position=[str(self.geometry().left()),str(self.geometry().top()),str(self.geometry().left()+self.playerWidth),str(self.geometry().top()+self.playerHeight)]
            else:
                newWidth=min(self.playerWidth,self.videoWidth)
                newHeight=min(self.playerHeight,self.videoHeight)
                position=[str(self.geometry().left()+(self.playerWidth-newWidth)/2),
                            str(self.geometry().top()+(self.playerHeight-newHeight)/2),
                            str(self.geometry().left()+(self.playerWidth-newWidth)/2+newWidth),
                            str(self.geometry().top()+(self.playerHeight-newHeight)/2+newHeight)]
            subprocess.call([os.path.abspath('dbuscontrol.sh'),'setvideopos',position[0],position[1],position[2],position[3]])


    def chooseFile(self,e):
        self.buttons[0].setPixmap(self.buttonPressedPic)
        self.app.processEvents()
        self.stopVideo()
        global mediaLocation
        self.file=str(QtGui.QFileDialog.getOpenFileName(self,"Select a media file",mediaLocation,"Music/Video (*.mp3 *.mp4)"))
        self.buttons[0].setPixmap(self.buttonDepressedPic)
        if self.file=='':
            return
        self.buttons[2].setPixmap(self.buttonPressedPic)
        self.buttons[4].setPixmap(self.buttonDepressedPic)
        self.buttons[6].setPixmap(self.buttonDepressedPic)
        self.app.processEvents()
        mediaLocation=self.file
        if platform.system() == "Darwin":
            print("✅ Playing with macOS 'open'")
            subprocess.call(["open", self.file])
        else:
            self.process = subprocess.Popen(["/usr/bin/omxplayer", "-o", settings["AudioMode"], self.file], stdin=subprocess.PIPE)
        s=p.communicate('')[1]
        
        for line in s.split('\n'):
            l=line.lstrip()
            if l.startswith('Duration: '):
                self.duration=int(l[10:12] * 3600)+int(l[13:15] * 60)+int(l[16:18])
            elif not l.find('Video: ')==-1:
                resolution=l.split(',')[2].split(' ')[1]
                ind=resolution.find('x')
                self.videoWidth=int(resolution[0:ind])
                self.videoHeight=int(resolution[ind+1:])
        self.progressText.setText("00:00:00/"+self.getClockString(self.duration))
        if self.videoWidth==0: #sound file
            self.process=subprocess.Popen(['/usr/bin/omxplayer','-o',settings['AudioMode'],
                self.file],stdin=subprocess.PIPE)
        else: #video file
            if settings['ScaleToScreen']=='True':
                position=str(self.geometry().left())+' '+str(self.geometry().top())+' '+str(self.geometry().left()+self.playerWidth)+' '+str(self.geometry().top()+self.playerHeight)
            else:
                newWidth=min(self.playerWidth,self.videoWidth)
                newHeight=min(self.playerHeight,self.videoHeight)
                position=str(self.geometry().left()+(self.playerWidth-newWidth)/2)+' '+\
                            str(self.geometry().top()+(self.playerHeight-newHeight)/2)+' '+\
                            str(self.geometry().left()+(self.playerWidth-newWidth)/2+newWidth)+' '+\
                            str(self.geometry().top()+(self.playerHeight-newHeight)/2+newHeight)
            self.process=subprocess.Popen(['/usr/bin/omxplayer','-o',settings['AudioMode'],
                '--win',position,self.file],stdin=subprocess.PIPE)
        QtCore.QThread.sleep(2) #wait for video to start
        self.clock.start(1000)
        oldVolume=self.volume
        self.volume=3
        self.setVolume(oldVolume)
        

    def controlPlayer(self,type):
        if self.process is not None and self.process.poll() is None:
            self.process.stdin.write(self.commands[type])

    def updateClock(self):
#        print("updateClock")
#        print(os.path.abspath('dbuscontrol.sh'))
        global GlobalVideoCounter
        GlobalVideoCounter = GlobalVideoCounter + 1
        if(GlobalVideoCounter+1 == self.duration):
            print("updateClock")
            print("play finish!")
            print("stop")
            try:
                httpR = HttpCmdLibrary.build('12345678', self.DeviceName, self.Log)
                tmpIp = self.getIP()
                #tmpTeamViewweId = self.getTeamViewerInfo()
                #tmpAnyDeskID = getAnyDeskID
                #tmpAnyDeskID = self.AnyDeskID
                #print('AnyDesk ID:{}'.format(tmpAnyDeskID))
                #tmpTeamViewweId = 'NULL'
                #print('Current TeamViewerId:{}'.format(tmpTeamViewweId))
                #httpR.SendPingCmd(tmpIp+'__'+tmpTeamViewweId+'_'+appVersion)
                #httpR.SendPingCmd(tmpIp + '_' + tmpAnyDeskID + '_' + appVersion)
                
                print('Current IP:{}'.format(tmpIp))
                httpR.SendPingCmd(tmpIp + '_' + appVersion)
                
            except:
                print('ping error')
            MediaStatus=False
            self.stopVideo("e")
            self.DoNext()
            self.playFile()
#        print("Current:"+str(GlobalVideoCounter))
#        print("Duration:"+str(self.duration))
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
        else:
            self.stopVideo()

    def getClockString(self,n):
        return str(n/3600).zfill(2)+':'+str((n%3600)/60).zfill(2)+':'+str(n%60).zfill(2)

    def playVideo(self,e):
        if self.process is not None and self.process.poll() is None and not self.clock.isActive():
            self.clock.start(1000)
            self.controlPlayer('play')
            self.buttons[2].setPixmap(self.buttonPressedPic)
            self.buttons[4].setPixmap(self.buttonDepressedPic)
            self.buttons[6].setPixmap(self.buttonDepressedPic)

    def pauseVideo(self,e):
        if self.clock.isActive():
            self.clock.stop()
            self.controlPlayer('pause')
            self.buttons[2].setPixmap(self.buttonDepressedPic)
            self.buttons[4].setPixmap(self.buttonPressedPic)
            self.buttons[6].setPixmap(self.buttonDepressedPic)

    def stopVideo(self,e=None):
        self.clock.stop()
        self.progressText.setText("00:00:00/00:00:00")
        self.progressCircle.move(int(10),int(int(self.gap/2-self.progressBarHeight/2)))
        if self.process is not None and self.process.poll() is None:
            self.process.stdin.write('q')
        self.duration=0
        self.videoWidth=0
        self.videoHeight=0
        self.timeElapsed=0
        self.process=None
        self.buttons[2].setPixmap(self.buttonDepressedPic)
        self.buttons[4].setPixmap(self.buttonDepressedPic)
        self.buttons[6].setPixmap(self.buttonPressedPic)

    def chooseSettings(self,choices):
        global settings
        self.stopVideo()
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

    def setVolume(self,v):
        for i in range(0,5):
            if i<v:
                self.volumeBars[i].setPixmap(self.volumeBarPressed[i])
            else:
                self.volumeBars[i].setPixmap(self.volumeBarDepressed[i])
        self.app.processEvents()
        if self.process is not None and self.process.poll() is None:
            command='volup'
            if v<self.volume:
                command='voldown'
            for i in range(0,abs(v-self.volume)*2):
                QtCore.QTimer.singleShot(i*50,lambda: self.controlPlayer(command))
        self.volume=v

    def setSeek(self,e):
        if self.process is not None and self.process.poll() is None:
            t = e.pos().x()*self.duration/(self.progressBar.size().width()-self.progressCircle.size().width())
            if t>0 and t<self.duration:
                self.timeElapsed = t
                subprocess.call([os.path.abspath('dbuscontrol.sh'),'setposition',str(self.timeElapsed*1000000)])


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
        #self.setStyleSheet("font-size : 40px; background-image: url(image/2_1.jpg); background-repeat: no-repeat;")
        #self.setStyleSheet("border-image: url(image/2_1.png); background-attachment: fixed;")
        self.isConnect = self.checkWifi()
        print('System connection status: ', self.isConnect)
        palette = QtGui.QPalette()
        if self.isConnect == True:
            palette.setBrush(QtGui.QPalette.Background,QtGui.QBrush(QtGui.QPixmap("image/2_1wireless.png")))
        else:
            palette.setBrush(QtGui.QPalette.Background,QtGui.QBrush(QtGui.QPixmap("image/2_1NoWireless.png")))
        
        self.setPalette(palette)
        self.Log = tmplog
        self.Log.i("TakeBallDialog_Initial")
        self.setWindowTitle("")
        # self.textEdit = QtGui.QTextEdit(self)
        font = QtGui.QFont()
        font.setPointSize(30)
        font.setBold(True)
        #font.setWeight(75)
        #self.setFont(font)
        

##        if self.isConnect == True:
##            MsgConnect = 'Internet Connect!'
##        else:
##            MsgConnect = 'Internet Disconnect!'
##        brush = QtGui.QBrush(QtGui.QColor(255, 255, 255))
##        brush.setStyle(QtCore.Qt.SolidPattern)
##
##        palette = QtGui.QPalette()
##        palette.setBrush(QtGui.QPalette.Active, QtGui.QPalette.WindowText, brush)
##
##
##        self.connectStatus = QtWidgets.QLabel(self)
##        self.connectStatus.setText(MsgConnect)
##        self.connectStatus.setPalette(palette)
##        self.connectStatus.move(int(1450),int(50))
##        self.connectStatus.resize(int(490),int(70))
##        self.connectStatus.setFont(font)

        brush = QtGui.QBrush(QtGui.QColor(255, 153, 71))
        brush.setStyle(QtCore.Qt.SolidPattern)

        palette = QtGui.QPalette()
        palette.setBrush(QtGui.QPalette.Active, QtGui.QPalette.WindowText, brush)
        
        #self.txtTransaction_code = QtWidgets.QLabel(self)
        #self.txtTransaction_code.setText(unicode("請輸入取球序號!", 'utf-8'))

        font = QtGui.QFont()
        font.setPointSize(50)
        font.setBold(True)

        self.txtTransaction_value = QtWidgets.QLabel(self)
        self.txtTransaction_value.setText("")
        self.txtTransaction_value.setPalette(palette)
        self.txtTransaction_value.move(int(660),int(500))
        self.txtTransaction_value.resize(int(490),int(70))
        self.txtTransaction_value.setFont(font)
        self.txtTransaction_value.setAlignment(QtCore.Qt.AlignCenter)

        #layout = QtWidgets.QVBoxLayout(self)
        #layout.addWidget(self.txtTransaction_code)
        #layout.addWidget(self.txtTransaction_value)
        #layout.addWidget(self.textEdit)
        self.resize(int(1820),int(1080))
        self.tmpHttpR = httpR
        self.Lock = False
        self.timeoutFlag = False
        self.timeout = None
#        self.start_timer(7)
        
        if self.isConnect == True:
            self.start_timer(7)
        else:
            self.start_timer2(27)

    
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
        if self.isConnect == True:
            if(EK==48 or EK==16777222):
                self.txtTransaction_value.setText(self.txtTransaction_value.text()+"0")
            elif(EK==49 or EK==16777233):
                self.txtTransaction_value.setText(self.txtTransaction_value.text()+"1")
            elif(EK==50 or EK==16777237):
                self.txtTransaction_value.setText(self.txtTransaction_value.text()+"2")
            elif(EK==51 or EK==16777239):
                self.txtTransaction_value.setText(self.txtTransaction_value.text()+"3")
            elif(EK==52 or EK==16777234):
                self.txtTransaction_value.setText(self.txtTransaction_value.text()+"4")
            elif(EK==53 or EK==16777227):
                self.txtTransaction_value.setText(self.txtTransaction_value.text()+"5")
            elif(EK==54 or EK==16777236):
                self.txtTransaction_value.setText(self.txtTransaction_value.text()+"6")
            elif(EK==55 or EK==16777232):
                self.txtTransaction_value.setText(self.txtTransaction_value.text()+"7")
            elif(EK==56 or EK==16777235):
                self.txtTransaction_value.setText(self.txtTransaction_value.text()+"8")
            elif(EK==57 or EK==16777238):
                self.txtTransaction_value.setText(self.txtTransaction_value.text()+"9")
            elif(EK==16777221):
                self.SendHttpRequest()
            elif(EK==16777219):
                print("delete")
                tmpString = self.txtTransaction_value.text()
                print("delete", len(tmpString))
                self.txtTransaction_value.setText(tmpString[:len(tmpString)-1])
        if(EK == 16777216):
            self.CloseDialog()

    def SendHttpRequest(self):
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
        print("TakeBallDialog timeout)!")
        if(self.timeoutFlag):
            self.tmpHttpR.error_description = "輸入超時"
            self.CloseDialog()
            self.Log.i("TakeBallDialog_timeoutFunc")
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
        self.timeout.start(1000 * second)
        
    def start_timer2(self, second):
        if self.timeout:
            self.timeout.stop()
            self.timeout.deleteLater()
        self.timeout = QtCore.QTimer()
        self.timeout.timeout.connect(self.timeoutFunc2)
        self.timeout.setSingleShot(True) 
        self.timeout.start(1000 * second)
        


class TakeBallConfirmDialog(QtWidgets.QDialog):
    def __init__(self, parent, httpR, tmplog):
        QtWidgets.QDialog.__init__(self, parent, QtCore.Qt.FramelessWindowHint)
        self.setAttribute(QtCore.Qt.WA_DeleteOnClose)
        self.setCursor(QtGui.QCursor(QtCore.Qt.BlankCursor))
        self.setWindowTitle("")

        #self.setStyleSheet("font-size : 40px; background-image: url(test.jpg); background-repeat: no-repeat;")
        palette = QtGui.QPalette()
        palette.setBrush(QtGui.QPalette.Background,QtGui.QBrush(QtGui.QPixmap("image/2_3.jpg")))
        self.setPalette(palette)
        font = QtGui.QFont()
        font.setPointSize(50)
        font.setBold(True)
        #font.setWeight(75)
        #self.setFont(font)

        self.Log = tmplog
        self.Log.i("TakeBallConfirmDialog_Initial,    "+unicode("交易號碼", 'utf-8') + " :  " + httpR.response_id+", "+unicode("取球數量", 'utf-8') + " :  " + str(httpR.response_take)+", "+unicode("剩餘球數", 'utf-8') + " :  " +str(httpR.response_ball_before))
        
        #self.txtTransaction_code = QtWidgets.QLabel(self)
        #self.txtTransaction_code.setText(unicode("交易號碼", 'utf-8') + " :  " + httpR.response_id)

        txtTake_brush = QtGui.QBrush(QtGui.QColor(255, 255, 255))
        txtTake_brush.setStyle(QtCore.Qt.SolidPattern)

        txtTake_palette = QtGui.QPalette()
        txtTake_palette.setBrush(QtGui.QPalette.Active, QtGui.QPalette.WindowText, txtTake_brush)
        
        self.txtTake = QtWidgets.QLabel(self)
        #self.txtTake.setText(unicode("取球數量", 'utf-8') + " :  " + str(httpR.response_take))
        self.txtTake.setText(str(httpR.response_take))
        self.txtTake.setPalette(txtTake_palette)
        self.txtTake.move(int(750),int(350))
        self.txtTake.resize(int(490),int(70))
        self.txtTake.setFont(font)
        #self.txtTake.setAlignment(QtCore.Qt.AlignVCenter)
        self.txtTake.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)

        txtBall_brush = QtGui.QBrush(QtGui.QColor(255, 153, 71))
        txtBall_brush.setStyle(QtCore.Qt.SolidPattern)

        txtBall_palette = QtGui.QPalette()
        txtBall_palette.setBrush(QtGui.QPalette.Active, QtGui.QPalette.WindowText, txtBall_brush)

        self.txtBall = QtWidgets.QLabel(self)
        #self.txtBall.setText(unicode("剩餘球數", 'utf-8') + " :  " +str(httpR.response_ball))
        self.txtBall.setText(str(httpR.response_use))
        self.txtBall.setPalette(txtBall_palette)
        self.txtBall.move(int(750),int(520))
        self.txtBall.resize(int(490),int(70))
        self.txtBall.setFont(font)
        #self.txtBall.setAlignment(QtCore.Qt.AlignVCenter)
        self.txtBall.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)
        
        
        #self.buttonConfirmOk = QtWidgets.QPushButton(unicode('確認', 'utf-8'), self)
        #self.buttonConfirmOk.clicked.connect(self.ConfirmOk)

        #layout = QtWidgets.QVBoxLayout(self)
        #layout.addWidget(self.txtTransaction_code)
        #layout.addWidget(self.txtTake)
        #layout.addWidget(self.txtBall)
        #layout.addWidget(self.buttonConfirmOk)
        self.resize(int(1920),int(1080))
        self.tmpHttpR = httpR
        self.Lock = False
        self.timeout = None
        self.start_timer(10)

    def keyPressEvent(self, event):
        print("MimeDialog,  key down")
        print(event.key())
        self.timeoutFlag = False
        EK = event.key()
        if(EK==48):
            self.CloseDialog()
        elif(EK==16777221):
            self.ConfirmOk()
        elif(EK == 16777216):
            self.CloseDialog()

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
        self.timeout.start(1000 * second)

class ErrorDialog(QtWidgets.QDialog):
    def __init__(self, parent, ErrorTitle, ErrorMsg, tmplog):
        QtWidgets.QDialog.__init__(self, parent, QtCore.Qt.FramelessWindowHint)
        self.setAttribute(QtCore.Qt.WA_DeleteOnClose)
        self.setCursor(QtGui.QCursor(QtCore.Qt.BlankCursor))
        self.setWindowTitle(unicode("Error", 'utf-8'))
        #self.setStyleSheet("font-size : 40px; background-image: url(test.jpg); background-repeat: no-repeat;")
        palette = QtGui.QPalette()
        palette.setBrush(QtGui.QPalette.Background,QtGui.QBrush(QtGui.QPixmap("image/2_4.jpg")))
        self.setPalette(palette)

        code_font = QtGui.QFont()
        code_font.setPointSize(50)
        code_font.setBold(True)
        value_font = QtGui.QFont()
        value_font.setPointSize(80)
        value_font.setBold(True)
        #font.setWeight(75)
        #self.setFont(font)

        self.Log = tmplog
        self.Log.e("ErrorDialog_Initial,    Title:"+unicode(ErrorTitle, 'utf-8')+", Msg:"+unicode(ErrorMsg, 'utf-8'))
        
        
        #self.txtTransaction_code = QtWidgets.QLabel(self)
        #self.txtTransaction_code.setText(unicode(ErrorTitle, 'utf-8'))

        #self.txtTransaction_value = QtWidgets.QLabel(self)
        #self.txtTransaction_value.setText(unicode(ErrorMsg, 'utf-8'))


        txtTransaction_value_brush = QtGui.QBrush(QtGui.QColor(255, 153, 71))
        txtTransaction_value_brush.setStyle(QtCore.Qt.SolidPattern)

        txtTransaction_value_palette = QtGui.QPalette()
        txtTransaction_value_palette.setBrush(QtGui.QPalette.Active, QtGui.QPalette.WindowText, txtTransaction_value_brush)

        self.txtTransaction_value = QtWidgets.QLabel(self)
        self.txtTransaction_value.setText(unicode(ErrorMsg, 'utf-8'))
        self.txtTransaction_value.setPalette(txtTransaction_value_palette)
        self.txtTransaction_value.move(int(715),int(500))
        self.txtTransaction_value.resize(int(490),int(100))
        self.txtTransaction_value.setFont(value_font)
        self.txtTransaction_value.setAlignment(QtCore.Qt.AlignCenter)
        #self.txtTransaction_value.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)

        



        txtTransaction_code_brush = QtGui.QBrush(QtGui.QColor(255, 255, 255))
        txtTransaction_code_brush.setStyle(QtCore.Qt.SolidPattern)

        txtTransaction_code_palette = QtGui.QPalette()
        txtTransaction_code_palette.setBrush(QtGui.QPalette.Active, QtGui.QPalette.WindowText, txtTransaction_code_brush)

        self.txtTransaction_code = QtWidgets.QLabel(self)
        self.txtTransaction_code.setText(unicode(ErrorTitle, 'utf-8'))
        self.txtTransaction_code.setPalette(txtTransaction_code_palette)
        self.txtTransaction_code.move(int(715),int(30))
        self.txtTransaction_code.resize(int(490),int(70))
        self.txtTransaction_code.setFont(code_font)
        self.txtTransaction_code.setAlignment(QtCore.Qt.AlignCenter)
        

        #layout = QtWidgets.QVBoxLayout(self)
        #layout.addWidget(self.txtTransaction_code)
        #layout.addWidget(self.txtTransaction_value)
        self.resize(int(1920),int(1080))
        self.timeout = None
        self.start_timer(5)

    def keyPressEvent(self, event):
        print("ErrorDialog,  colse dialog")
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
        self.timeout.start(1000 * second)

class SuccessDialog(QtWidgets.QDialog):
    def __init__(self, parent, httpR, tmplog, tmpBallControl):
        QtWidgets.QDialog.__init__(self, parent, QtCore.Qt.FramelessWindowHint)
        self.setAttribute(QtCore.Qt.WA_DeleteOnClose)
        self.setCursor(QtGui.QCursor(QtCore.Qt.BlankCursor))
        self.setWindowTitle('')
        #self.setStyleSheet("font-size : 40px; background-image: url(test.jpg); background-repeat: no-repeat; background-position: center;")
        palette = QtGui.QPalette()
        palette.setBrush(QtGui.QPalette.Background,QtGui.QBrush(QtGui.QPixmap("image/3_1.jpg")))
        self.setPalette(palette)

        font = QtGui.QFont()
        font.setPointSize(30)
        font.setBold(True)
        txtTake_font = QtGui.QFont()
        txtTake_font.setPointSize(200)
        txtTake_font.setBold(True)
        
        #font.setWeight(75)
        #self.setFont(font)

        self.TotalTime = 5 - httpR.response_timer
        
        self.Log = tmplog
        self.Log.i("SuccessDialog_Initial,    "+unicode("交易號碼", 'utf-8') + " :  " + httpR.response_id+", "+unicode("剩餘球數", 'utf-8') + " :  " +str(httpR.response_ball_after))



        #===============================================UI
        txtTake_brush = QtGui.QBrush(QtGui.QColor(255, 255, 255))
        txtTake_brush.setStyle(QtCore.Qt.SolidPattern)

        txtTake_palette = QtGui.QPalette()
        txtTake_palette.setBrush(QtGui.QPalette.Active, QtGui.QPalette.WindowText, txtTake_brush)
        
        self.txtTake = QtWidgets.QLabel(self)
        #self.txtTake.setText(unicode("取球數量", 'utf-8') + " :  " + str(httpR.response_take))
        self.txtTake.setText(str(httpR.response_take))
        self.txtTake.setPalette(txtTake_palette)
        self.txtTake.move(int(950),int(380))
        self.txtTake.resize(int(620),int(240))
        self.txtTake.setFont(txtTake_font)
        self.txtTake.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)

        #self.txtTransaction_code = QtWidgets.QLabel(self)
        #self.txtTransaction_code.setText(unicode("交易號碼", 'utf-8') + " :  " + httpR.response_id)



        txtBall_brush = QtGui.QBrush(QtGui.QColor(255, 255, 255))
        txtBall_brush.setStyle(QtCore.Qt.SolidPattern)

        txtBall_palette = QtGui.QPalette()
        txtBall_palette.setBrush(QtGui.QPalette.Active, QtGui.QPalette.WindowText, txtBall_brush)

        self.txtBall_after = QtWidgets.QLabel(self)
        #self.txtBall.setText(unicode("剩餘球數", 'utf-8') + " :  " +str(httpR.response_ball_after))
        self.txtBall_after.setText(str(httpR.response_ball_after))
        self.txtBall_after.setPalette(txtBall_palette)
        self.txtBall_after.move(int(270),int(450))
        self.txtBall_after.resize(int(490),int(240))
        self.txtBall_after.setFont(font)
        self.txtBall_after.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)


        diff_value = int(httpR.response_use)

        txtBall_brush_diff = QtGui.QBrush(QtGui.QColor(255, 153, 71))
        txtBall_brush_diff.setStyle(QtCore.Qt.SolidPattern)

        txtBall_palette_diff = QtGui.QPalette()
        txtBall_palette_diff.setBrush(QtGui.QPalette.Active, QtGui.QPalette.WindowText, txtBall_brush_diff)

        self.txtBall_diff = QtWidgets.QLabel(self)
        self.txtBall_diff.setText(str(diff_value))
        self.txtBall_diff.setPalette(txtBall_palette_diff)
        self.txtBall_diff.move(int(270),int(320))
        self.txtBall_diff.resize(int(490),int(240))
        self.txtBall_diff.setFont(font)
        self.txtBall_diff.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)

        



        txtBall_brush_before = QtGui.QBrush(QtGui.QColor(108, 204, 108))
        txtBall_brush_before.setStyle(QtCore.Qt.SolidPattern)

        txtBall_palette_before = QtGui.QPalette()
        txtBall_palette_before.setBrush(QtGui.QPalette.Active, QtGui.QPalette.WindowText, txtBall_brush_before)

        self.txtBall_before = QtWidgets.QLabel(self)
        self.txtBall_before.setText(str(httpR.response_ball_before))
        self.txtBall_before.setPalette(txtBall_palette_before)
        self.txtBall_before.move(int(270),int(225))
        self.txtBall_before.resize(int(490),int(240))
        self.txtBall_before.setFont(font)
        self.txtBall_before.setAlignment(QtCore.Qt.AlignRight|QtCore.Qt.AlignVCenter)

        #layout = QtWidgets.QVBoxLayout(self)
        #layout.addWidget(self.txtTransaction_code)
        #layout.addWidget(self.txtBall)
        self.resize(int(1920),int(1080))
        #==========================Controller
        self.Control = tmpBallControl
        self.Control.setSetting(httpR.response_type, httpR.response_timer)
        self.ReverseTime = self.Control.getReverseTime(httpR.response_take)
        self.timeout = None
        #20180307
        self.tmpHttpR = httpR
        if httpR.response_type == 2:
            self.plateNum = math.ceil(float(httpR.response_take) / float(httpR.response_ball_per_plate))
            self.plateCounter = 0
            self.plateDelayDic = {'StopInterval':float(httpR.response_interval_timer), 'StartInterval':float(httpR.response_start_timer)}
            self.start_timer(self.plateDelayDic['StartInterval'])
            self.Control.StartMotor()
        else:
            self.start_timer(self.ReverseTime)
            self.Control.StartMotor()

    def timeoutFunc(self):
        if self.tmpHttpR.response_type == 2:
            if self.Control.motorFlag == 1:
                self.Control.StopMotor()
                self.plateCounter = self.plateCounter + 1
                self.start_timer(self.plateDelayDic['StopInterval'])
            else:
                if self.plateNum == self.plateCounter:
                    self.CloseDialog()
                else:
                    self.start_timer(self.plateDelayDic['StartInterval'])
                    self.Control.StartMotor()
        else:
            self.Control.checkIsFinish()
            self.Control.StopMotor()
            #self.CloseDialog()
            if(self.TotalTime>0.5):
                self.start_timer_after_Control(self.TotalTime)
            else:
                self.start_timer_after_Control(0.5)
        #20180307 End
        
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
        self.timeout.start(1000 * second)


    def timeoutFunc_after_Control(self):
        self.CloseDialog()
        
    def start_timer_after_Control(self, second):
        if self.timeout:
            self.timeout.stop()
            self.timeout.deleteLater()
        self.timeout = QtCore.QTimer()
        self.timeout.timeout.connect(self.timeoutFunc_after_Control)
        self.timeout.setSingleShot(True) 
        self.timeout.start(1000 * second)


if __name__=='__main__':
    app = QtWidgets.QApplication([])
    mp = MediaPlayer(app,None)
    mp.move(int(0),int(-90))
    mp.resize(int(1930),int(1280))
    mp.show()
    app.exec_()
