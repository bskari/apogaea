#!/bin/sh
EXTRA_FLAGS='-DNUMSTRIPS=15 -DNUM_LEDS_PER_STRIP=151'
PASSTHROUGH=''
PINS_SET=0
IMAGE=main

# --v2-1-pins is the default, it's the board I have as of now

for arg in "$@"; do
    case "$arg" in
        --v2-1-pins) EXTRA_FLAGS="$EXTRA_FLAGS -DUSE_V2_1_PINS"; PINS_SET=1 ;;
        --v2-2-pins) EXTRA_FLAGS="$EXTRA_FLAGS -DUSE_V2_2_PINS"; PINS_SET=1 ;;
        --artnet) EXTRA_FLAGS="$EXTRA_FLAGS -DARTNET_IMAGE"; IMAGE=artnet ;;
        *)  PASSTHROUGH="$PASSTHROUGH $arg" ;;
    esac
done

if [ "$PINS_SET" -eq 0 ]; then
    echo "Error: must specify either --v2-1-pins or --v2-2-pins" >&2
    exit 1
fi

# Two firmware images share the flash (two OTA app slots), see ARTNET_IMAGE in piddle.ino:
#   ./compile.sh --v2-1-pins            -> build/main   (Bluetooth audio + BLE config)
#   ./compile.sh --v2-1-pins --artnet   -> build/artnet (ArtNet over WiFi)
arduino-cli compile \
    --build-property "compiler.cpp.extra_flags=$EXTRA_FLAGS" \
    --build-path "$PWD/build/$IMAGE/tmp" --output-dir "$PWD/build/$IMAGE" \
    --fqbn esp32:esp32:esp32da:PartitionScheme=min_spiffs piddle.ino $PASSTHROUGH --warnings all
