# Simulator

Just simulates the LEDs. Give it some files with audio.

## Compile

    cd sim
    mkdir -p build && cd build
    cmake ..
    make

## Controls

- C: Cycle color palette (default, rainbow, pastel, fire, ocean, sunset)
- N: Toggle normalize bands
- up/down: change brightness
- Left/Right: Speed +/- 5
- ,/.: Sensitivity +/- 5
- [/]: Previous/Next audio file
- H: Print help
- Escape: Quit
