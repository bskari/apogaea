#pragma once

#include <SDL2/SDL.h>
#include <atomic>
#include <cstdint>

struct AudioState {
    uint8_t*                buf;   // raw PCM bytes (malloc'd, freed by closeAudio)
    uint32_t                len;   // total bytes
    std::atomic<uint32_t>   pos;   // current playback position in bytes
    SDL_AudioSpec           spec;
    SDL_AudioDeviceID       dev;
};

// Returns false and prints an error on failure.
bool initAudio(const char* wavFile, AudioState& state);
void closeAudio(AudioState& state);

// Current playback position in samples.
uint32_t audioSamplePos(const AudioState& state);

// Pointer to the entire sample buffer cast to int16_t.
const int16_t* audioSamples(const AudioState& state);

uint32_t audioTotalSamples(const AudioState& state);
