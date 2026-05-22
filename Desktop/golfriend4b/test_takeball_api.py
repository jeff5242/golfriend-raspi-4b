
import time
import datetime
import hashlib
import requests
import json

device_code = "TEST_1001"
transaction_code = "8831"
currentTime = str(int(time.mktime(datetime.datetime.now().timetuple())))

sn = hashlib.sha1(("salt" + currentTime + transaction_code + device_code).encode('utf-8')).hexdigest()


payload = {
    'device_code': device_code,
    'transaction_code': transaction_code,
    'time': currentTime,
    'sn': sn
}

print("Sending payload:")
print(json.dumps(payload, indent=4))




try:
    response = requests.post("http://golfpoint.milkidea.com/api/take_ball", payload)
    # response = requests.post("http://golfpoint.milkidea.com/api/take_ball", data=payload)
    print(f"Status code: {response.status_code}")
    print("Response body:")
    print(response.text)
except Exception as e:
    print("[ERROR] Exception during POST:", e)
