#!/bin/sh
EXTRA_FLAGS='-DNUMSTRIPS=15 -DNUM_LEDS_PER_STRIP=151'
PASSTHROUGH=''

for arg in "$@"; do
    case "$arg" in
        --v2-1-pins) EXTRA_FLAGS="$EXTRA_FLAGS -DUSE_V2_1_PINS" ;;
        *)  PASSTHROUGH="$PASSTHROUGH $arg" ;;
    esac
done

arduino-cli compile \
    --build-property "compiler.cpp.extra_flags=$EXTRA_FLAGS" \
    --fqbn esp32:esp32:esp32da:PartitionScheme=huge_app piddle.ino $PASSTHROUGH --warnings all
