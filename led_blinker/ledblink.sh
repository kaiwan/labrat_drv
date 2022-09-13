#!/bin/bash
# A sample circuit:
#  https://www.makeuseof.com/tag/raspberry-pi-control-led/
name=$(basename $0)
# Run 'pinout'; the LED's attached to GPIO physical pin # 12, BCM GPIO
# GPIO18.
LED_GPIO=18
GPIODIR=/sys/class/gpio
GPIO=${GPIODIR}/gpio${LED_GPIO}

[ $(id -u) -ne 0 ] && {
	echo "${name}: requires root."
	exit 1
}
[ ! -d ${GPIO} ] && echo ${LED_GPIO} > ${GPIODIR}/export
[ ! -d ${GPIO} ] && {
	echo "${name}: creating/exporting gpio dir failed."
	exit 1
}

# 'Signal handler' to run on exit, ^C (SIGINT) or ^\ (SIGQUIT)
trap \
   'echo 18 > ${GPIODIR}/unexport ; exit 0' \
   EXIT INT QUIT

echo out > ${GPIO}/direction

while [ true ]
do
	echo 1 > ${GPIO}/value
	sleep 1
	echo 0 > ${GPIO}/value
	sleep 1
done
exit 0
