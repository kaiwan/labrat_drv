#!/bin/bash
set -euo pipefail
name=$(basename $0)

KDRV=dht2x_kdrv
i2cbus=1        # on I2C bus #1; update if required
chip_addr=0038  # chip addr is 0x38; update if required
intv_sec=2

detect_board()
{
  MODEL=$(cat /sys/firmware/devicetree/base/model 2>/dev/null) || true
  [[ -z "${MODEL}" ]] && {
    echo "Platform not supported; this driver is only currently supported on the Raspberry Pi, TI BBB and BeaglePlay" ; exit 1
  }
  set +e
  [[ "${MODEL}" = "TI AM335x BeagleBone Black" ]] && return 1 || true # it's a TI BBB!
  echo "${MODEL}" |grep "Raspberry Pi" >/dev/null && return 2 || true
  echo "${MODEL}" |grep "BeaglePlay" >/dev/null && return 3 || true
  return -1
}

# display_on_oled()
# Params:
#  $1 = temperature value
#  $2 = humidity value
display_on_oled()
{
local TEMP_FP=$(echo "scale=1; $1 / 1000" | bc)
local HUMD_FP=$(echo "scale=1; $2 / 1000" | bc)

echo -n "${TEMP_FP}C" > ${OLED_LARGE_ROW_TARGET}
sleep ${intv_sec}
echo -n "${HUMD_FP}%" > ${OLED_LARGE_ROW_TARGET}
}

# TODO :
#  [ ] soft-code the i2cbus # for the OLED display
setup_display_on_oled()
{
OLED_I2CBUS=3
OLED_SSD_ADDR=3c  # 0x3c
SYSFS_OLED_PFX=/sys/bus/i2c/devices/${OLED_I2CBUS}-00${OLED_SSD_ADDR}
OLED_LARGE_ROW_TARGET=${SYSFS_OLED_PFX}/write_largefont_rows2to6

[[ ! -f ${OLED_LARGE_ROW_TARGET} ]] && {
  echo "Couldn't get path to the OLED display's 'large' rows, aborting.."
  exit 1
} || true
ROW0=${SYSFS_OLED_PFX}/write_smallfont_to_row0
ROW1=${SYSFS_OLED_PFX}/write_smallfont_to_row1
ROW2=${SYSFS_OLED_PFX}/write_smallfont_to_row2
ROW3=${SYSFS_OLED_PFX}/write_smallfont_to_row3
ROW4=${SYSFS_OLED_PFX}/write_smallfont_to_row4
ROW5=${SYSFS_OLED_PFX}/write_smallfont_to_row5
ROW6=${SYSFS_OLED_PFX}/write_smallfont_to_row6
ROW7=${SYSFS_OLED_PFX}/write_smallfont_to_row7
oled_clrscr
echo 'Env Info Prj' > ${ROW0}
}

oled_clrscr()
{
local i row
for i in $(seq 0 7)
do
#eval "echo \${$myvar}"
        row=ROW${i}
        # place one variable within another! ref: https://unix.stackexchange.com/a/41409/55746
        eval "echo '' > \${$row}"
done
}


#--- 'main'

detect_board
ret=$?
if [[ ${ret} -eq -1 ]] ; then
   echo "!!! Unknown board, aborting [this driver is only currently supported on the Raspberry Pi and TI BBB]"
   exit 1
fi

if [[ ${ret} -eq 1 ]] ; then
   i2cbus=2 # on the TI BBB, the DTS specifies the I2C bus #2 as having the chip
   ln -sf Makefile.bbb Makefile  # setup the Makefile slink to point to the correct Makefile
   echo "+++ Detected we're running on the ${MODEL}"
elif [[ ${ret} -eq 2 ]] ; then
   i2cbus=1 # on the R Pi, the DTS specifies the I2C bus #1 as having the chip
   ln -sf Makefile.rpi Makefile  # setup the Makefile slink to point to the correct Makefile
   echo "+++ Detected we're running on the ${MODEL}"
elif [[ ${ret} -eq 3 ]] ; then
   i2cbus=1 # on the BeaglePlay via Grove, the DTS specifies the I2C bus #1 as having the chip
else
   echo "!!! Unknown board, aborting [this driver is only currently supported on the Raspberry Pi and TI BBB]"
   exit 1
fi
set -e

DISPLAY_ON_OLED=1

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
a) The current DTB (Device Tree Blob) being used includes the DHT2x sensor chip
   definition, OR
   The appropriate DT Overlay (.dtbo) is loaded into memory
b) The DHT2x sensor is correctly connected to the board
c) Are all the wires making contact properly?
d) If the DTB[O] has been modified, pl reboot and retry"
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

# Display Loop!
[[ ${DISPLAY_ON_OLED} -eq 1 ]] && setup_display_on_oled
echo "temperature(milliC),rel_humidity(milli%)"
while [ true ]
do
   temp=$(cat ${temp_file})
   humd=$(cat ${humd_file})
   echo "${temp},${humd}"
   [[ ${DISPLAY_ON_OLED} -eq 1 ]] && display_on_oled ${temp} ${humd}
   sleep ${intv_sec}
done
