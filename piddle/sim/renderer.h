#pragma once

#include "sim_constants.h"
#include <cstdint>

struct CRGB {
    uint8_t r, g, b;
    CRGB() : r(0), g(0), b(0) {}
    CRGB(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
    CRGB& operator+=(const CRGB& o);
    static const CRGB Black;
};

struct CHSV {
    uint8_t h, s, v;
    CHSV(uint8_t h, uint8_t s, uint8_t v) : h(h), s(s), v(v) {}
};

CRGB hsv2rgb(CHSV hsv);

// Color palettes, cycled with the C key. PALETTE_DEFAULT is the original
// quadwave-animated hue; the rest scroll through a palette lookup.
enum Palette : uint8_t {
    PALETTE_DEFAULT,
    PALETTE_RAINBOW,
    PALETTE_PASTEL,
    PALETTE_FIRE,
    PALETTE_OCEAN,
    PALETTE_SUNSET,
    PALETTE_COUNT
};

const char* paletteName(Palette p);

// millis: current time in milliseconds (for hue animation).
// patternLength: number of LEDs per repeating tile (5..LEDS_PER_STRIP).
// tileOffset: history positions each successive tile shifts (0 = identical copies).
void renderFft(CRGB leds[STRIP_COUNT][LEDS_PER_STRIP],
               const float noteValues[NOTE_COUNT],
               Palette palette, bool normalizeBands, uint32_t millis, int patternLength, int tileOffset);
