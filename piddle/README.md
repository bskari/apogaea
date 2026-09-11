Piddle
======

Sound reactive LED strips, originating from a central point and placed in a
geodesic dome. Running on a custom PCB with ESP32.

Wiring
------

- LED pins: see source
- Microphone
  - SCK: 19
  - WS: 22
  - SD: 21
  - L/R: ground
  - VDD: 3.3V
  - GND: ground

Hardware
--------

- ESP32 WROOM 32
- I2S microphone
- 15 WS2812B LED strips

Installation
------------
    arduino-cli core install esp32:esp32
    arduino-cli install fastled@3.7.8
    cd ~/Arduino/libraries
    git clone https://github.com/pschatzmann/ESP32-A2DP.git
    git clone https://github.com/pschatzmann/arduino-audio-tools.git
