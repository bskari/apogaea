# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Piddle is an audio-reactive LED display system built on ESP32. It captures audio via an I2S INMP441 microphone, performs real-time FFT spectrum analysis, and visualizes sound frequencies as flowing colors on WS2812B LED strips.

## Build Commands

```bash
./compile.sh                    # Compile with Arduino CLI
./upload.sh                     # Upload to /dev/ttyUSB0
./serial.sh                     # Monitor serial output at 115200 baud
```

Direct Arduino CLI commands:
```bash
arduino-cli compile --fqbn esp32:esp32:esp32da piddle.ino --warnings all
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32da piddle.ino
```

## Utility Scripts

```bash
python3 steps.py <bucket_count> <frequency>  # Generate FFT bucket-to-note mappings
python3 power_sim.py                          # Simulate power consumption
```

## Architecture

**Dual-Core FreeRTOS Design:**
- **Core 1:** `collectSamplesFunction` - Continuous I2S microphone sampling into circular buffer
- **Core 0:** `displayLedsFunction` - FFT computation, visualization rendering, LED updates (every 20ms)

**Key Files:**
- `piddle.ino` - Entry point, FreeRTOS task setup
- `spectrumAnalyzer.cpp/hpp` - FFT processing, A-weighting, Hann windowing, visualization logic
- `esp32-fft.cpp/hpp` - Radix-2 FFT implementation
- `constants.hpp` - Hardware pin definitions, LED configuration
- `I2SClocklessLedDriver/` - Custom ESP32 I2S-based LED driver library

**DSP Pipeline:**
1. I2S sample collection (44.1 kHz mono)
2. Hann windowing
3. 2048-point real FFT
4. A-weighting (perceptual frequency compensation)
5. Musical note mapping (E1-B7, 39 notes)
6. Gamma-corrected LED color rendering with hue cycling

## Hardware Configuration

- **Board:** ESP32 WROOM 32 (`esp32:esp32:esp32da`)
- **LED strips:** Up to 15 strips, 151 LEDs per strip (pins defined in constants.hpp)
- **I2S Microphone:** SCK=19, WS=22, SD=21
- **Brightness button:** GPIO 0 (boot button), cycles through 5 levels

## PCB Design

KiCad 9.0 project files in `pcb/` directory. Manufacturing files in `pcb/gerber/`.
