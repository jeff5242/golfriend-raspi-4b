import math
import RPi.GPIO as GPIO
from time import sleep


class BallControl:
	def __init__(self, tmplog):
		self.OutputPinNum = 18
		self.InputPinNum = 17
		self.ModeSetting = None
		self.Interval = None
		self.Log = tmplog
		GPIO.setmode(GPIO.BCM)
		GPIO.setwarnings(False)
		GPIO.setup(self.OutputPinNum, GPIO.OUT)
		GPIO.setup(self.InputPinNum, GPIO.IN)
		self.NumberOfReverse = 0
		self.Log.i('BallControl_Initial')
		self.motorFlag = 0              #20180307
		

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
##                        if(i>1000):
##                                Counter = self.NumberOfReverse

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
		self.Log.i('BallControl_StartMotor')
		self.motorFlag = 1              #20180307
		GPIO.output(self.OutputPinNum, 1)

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
