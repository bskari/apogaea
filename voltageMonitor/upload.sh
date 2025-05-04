#!/bin/sh
if [ -e /dev/ttyUSB0 ] ;
then
    arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:makergo_c3_supermini voltageMonitor.ino $@
elif [ -e /dev/ttyACM0 ] ;
then
    arduino-cli upload -p /dev/ttyACM0 --fqbn esp32:esp32:makergo_c3_supermini voltageMonitor.ino $@
else
    echo 'No port found'
    exit 1
fi
