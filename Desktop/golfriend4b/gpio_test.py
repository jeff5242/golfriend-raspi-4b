import RPi.GPIO as GPIO
from time import sleep

GPIO.setmode(GPIO.BCM)
GPIO.setup(18, GPIO.OUT)

print("Relay ON")
GPIO.output(18, GPIO.HIGH)
sleep(0.1)

print("Relay OFF")
GPIO.output(18, GPIO.LOW)

GPIO.cleanup()
