#!/bin/sh
# Dump the serial connection from Arduino
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

echo '********** Ctrl+A, Ctrl+X to exit **********'
sleep 0.5
picocom $device --baud 115200 --echo --imap lfcrlf
