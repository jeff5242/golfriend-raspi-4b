import platform
import os

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

import math
        # ===== GPIO Compatibility Layer for macOS/Non-Raspberry Pi =====
import platform
GPIO_AVAILABLE = False

try:
    import RPi.GPIO as GPIO
    GPIO_AVAILABLE = True
except (ImportError, RuntimeError):
    print("⚠️ RPi.GPIO module not available. GPIO features disabled.")
    class MockGPIO:
        BCM = 'BCM'
        OUT = 'OUT'
        IN = 'IN'
        HIGH = 1
        LOW = 0
        BOARD = OUT = IN = LOW = HIGH = None
        def setmode(self, *args, **kwargs): pass
        def setup(self, *args, **kwargs): pass
        def output(self, *args, **kwargs): pass
        def input(self, *args, **kwargs): return 0
        def cleanup(self, *args, **kwargs): pass
        def setwarnings(self, *args, **kwargs): pass
    GPIO = MockGPIO()
    # ================================================================
    
from time import sleep
class BallControl:
    def __init__(self, tmplog):
        self.OutputPinNum = 18
        self.InputPinNum = 17
        self.ModeSetting = None
        self.Interval = None
        self.Log = tmplog
        self.NumberOfReverse = 0
        self.motorFlag = 0
        try:
            GPIO.setmode(GPIO.BCM)
            GPIO.setwarnings(False)
            GPIO.setup(self.OutputPinNum, GPIO.OUT)
            GPIO.setup(self.InputPinNum, GPIO.IN)
            self.Log.i('BallControl_Initial')
        except Exception as e:
            self.Log.e(f'BallControl_Initial GPIO error: {e}')
            print(f'⚠️ GPIO init failed: {e} — motor control disabled')
    def getReverseTime(self, tmpBallNum):
        self.NumberOfReverse = math.ceil(float(tmpBallNum)/5)
        if(self.HaveSensor==False):
            self.Log.i('BallControl_getReverseTime: ' + str(self.NumberOfReverse))
            return self.NumberOfReverse * self.Interval
        else:
            return self.Interval/2
    def checkIsFinish(self):
        if(self.HaveSensor == False):
            return True
        else:
            return self.checkLightSensor()
    def checkLightSensor(self):
        Counter = 0
        OldStatus = self.ReadLightSensor()
        CurrentStatus = 1
        i=0
        while(Counter < self.NumberOfReverse):
            i = i + 1
            sleep(0.005)
            CurrentStatus = self.ReadLightSensor()
            if((CurrentStatus - OldStatus)>0):
                Counter = Counter + 1
                print("enter Cur="+str(CurrentStatus) + ", Old:"+str(OldStatus))
                OldStatus = CurrentStatus

    def setSetting(self, tmpType, tmpInterval):
        #Interval time per reverse
        if tmpType == 1:
            self.HaveSensor = False
            self.Interval = tmpInterval
            self.Log.i('BallControl_setSetting,   Timer Mode, Interval:'+str(tmpInterval))
        else:
            self.HaveSensor = True
            self.Interval = tmpInterval
            self.Log.i('BallControl_setSetting,   Sensor Mode, Interval is a half:'+str(tmpInterval))
    def StartMotor(self):
        print('BallControl_StartMotor')
        self.motorFlag = 1              #20180307
        GPIO.output(self.OutputPinNum, 1)

        # print("Relay ON")
        # GPIO.output(18, GPIO.HIGH)
        # sleep(0.1)

        # print("Relay OFF")
        # GPIO.output(18, GPIO.LOW)
        # GPIO.cleanup()

    def StopMotor(self):
        self.Log.i('BallControl_StopMotor')
        self.motorFlag = 0              #20180307
        GPIO.output(self.OutputPinNum, 0)
        #20180307
    def getMotorStatus(self):
        return self.motorFlag
        #20180307 End
    def ReadLightSensor(self):
        #print GPIO.input(self.InputPinNum)
        return int(GPIO.input(self.InputPinNum))
    if __name__=='__main__':
        # Usage
        aaa = BallControl('ss')
        for i in range(1000):
            aaa.ReadLightSensor()
            sleep(0.005)
            ##        aaa.StartMotor()
            sleep(10)
            ##        aaa.StopMotor()
