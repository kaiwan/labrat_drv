#!/bin/bash
name=$(basename $0)
i2cbus=1        # on I2C bus #1; update if required
chip_addr=0038  # chip addr is 0x38; update if required
intv_sec=1

[ "$1" = "-h" ] && {
   echo "Usage: ${name} [refresh-interval-in-seconds]"
   echo "Displays:
   temperature in millidegrees Celsius,rel humidity in milli percentage
   (you need to div both values by 1000.0)"
   exit 0
}
[ $# -eq 1 ] && intv_sec=$1

humd_file=/sys/bus/i2c/devices/${i2cbus}-${chip_addr}/dht2x_humd
temp_file=/sys/bus/i2c/devices/${i2cbus}-${chip_addr}/dht2x_temp
while [ true ]
do
   temp=$(cat ${temp_file})
   humd=$(cat ${humd_file})
   echo "${temp},${humd}"
   sleep ${intv_sec}
done
