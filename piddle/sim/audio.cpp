#include "audio.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void audioCallback(void* userdata, Uint8* stream, int len) {
    AudioState* s = static_cast<AudioState*>(userdata);
    uint32_t pos = s->pos.load(std::memory_order_relaxed);
    uint32_t remaining = (pos < s->len) ? (s->len - pos) : 0;
    uint32_t toCopy = (static_cast<uint32_t>(len) < remaining)
                          ? static_cast<uint32_t>(len) : remaining;
    if (toCopy > 0) {
        memcpy(stream, s->buf + pos, toCopy);
    }
    if (toCopy < static_cast<uint32_t>(len)) {
        memset(stream + toCopy, 0, len - toCopy);
    }
    s->pos.store(pos + toCopy, std::memory_order_relaxed);
}

// Decode any audio file to 44100 Hz, 16-bit signed, mono via ffmpeg.
static uint8_t* decodeWithFfmpeg(const char* path, uint32_t* outLen) {
    // Build command: ffmpeg converts to raw s16le PCM on stdout, suppressing logs.
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "ffmpeg -loglevel error -i \"%s\" -f s16le -ar 44100 -ac 1 -",
             path);

    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        fprintf(stderr, "popen(ffmpeg) failed\n");
        return nullptr;
    }

    std::vector<uint8_t> buf;
    uint8_t chunk[65536];
    size_t n;
    while ((n = fread(chunk, 1, sizeof(chunk), pipe)) > 0) {
        buf.insert(buf.end(), chunk, chunk + n);
    }

    int ret = pclose(pipe);
    if (ret != 0) {
        fprintf(stderr, "ffmpeg failed (exit %d) for: %s\n", ret, path);
        return nullptr;
    }

    if (buf.empty()) {
        fprintf(stderr, "ffmpeg produced no output for: %s\n", path);
        return nullptr;
    }

    uint8_t* out = static_cast<uint8_t*>(malloc(buf.size()));
    if (!out) {
        fprintf(stderr, "malloc failed\n");
        return nullptr;
    }
    memcpy(out, buf.data(), buf.size());
    *outLen = static_cast<uint32_t>(buf.size());
    return out;
}

bool initAudio(const char* audioFile, AudioState& state) {
    uint32_t len = 0;
    uint8_t* buf = decodeWithFfmpeg(audioFile, &len);
    if (!buf) return false;

    state.buf = buf;
    state.len = len;
    state.pos.store(0);

    state.spec = SDL_AudioSpec{};
    state.spec.freq     = 44100;
    state.spec.format   = AUDIO_S16SYS;
    state.spec.channels = 1;
    state.spec.samples  = 4096;
    state.spec.callback = audioCallback;
    state.spec.userdata = &state;

    SDL_AudioSpec obtained{};
    state.dev = SDL_OpenAudioDevice(nullptr, 0, &state.spec, &obtained, 0);
    if (state.dev == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        free(buf);
        return false;
    }

    SDL_PauseAudioDevice(state.dev, 0); // start playback
    return true;
}

void closeAudio(AudioState& state) {
    SDL_CloseAudioDevice(state.dev);
    free(state.buf);
    state.buf = nullptr;
}

uint32_t audioSamplePos(const AudioState& state) {
    // 16-bit mono: 2 bytes per sample
    return state.pos.load(std::memory_order_relaxed) / sizeof(int16_t);
}

const int16_t* audioSamples(const AudioState& state) {
    return reinterpret_cast<const int16_t*>(state.buf);
}

uint32_t audioTotalSamples(const AudioState& state) {
    return state.len / sizeof(int16_t);
}
