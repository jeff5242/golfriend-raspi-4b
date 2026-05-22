#coding=utf-8 
import logging 
from logging.handlers import RotatingFileHandler

class mLog:
	def __init__(self):
		filePath = '/home/pi/Desktop/golfTest.log'
		self.logger = logging.getLogger('golfLogger')  
		self.logger.setLevel(logging.DEBUG)
		# 创建一个handler，用于写入日志文件  
		fh = logging.FileHandler(filePath)  
		fh.setLevel(logging.DEBUG)  
		  
		# 再创建一个handler，用于输出到控制台  
		# ch = logging.StreamHandler()  
		# ch.setLevel(logging.DEBUG)  

		#size handler for limit file size
		# sh = RotatingFileHandler(filePath, maxBytes=1024*1024, backupCount=5)
		  
		# 定义handler的输出格式  
		formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')  
		fh.setFormatter(formatter)  
		# ch.setFormatter(formatter)  
		# sh.setFormatter(formatter)
		  
		# 给logger添加handler  
		#self.logger.addHandler(fh)  
		# self.logger.addHandler(ch)
		# self.logger.addHandler(sh)    

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

# Usage
# if __name__=='__main__':
# 	Log = mLog()
# 	Log.d("hello")
