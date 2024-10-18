#!/bin/bash
set -euo pipefail
name=$(basename $0)

KDRV=dht2x_kdrv
i2cbus=1        # on I2C bus #1; update if required
chip_addr=0038  # chip addr is 0x38; update if required
intv_sec=1

detect_board()
{
  MODEL=$(cat /sys/firmware/devicetree/base/model 2>/dev/null)
  set +e
  [[ "${MODEL}" = "TI AM335x BeagleBone Black" ]] && return 1 || true # it's a TI BBB!
  echo "${MODEL}" |grep "Raspberry Pi" >/dev/null && return 2 || true
}


#--- 'main'

detect_board
ret=$?
if [[ ${ret} -eq 1 ]] ; then
   i2cbus=2 # on the TI BBB, the DTS specifies the I2C bus #2 as having the chip
   ln -sf Makefile.bbb Makefile  # setup the Makefile slink to point to the correct Makefile
   echo "+++ Detected we're running on the ${MODEL}"
elif [[ ${ret} -eq 2 ]] ; then
   i2cbus=1 # on the R Pi, the DTS specifies the I2C bus #1 as having the chip
   ln -sf Makefile.rpi Makefile  # setup the Makefile slink to point to the correct Makefile
   echo "+++ Detected we're running on the ${MODEL}"
else
   echo "!!! Unknown board, aborting"
   exit 1
fi
set -e

# ${VARNAME:-DEFAULT_VALUE} evals to DEFAULT_VALUE if VARNAME undefined
p1=${1:-}
[ "$p1" = "-h" ] && {
   echo "Usage: ${name} [refresh-interval-in-seconds]"
   echo "Displays:
   temperature in millidegrees Celsius,rel humidity in milli percentage
   (you need to div both values by 1000.0)"
   exit 0
}
[ $# -eq 1 ] && intv_sec=$p1

set +e
lsmod|grep -w "^${KDRV}" >/dev/null 2>&1
[ $? -ne 0 ] && {
  echo "${name}: loading driver ${KDRV} now..."
  [[ ! -f ${KDRV}.ko ]] && make || true
  sudo insmod ${KDRV}.ko || {
	  echo "insmod failed!" ;  exit 1
  } && true
}
set -e

FAIL_MSG="
Please ensure that:
a) The DHT2x sensor is correctly connected to the device
b) Are all the wires making contact properly?
c) If the DTB[O] has been modified, pl reboot and retry"
humd_file=/sys/bus/i2c/devices/${i2cbus}-${chip_addr}/dht2x_humd
temp_file=/sys/bus/i2c/devices/${i2cbus}-${chip_addr}/dht2x_temp
[[ ! -f ${humd_file} ]] && {
  echo "humidity sysfs file not present? aborting..."
  echo "${FAIL_MSG}"; exit 1
}
[[ ! -f ${temp_file} ]] && {
  echo "temperature sysfs file not present? aborting..."
  echo "${FAIL_MSG}"; exit 1
}

echo "temperature(milliC),rel_humidity(milli%)"
while [ true ]
do
   temp=$(cat ${temp_file})
   humd=$(cat ${humd_file})
   echo "${temp},${humd}"
   sleep ${intv_sec}
done
