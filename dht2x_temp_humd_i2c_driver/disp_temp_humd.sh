#!/bin/bash
set -euo pipefail
name=$(basename $0)
drv=dht2x_kdrv
i2cbus=1        # on I2C bus #1; update if required
chip_addr=0038  # chip addr is 0x38; update if required
intv_sec=1

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
lsmod|grep -w "^${drv}" >/dev/null 2>&1
[ $? -ne 0 ] && {
  echo "${name}: driver ${drv} not loaded? aborting..."
  exit 1
}
set -e

humd_file=/sys/bus/i2c/devices/${i2cbus}-${chip_addr}/dht2x_humd
temp_file=/sys/bus/i2c/devices/${i2cbus}-${chip_addr}/dht2x_temp
[[ ! -f ${humd_file} ]] && {
  echo "humidity sysfs file not present? aborting..." ; exit 1
}
[[ ! -f ${temp_file} ]] && {
  echo "temeprature sysfs file not present? aborting..." ; exit 1
}

echo "temperature(milliC),rel_humidity(milli%)"
while [ true ]
do
   temp=$(cat ${temp_file})
   humd=$(cat ${humd_file})
   echo "${temp},${humd}"
   sleep ${intv_sec}
done
