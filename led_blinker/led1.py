#!/usr/bin/python
# A sample circuit:
#  https://www.makeuseof.com/tag/raspberry-pi-control-led/
from gpiozero import LED
from time import sleep

led = LED(18)

while True:
    print("+")
    led.on()
    sleep(1)
    print("-")
    led.off()
    sleep(1)
