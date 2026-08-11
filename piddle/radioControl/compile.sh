#!/bin/sh
# We need this when we include constants.hpp
for arg in "$@"; do
    case "$arg" in
        --v2-1-pins) EXTRA_FLAGS="$EXTRA_FLAGS -DUSE_V2_1_PINS"; PINS_SET=1 ;;
        --v2-2-pins) EXTRA_FLAGS="$EXTRA_FLAGS -DUSE_V2_2_PINS"; PINS_SET=1 ;;
        *)  PASSTHROUGH="$PASSTHROUGH $arg" ;;
    esac
done

arduino-cli compile \
    --build-property "compiler.cpp.extra_flags=$EXTRA_FLAGS" \
    --fqbn esp32:esp32:esp32da:PartitionScheme=huge_app radioControl.ino --warnings all
