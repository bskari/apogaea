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

void slideDown(CRGB leds[STRIP_COUNT][LEDS_PER_STRIP], int count);

// millis: current time in milliseconds (for hue animation).
void renderFft(CRGB leds[STRIP_COUNT][LEDS_PER_STRIP],
               const float noteValues[NOTE_COUNT],
               bool rainbow, bool normalizeBands, uint32_t millis);
