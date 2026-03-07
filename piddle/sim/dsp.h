#pragma once

#include "sim_constants.h"
#include "../esp32-fft.hpp"
#include <cstdint>

struct DSPState {
    float            input[SAMPLE_COUNT];
    float            output[SAMPLE_COUNT];
    float            weightingConstants[SAMPLE_COUNT];
    float            windowingConstants[SAMPLE_COUNT];
    fft_config_t*    fftPlan;
};

DSPState* initDSP();
void destroyDSP(DSPState* s);

// Feed SAMPLE_COUNT samples starting at 'samples'. sensitivity_p is 0..100.
// Fills noteValuesOut[NOTE_COUNT] with values in [0..1].
void processDSP(DSPState* s, const int16_t* samples, uint8_t sensitivity_p,
                float noteValuesOut[NOTE_COUNT]);
