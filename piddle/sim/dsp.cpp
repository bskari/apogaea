// DSP pipeline ported from spectrumAnalyzer.cpp.
// All arithmetic is identical to the ESP32 version.

#include "dsp.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>

// Generated from python3 steps.py 2048 44100.0 -s
static const uint16_t NOTE_TO_OUTPUT_INDEX[NOTE_COUNT] = {
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 24, 25, 27, 28, 30, 32, 34, 36, 38, 40, 43, 45, 48, 51, 54, 57,
    61, 64, 68, 72, 77, 81, 86, 91, 97, 102, 109, 115, 122, 129, 137, 145,
    154, 163, 173, 183
};

static constexpr float square(float f) { return f * f; }

static float aWeightingMultiplier(float frequency) {
    const float freq_2 = square(frequency);
    const float denom1 = freq_2 + square(20.6f);
    const float denom2 = sqrtf((freq_2 + square(107.7f)) * (freq_2 + square(737.9f)));
    const float denom3 = freq_2 + square(12194.0f);
    const float denom  = denom1 * denom2 * denom3;
    const float enumer = square(freq_2) * square(12194.0f);
    const float ra     = enumer / denom;
    const float aWeighting_db = 2.0f + 20.0f * logf(ra) / logf(10.0f);
    return powf(10.0f, aWeighting_db / 10.0f);
}

static float windowingMultiplier(int offset) {
    const float a0 = 0.53836f;
    return a0 - (1.0f - a0) * cosf(2.0f * (float)M_PI * offset / SAMPLE_COUNT);
}

static void powerOfTwo(float* array, int length) {
    for (int i = 0; i < length / 2; ++i) {
        array[i] = array[i * 2] * array[i * 2] + array[i * 2 + 1] * array[i * 2 + 1];
    }
}

static void normalizeTo0_1(float samples[], int length, float minimumDivisor) {
    float minSample = samples[0];
    for (int i = 1; i < length; ++i) minSample = std::min(minSample, samples[i]);
    for (int i = 0; i < length; ++i) samples[i] -= minSample;

    float maxSample = samples[0];
    for (int i = 1; i < length; ++i) maxSample = std::max(maxSample, samples[i]);

    const float divisor   = std::max(maxSample, minimumDivisor);
    const float multiplier = 1.0f / divisor;
    for (int i = 0; i < length; ++i) samples[i] *= multiplier;
}

static float maxOutputForNote(const float* output, int note) {
    if (note >= NOTE_COUNT - 1) {
        return output[NOTE_TO_OUTPUT_INDEX[NOTE_COUNT - 1]];
    }
    float maxOutput = output[NOTE_TO_OUTPUT_INDEX[note]];
    for (int i = NOTE_TO_OUTPUT_INDEX[note]; i < NOTE_TO_OUTPUT_INDEX[note + 1]; ++i) {
        maxOutput = std::max(maxOutput, output[i]);
    }
    return maxOutput;
}

DSPState* initDSP() {
    DSPState* s = new DSPState{};
    s->fftPlan = fft_init(SAMPLE_COUNT, FFT_REAL, FFT_FORWARD, s->input, s->output);
    assert(s->fftPlan);

    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        s->windowingConstants[i] = windowingMultiplier(i);
    }
    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        const float freq = static_cast<float>(SAMPLE_RATE) / SAMPLE_COUNT * (i + 1);
        s->weightingConstants[i] = aWeightingMultiplier(freq);
    }
    return s;
}

void destroyDSP(DSPState* s) {
    if (s) {
        fft_destroy(s->fftPlan);
        delete s;
    }
}

void processDSP(DSPState* s, const int16_t* samples, uint8_t sensitivity_p,
                float noteValuesOut[NOTE_COUNT]) {
    // Copy and window
    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        s->input[i] = static_cast<float>(samples[i]) * s->windowingConstants[i];
    }

    rfft(s->input, s->output, s->fftPlan->twiddle_factors, SAMPLE_COUNT);

    s->output[0] = 0.0f;
    s->output[1] = 0.0f;
    s->output[SAMPLE_COUNT - 1] = 0.0f;

    powerOfTwo(s->output, SAMPLE_COUNT);

    for (int i = 0; i < SAMPLE_COUNT; ++i) {
        s->output[i] *= s->weightingConstants[i];
    }

    const float minimumDivisor = square((130 - sensitivity_p) * 6000.0f);
    normalizeTo0_1(s->output, SAMPLE_COUNT, minimumDivisor);

    for (int note = 0; note < NOTE_COUNT; ++note) {
        noteValuesOut[note] = maxOutputForNote(s->output, note);
    }
}
