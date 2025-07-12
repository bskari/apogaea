#!/bin/sh
if [ -e /dev/ttyUSB0 ] ;
then
    device=/dev/ttyUSB0
elif [ -e /dev/ttyACM0 ] ;
then
    device=/dev/ttyACM0
else
    echo 'Device not found'
    exit 1
fi

arduino-cli upload -p $device --fqbn esp32:esp32:esp32da piddle.ino $@
