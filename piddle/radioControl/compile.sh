#!/bin/sh
arduino-cli compile \
    --fqbn esp32:esp32:esp32da:PartitionScheme=huge_app radioControl.ino --warnings all
