#!/bin/sh
arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 spokes.ino $@ --warnings all
