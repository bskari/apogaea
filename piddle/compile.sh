#!/bin/sh
EXTRA_FLAGS='-DNUMSTRIPS=15 -DNUM_LEDS_PER_STRIP=151'
PASSTHROUGH=''
MODE_SET=0

for arg in "$@"; do
    case "$arg" in
        -b) EXTRA_FLAGS="$EXTRA_FLAGS -DUSE_BLUETOOTH"; MODE_SET=1 ;;
        -a) EXTRA_FLAGS="$EXTRA_FLAGS -DUSE_ARTNET"; MODE_SET=1 ;;
        --v2-1-pins) EXTRA_FLAGS="$EXTRA_FLAGS -DUSE_V2_1_PINS"; PIN=1 ;;
        *)  PASSTHROUGH="$PASSTHROUGH $arg" ;;
    esac
done

if [ "$MODE_SET" -eq 0 ]; then
    echo "Error: specify -b (Bluetooth) and/or -a (ArtNet)" >&2
    exit 1
fi

arduino-cli compile \
    --build-property "compiler.cpp.extra_flags=$EXTRA_FLAGS" \
    --fqbn esp32:esp32:esp32da:PartitionScheme=huge_app piddle.ino $PASSTHROUGH --warnings all
