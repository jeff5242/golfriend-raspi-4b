
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

#coding=utf-8 
import hashlib, datetime, time
import requests
import json


#Usage
#self.httpR = HttpCmdLibrary.build('12345678', 'TEST_0001')
# self.httpR.setTransaction_code(str(self.txtTransaction_value.text()))
# self.httpR.SendTakeBallCmd()
# self.httpR.showTakeBallMessage()
# TakeBall_Confirm
# self.httpR.SendTakeBallConfirmCmd('takeBall'+str(self.httpR.response_take), '1')
# self.httpR.showTakeBallConfirmMessage()
class build:
	def __init__(self, tmpTransaction_code, tmpDevice_code, tmplog):
		self.error_code = True
		self.error_msg = ""
		self.error_description = ""
		self.response_id = ""
		self.response_ball_before = ""
		local_ball_before = ""
		self.response_ball_after = ""
		local_ball_after = ""
		self.response_use = ""
		self.response_take = ""
		self.transaction_code = tmpTransaction_code
		self.device_code = tmpDevice_code
		self.ProcessTag = "initial"
		self.Log = tmplog
		self.response_timer = 0.75
		self.response_type = 0
#20180307
		self.response_start_timer = 0
		self.response_interval_timer = 0
		self.response_ball_per_plate = 0
#20180307 End
		

	def setProcessStage(self, Tag):
		self.ProcessTag = Tag

	def setTransaction_code(self, tmpTransaction_code):
		self.transaction_code = tmpTransaction_code
	
	def SendTakeBallErrorSet(self, ErrorMsg):
		if ErrorMsg=="1":
			return "輸入錯誤";
		elif ErrorMsg=="2":
			return "timeout"
		elif ErrorMsg=="3":
			return "加密字串錯誤"
		elif ErrorMsg=="4":
			return "裝置不存在"
		elif ErrorMsg=="5" :
			return "取球碼不存在"
		elif ErrorMsg=="6":
			return "取球碼失效"
		elif ErrorMsg=="9":
			return "伺服器錯誤"
		else:
			return "Unknow Error"

	def SendTakeBallConfirmErrorSet(self, ErrorMsg):
		if ErrorMsg=="1":
			return "輸入錯誤";
		elif ErrorMsg=="2":
			return "timeout"
		elif ErrorMsg=="3":
			return "加密字串錯誤"
		elif ErrorMsg=="4":
			return "交易序號不存在"
		elif ErrorMsg=="5" :
			return "已設為成功 不得重複呼叫"
		elif ErrorMsg=="6":
			return "已設為失敗 不得重複呼叫"
		elif ErrorMsg=="9":
			return "伺服器錯誤"
		else:
			return "Unknow Error"

	def SendGetSettingErrorSet(self, ErrorMsg):
		if ErrorMsg=="1":
			return "輸入錯誤";
		elif ErrorMsg=="2":
			return "timeout"
		elif ErrorMsg=="3":
			return "加密字串錯誤"
		elif ErrorMsg=="4":
			return "裝置不存在"
		elif ErrorMsg=="9":
			return "伺服器錯誤"
		else:
			return "Unknow Error"

	def SendPingErrorSet(self, ErrorMsg):
		if ErrorMsg=="1":
			return "輸入錯誤";
		elif ErrorMsg=="2":
			return "timeout"
		elif ErrorMsg=="3":
			return "加密字串錯誤"
		elif ErrorMsg=="4":
			return "裝置不存在"
		elif ErrorMsg=="9":
			return "伺服器錯誤"
		else:
			return "Unknow Error"

	def SendTakeBallCmd(self):
		self.error_code = True
		self.error_description = "網路連接錯誤"
		self.ProcessTag = "TakeBall"
		currentTime = str(int(time.mktime(datetime.datetime.now().timetuple())))
		sha1 =  hashlib.sha1(("salt" + currentTime + self.transaction_code + self.device_code).encode('utf-8')).hexdigest()
		payload = {
			'device_code':self.device_code,
			'transaction_code':self.transaction_code,
			'time':currentTime,
			'sn':sha1,
		}
		print(payload)
		self.Log.i("")
		self.Log.i("HttpCmdLibrary_SendTakeBallCmd,    url:http://golfpoint.milkidea.com/api/take_ball\npayload: "+str(payload))
		try:
			r = requests.post("http://golfpoint.milkidea.com/api/take_ball", payload, timeout=10)
		except Exception as e:
			self.Log.e(f"HttpCmdLibrary_SendTakeBallCmd,    網路異常: {e}")
			self.error_code = True
			self.error_description = "網路異常，請稍後再試"
			return
		print(r.status_code, r.reason)
		print(r.text)
		
		if(r.status_code==200 or r.status_code == 400):
			self.Log.i("HttpCmdLibrary_SendTakeBallCmd,    Response Status"+str(r.status_code))
			self.Log.i("HttpCmdLibrary_SendTakeBallCmd,    \nbody:"+r.text)
			#json solve
			dict_httpResponese = json.loads(r.text)
			self.error_msg = dict_httpResponese.get('error_msg')
			self.error_code = dict_httpResponese.get('error')
			dict_body = dict_httpResponese.get('body')
			if(self.error_code==False):
				self.response_id = dict_body.get('id')
				#self.response_ball_before = dict_body.get('ball')
				
				local_ball_before = dict_body.get('point1')
				if(local_ball_before == 999999):
					self.response_ball_before = "------"
				else:
					self.response_ball_before = local_ball_before
				
				local_ball_after = dict_body.get('point2')
				if(local_ball_after == 999999):
					self.response_ball_after = "------"
				else:
					self.response_ball_after = local_ball_after
				
				#self.response_ball_before = dict_body.get('point1')
				#self.response_ball_after = dict_body.get('point2')
				
				self.response_use = dict_body.get('use')
				self.response_take = dict_body.get('take')
				self.response_timer = float(dict_body.get('timer'))
				self.response_type = int(dict_body.get('type'))
				#20180307
				self.response_start_timer = float(dict_body.get('start_timer'))
				self.response_interval_timer = float(dict_body.get('interval_timer'))
				self.response_ball_per_plate = float(dict_body.get('ball_per_plate'))
				#20180307 End
			else:
				self.Log.e("HttpCmdLibrary_SendTakeBallCmd,    json error msg"+str(self.error_msg))
				self.error_description = self.SendTakeBallErrorSet(self.error_msg)
		else:
			self.Log.e("HttpCmdLibrary_SendTakeBallCmd,    Response Status"+str(r.status_code))
			self.error_code = True
			self.error_description = str(r.reason)

	def SendTakeBallConfirmCmd(self, note, status):
		self.error_code = True
		self.error_description = "網路連接錯誤"
		self.ProcessTag = "TakeBallConfirm"
		currentTime = str(int(time.mktime(datetime.datetime.now().timetuple())))
		sha1 =  hashlib.sha1(("salt" + currentTime + self.response_id).encode('utf-8')).hexdigest()
		payload = {
			'transaction_code':self.response_id,
			'time':currentTime,
			'sn':sha1,
			'note':note,
			'status':status
		}
		print(payload)
		self.Log.i("")
		self.Log.i("HttpCmdLibrary_SendTakeBallConfirmCmd,    url:http://golfpoint.milkidea.com/api/take_ball_confirm\npayload: "+str(payload))
		try:
			r = requests.post("http://golfpoint.milkidea.com/api/take_ball_confirm", payload, timeout=10)
		except Exception as e:
			self.Log.e(f"HttpCmdLibrary_SendTakeBallConfirmCmd,    網路異常: {e}")
			self.error_code = True
			self.error_description = "網路異常，請稍後再試"
			return
		print(r.status_code, r.reason)
		print(r.text)
		if(r.status_code==200 or r.status_code == 400):
			self.Log.i("HttpCmdLibrary_SendTakeBallConfirmCmd,    Response Status"+str(r.status_code))
			self.Log.i("HttpCmdLibrary_SendTakeBallConfirmCmd,    \nbody:"+r.text)
			#json solve
			dict_httpResponese = json.loads(r.text)
			self.error_msg = dict_httpResponese.get('error_msg')
			self.error_code = dict_httpResponese.get('error')
			#if(self.error_code==False):
				#dict_body = dict_httpResponese.get('body')
				#self.response_ball_after = dict_body.get('ball')
			#else:
			if(self.error_code==True):
				self.Log.e("HttpCmdLibrary_SendTakeBallConfirmCmd,    json error msg"+str(self.error_msg))
				self.error_description = self.SendTakeBallConfirmErrorSet(self.error_msg)
		else:
			self.Log.e("HttpCmdLibrary_SendTakeBallConfirmCmd,    Response Status"+str(r.status_code))
			self.error_code = True
			self.error_description = str(r.reason)


	def SendGetSettingCmd(self):
		self.error_code = True
		self.error_description = "網路連接錯誤"
		self.ProcessTag = "GetSetting"
		currentTime = str(int(time.mktime(datetime.datetime.now().timetuple())))
		sha1 =  hashlib.sha1(("salt" + currentTime + str(self.response_timer) + self.device_code).encode('utf-8')).hexdigest()
		payload = {
			'device_code':self.device_code,
			'config':self.config,
			'time':currentTime,
			'timer':str(self.response_timer) ,
			'sn':sha1,
		}
		print(payload)
		self.Log.i("")
		self.Log.i("HttpCmdLibrary_SendGetSettingCmd,    url:http://golfpoint.milkidea.com/api/take_ball_set\npayload: "+str(payload))
		try:
			r = requests.post("http://golfpoint.milkidea.com/api/take_ball_set", payload, timeout=10)
		except Exception as e:
			self.Log.e(f"HttpCmdLibrary_SendGetSettingCmd,    網路異常: {e}")
			self.error_code = True
			self.error_description = "網路異常，請稍後再試"
			return
		print(r.status_code, r.reason)
		print(r.text)


		if(r.status_code==200  or r.status_code==400):
			self.Log.i("HttpCmdLibrary_SendGetSettingCmd,    Response Status"+str(r.status_code))
			self.Log.i("HttpCmdLibrary_SendGetSettingCmd,    \nbody:"+r.text)
			#json solve
			dict_httpResponese = json.loads(r.text)
			self.error_msg = dict_httpResponese.get('error_msg')
			self.error_code = dict_httpResponese.get('error')
			dict_body = dict_httpResponese.get('body')
			if(self.error_code==False):
				self.config = dict_body.get('config')
				self.response_timer = float(dict_body.get('timer'))
				self.response_type = int(dict_body.get('type'))
			else:
				self.Log.e("HttpCmdLibrary_SendGetSettingCmd,    json error msg"+str(self.error_msg))
				self.error_description = self.SendGetSettingErrorSet(self.error_msg)
		else:
			self.Log.e("HttpCmdLibrary_SendGetSettingCmd,    Response Status"+str(r.status_code))
			self.error_code = True
			self.error_description = str(r.reason)

	def SendPingCmd(self, note):
		self.error_code = True
		self.error_description = "網路連接錯誤"
		self.ProcessTag = "Ping"
		currentTime = str(int(time.mktime(datetime.datetime.now().timetuple())))
		sha1 =  hashlib.sha1(("salt" + currentTime + note + self.device_code).encode('utf-8')).hexdigest()
		payload = {
			'device_code':self.device_code,
			'time':currentTime,
			'note':note ,
			'sn':sha1,
		}
		print(payload)
		self.Log.i("")
		self.Log.i("HttpCmdLibrary_SendPingCmd,    url:http://golfpoint.milkidea.com/api/take_ball_ping\npayload: "+str(payload))
		try:
			r = requests.post("http://golfpoint.milkidea.com/api/take_ball_ping", payload, timeout=10)
		except Exception as e:
			self.Log.e(f"HttpCmdLibrary_SendPingCmd,    網路異常: {e}")
			self.error_code = True
			self.error_description = "網路異常"
			return
		print(r.status_code, r.reason)
		print(r.text)


		if(r.status_code==200):
			self.Log.i("HttpCmdLibrary_SendPingCmd,    Response Status"+str(r.status_code))
			self.Log.i("HttpCmdLibrary_SendPingCmd,    \nbody:"+r.text)
			#json solve
			dict_httpResponese = json.loads(r.text)
			self.error_msg = dict_httpResponese.get('error_msg')
			self.error_code = dict_httpResponese.get('error')
			dict_body = dict_httpResponese.get('body')
			if(self.error_code==False):
				self.config = dict_body.get('config')
				self.response_timer = float(dict_body.get('timer'))
				self.response_type = int(dict_body.get('type'))
			else:
				self.Log.e("HttpCmdLibrary_SendPingCmd,    json error msg"+str(self.error_msg))
				self.error_description = self.SendPingErrorSet(self.error_msg)
		else:
			self.Log.e("HttpCmdLibrary_SendPingCmd,    Response Status"+str(r.status_code))
			self.error_code = True
			self.error_description = str(r.reason)

	def showTakeBallMessage(self):
		print(("================showTakeBallMessage===================="))
		print("ErrorCode: "+ str(self.error_code) + " " + str(type(self.error_code)))
		print("ErrorMsg: "+ str(self.error_msg) + " " + str(type(self.error_msg)))
		if(self.error_code):
			print(("ErrorMsgDescription: "+self.error_description ))
		else:
			print("ResponseId: "+ str(self.response_id) + " " + str(type(self.response_id)))
			print("ResponseBall: "+ str(self.response_ball_before) + " " + str(type(self.response_ball_before)))
			print("ResponseTake: "+ str(self.response_take) + " " + str(type(self.response_take)))
			#20180307
			print("ResponseUse: "+ str(self.response_use) + " " + str(type(self.response_use)))
			print("ResponseTimer: "+ str(self.response_timer) + " " + str(type(self.response_timer)))
			print("ResponseType: "+ str(self.response_type) + " " + str(type(self.response_type)))
			print("ResponseStartTimer: "+ str(self.response_start_timer) + " " + str(type(self.response_start_timer)))
			print("ResponseIntervalTimer: "+ str(self.response_interval_timer) + " " + str(type(self.response_interval_timer)))
			print("ResponseBallPerPlate: "+ str(self.response_ball_per_plate) + " " + str(type(self.response_ball_per_plate)))
			#20180307 End

	def showTakeBallConfirmMessage(self):
		print(("================showTakeBallConfirmMessage===================="))
		print("ErrorCode: "+ str(self.error_code) + " " + str(type(self.error_code)))
		print("ErrorMsg: "+ str(self.error_msg) + " " + str(type(self.error_msg)))
		if(self.error_code):
			print(("ErrorMsgDescription: "+self.error_description ))
		else:
			print("ResponseBall: "+ str(self.response_ball_after) + " " + str(type(self.response_ball_after)))

	def showGetSettingMessage(self):
		print(("================showGetSettingMessage===================="))
		print("ErrorCode: "+ str(self.error_code) + " " + str(type(self.error_code)))
		print("ErrorMsg: "+ str(self.error_msg) + " " + str(type(self.error_msg)))
		if(self.error_code):
			print(("ErrorMsgDescription: "+self.error_description ))
		else:
			print("config: "+ str(self.config) + " " + str(type(self.config)))
			print("timer: "+ str(self.response_timer) + " " + str(type(self.response_timer)))
			print("type: "+ str(self.response_type) + " " + str(type(self.response_type)))


	def showPingMessage(self):
		print(("================showPingMessage===================="))
		print("ErrorCode: "+ str(self.error_code) + " " + str(type(self.error_code)))
		print("ErrorMsg: "+ str(self.error_msg) + " " + str(type(self.error_msg)))
		if(self.error_code):
			print(("ErrorMsgDescription: "+self.error_description ))
		else:
			print("config: "+ str(self.config) + " " + str(type(self.config)))
			print("timer: "+ str(self.response_timer) + " " + str(type(self.response_timer)))
			print("type: "+ str(self.response_type) + " " + str(type(self.response_type)))


