#!/bin/sh
# ./upload.sh            flashes build/main (also writes bootloader + partition table; slot app0)
# ./upload.sh --artnet   flashes build/artnet into the second app slot (app1) without touching app0
# Run ./compile.sh for both images first. Flash main first on a fresh board (it writes the partitions).
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

if [ "$1" = "--artnet" ]; then
    shift
    # app1 offset from the min_spiffs partition table
    esptool=$(ls ~/.arduino15/packages/esp32/tools/esptool_py/*/esptool | tail -n 1)
    "$esptool" --chip esp32 --port "$device" --baud 460800 write-flash 0x1F0000 build/artnet/piddle.ino.bin "$@"
else
    arduino-cli upload -p $device --fqbn esp32:esp32:esp32da:PartitionScheme=min_spiffs --input-dir build/main piddle.ino $@
fi
