#!/bin/sh
arduino-cli compile \
    --build-property 'compiler.cpp.extra_flags=-DNUMSTRIPS=15 -DNUM_LEDS_PER_STRIP=151' \
    --fqbn esp32:esp32:esp32da piddle.ino $@ --warnings all
