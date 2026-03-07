// Rendering logic ported from spectrumAnalyzer.cpp.
// CHSV/CRGB arithmetic, hue cycling, and slideDown match the ESP32 exactly.

#include "renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

const CRGB CRGB::Black{0, 0, 0};

static uint8_t addSat(uint8_t a, uint8_t b) {
    int sum = static_cast<int>(a) + b;
    return sum > 255 ? 255 : static_cast<uint8_t>(sum);
}

CRGB& CRGB::operator+=(const CRGB& o) {
    r = addSat(r, o.r);
    g = addSat(g, o.g);
    b = addSat(b, o.b);
    return *this;
}

// Standard HSV->RGB with 0-255 hue/sat/val.
CRGB hsv2rgb(CHSV hsv) {
    if (hsv.s == 0) return CRGB(hsv.v, hsv.v, hsv.v);

    const uint8_t region    = hsv.h / 43;
    const uint8_t remainder = (hsv.h - region * 43) * 6;

    const uint8_t p = static_cast<uint8_t>((static_cast<uint16_t>(hsv.v) * (255 - hsv.s)) >> 8);
    const uint8_t q = static_cast<uint8_t>((static_cast<uint16_t>(hsv.v) *
                          (255 - ((static_cast<uint16_t>(hsv.s) * remainder) >> 8))) >> 8);
    const uint8_t t = static_cast<uint8_t>((static_cast<uint16_t>(hsv.v) *
                          (255 - ((static_cast<uint16_t>(hsv.s) * (255 - remainder)) >> 8))) >> 8);

    switch (region) {
        case 0:  return CRGB(hsv.v, t, p);
        case 1:  return CRGB(q, hsv.v, p);
        case 2:  return CRGB(p, hsv.v, t);
        case 3:  return CRGB(p, q, hsv.v);
        case 4:  return CRGB(t, p, hsv.v);
        default: return CRGB(hsv.v, p, q);
    }
}

void slideDown(CRGB leds[STRIP_COUNT][LEDS_PER_STRIP], int count) {
    const int byteCount = (LEDS_PER_STRIP - count) * sizeof(leds[0][0]);
    for (int i = 0; i < STRIP_COUNT; ++i) {
        memmove(&leds[i][count], &leds[i][0], byteCount);
    }
}

// quadwave8 approximation: maps 0-255 input to 0-255 sine output.
static uint8_t quadwave8(uint8_t x) {
    float s = sinf(x * 2.0f * (float)M_PI / 256.0f);
    return static_cast<uint8_t>(s * 127.5f + 127.5f);
}

void renderFft(CRGB leds[STRIP_COUNT][LEDS_PER_STRIP],
               const float noteValuesIn[NOTE_COUNT],
               bool rainbow, bool normalizeBands, uint32_t millis) {
    const int c4Index  = 11;
    const int startNote = c4Index - 4; // = 7

    slideDown(leds, SLIDE_COUNT);

    // Clear the head positions after slide
    for (int i = 0; i < STRIP_COUNT; ++i) {
        for (int j = 0; j < SLIDE_COUNT; ++j) {
            leds[i][j] = CRGB::Black;
        }
    }

    // Local mutable copy for normalizeBands
    float noteValues[NOTE_COUNT];
    for (int i = 0; i < NOTE_COUNT; ++i) noteValues[i] = noteValuesIn[i];

    static uint8_t rainbowOffset = 0;
    constexpr uint16_t hue16Step = 256 * 3;

    const uint8_t hueStart = [&]() -> uint8_t {
        if (rainbow) {
            return ++rainbowOffset;
        }
        const int quadWaveMillisDiv = 64;
        const int quadWaveDiv       = 8;
        return static_cast<uint8_t>(quadwave8(millis / quadWaveMillisDiv) / quadWaveDiv - 20);
    }();

    if (normalizeBands) {
        float allMaxValue = noteValues[0];
        for (int i = 0; i < STRIP_COUNT * 3; ++i) {
            allMaxValue = std::max(allMaxValue, noteValues[i]);
        }
        for (int range = 0; range < 3; ++range) {
            const int start = startNote + STRIP_COUNT * range;
            const int end   = start + STRIP_COUNT;
            float maxValue  = noteValues[start];
            for (int note = start + 1; note < end; ++note) {
                maxValue = std::max(maxValue, noteValues[note]);
            }
            const float inverse = 1.0f / ((maxValue + allMaxValue) * 0.5f);
            for (int note = start; note < end; ++note) {
                noteValues[note] *= inverse;
            }
        }
    }

    uint16_t hue16 = static_cast<uint16_t>(hueStart) * 256;
    int strip = 0;
    for (int note = startNote; note < NOTE_COUNT - 1; ++note) {
        const float floatValue    = noteValues[note];
        const uint8_t intValue    = static_cast<uint8_t>(floatValue * 254);
        const uint8_t gammaCorrected = static_cast<uint8_t>(
            static_cast<uint16_t>(intValue) * intValue / 255);
        const uint8_t hue = static_cast<uint8_t>(hue16 >> 8);
        hue16 += hue16Step;

        CRGB color = hsv2rgb(CHSV(hue, 255, gammaCorrected));
        for (int i = 0; i < SLIDE_COUNT; ++i) {
            leds[strip][i] += color;
        }

        ++strip;
        if (strip >= STRIP_COUNT) {
            strip = 0;
            // Same uint16_t wraparound as ESP32. 65536/3 - 768*15*2 = -1195 → 64341 as uint16_t.
            const uint16_t step = static_cast<uint16_t>(
                65536 / 3 - (int)hue16Step * STRIP_COUNT * 2);
            hue16 += step;
        }
    }
}
