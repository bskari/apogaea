#!/bin/sh
arduino-cli compile --fqbn esp32:esp32:makergo_c3_supermini voltageMonitor.ino $@ --warnings all
