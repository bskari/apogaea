#include "audio.h"
#include "display.h"
#include "dsp.h"
#include "renderer.h"
#include "sim_constants.h"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

static void printHelp() {
    printf("Piddle Simulator\n");
    printf("Controls:\n");
    printf("  R         Toggle rainbow mode\n");
    printf("  N         Toggle normalize bands\n");
    printf("  Up/Down   Brightness +/- 5\n");
    printf("  Left/Right  Speed +/- 5\n");
    printf("  ,/.       Sensitivity +/- 5\n");
    printf("  [/]       Previous/Next audio file\n");
    printf("  -/=       Pattern length -/+ 5\n");
    printf("  Q/Escape  Quit\n");
    printf("\n");
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <audio_file> [audio_file2 ...]\n", argv[0]);
        return 1;
    }

    // Build playlist from all positional arguments
    int              fileCount   = argc - 1;
    char**           files       = argv + 1;
    int              fileIdx     = 0;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    AudioState audio{};

    DisplayState display{};
    if (!initDisplay(display)) {
        SDL_Quit();
        return 1;
    }

    DSPState* dsp = initDSP();

    printHelp();

    uint8_t brightness_p   = 100;
    uint8_t sensitivity_p  = 50;
    uint8_t speed_p        = 85;
    int     patternLength  = 20;
    bool    rainbow        = false;
    bool    normalizeBands = false;

    // LED state
    static CRGB leds[STRIP_COUNT][LEDS_PER_STRIP]{};
    memset(leds, 0, sizeof(leds));

    const int16_t* samples      = nullptr;
    uint32_t       totalSamples = 0;

    // Load starting at startIdx, advancing by step (+1 or -1) on failure.
    // Skips files that ffmpeg can't decode, returns false if none succeed.
    auto loadFile = [&](int startIdx, int step) -> bool {
        for (int idx = startIdx; idx >= 0 && idx < fileCount; idx += step) {
            closeAudio(audio);
            audio.len = 0;
            audio.pos.store(0);
            audio.dev = 0;
            if (initAudio(files[idx], audio)) {
                fileIdx      = idx;
                samples      = audioSamples(audio);
                totalSamples = audioTotalSamples(audio);
                printf("Playing [%d/%d]: %s\n", fileIdx + 1, fileCount, files[fileIdx]);
                return true;
            }
            fprintf(stderr, "Skipping (not audio?): %s\n", files[idx]);
        }
        return false;
    };

    if (!loadFile(0, 1)) {
        fprintf(stderr, "No playable audio files.\n");
        closeDisplay(display);
        SDL_Quit();
        return 1;
    }

    bool quit = false;
    while (!quit) {
        // --- Event handling ---
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                quit = true;
            } else if (ev.type == SDL_KEYDOWN) {
                switch (ev.key.keysym.sym) {
                    case SDLK_q:
                    case SDLK_ESCAPE: quit = true; break;
                    case SDLK_r:
                        rainbow = !rainbow;
                        printf("Rainbow: %s\n", rainbow ? "ON" : "OFF");
                        break;
                    case SDLK_n:
                        normalizeBands = !normalizeBands;
                        printf("Normalize bands: %s\n", normalizeBands ? "ON" : "OFF");
                        break;
                    case SDLK_UP:
                        brightness_p = static_cast<uint8_t>(
                            std::min(100, static_cast<int>(brightness_p) + 5));
                        printf("Brightness: %d%%\n", brightness_p);
                        break;
                    case SDLK_DOWN:
                        brightness_p = static_cast<uint8_t>(
                            std::max(0, static_cast<int>(brightness_p) - 5));
                        printf("Brightness: %d%%\n", brightness_p);
                        break;
                    case SDLK_RIGHT:
                        speed_p = static_cast<uint8_t>(
                            std::min(100, static_cast<int>(speed_p) + 5));
                        printf("Speed: %d\n", speed_p);
                        break;
                    case SDLK_LEFT:
                        speed_p = static_cast<uint8_t>(
                            std::max(0, static_cast<int>(speed_p) - 5));
                        printf("Speed: %d\n", speed_p);
                        break;
                    case SDLK_PERIOD:
                        sensitivity_p = static_cast<uint8_t>(
                            std::min(100, static_cast<int>(sensitivity_p) + 5));
                        printf("Sensitivity: %d\n", sensitivity_p);
                        break;
                    case SDLK_COMMA:
                        sensitivity_p = static_cast<uint8_t>(
                            std::max(0, static_cast<int>(sensitivity_p) - 5));
                        printf("Sensitivity: %d\n", sensitivity_p);
                        break;
                    case SDLK_RIGHTBRACKET:
                        if (fileIdx + 1 < fileCount) { if (!loadFile(fileIdx + 1, 1)) printf("No more playable files.\n"); }
                        else printf("Already at last file.\n");
                        break;
                    case SDLK_LEFTBRACKET:
                        if (fileIdx > 0) { if (!loadFile(fileIdx - 1, -1)) printf("No more playable files.\n"); }
                        else printf("Already at first file.\n");
                        break;
                    case SDLK_EQUALS:
                        patternLength = std::min(LEDS_PER_STRIP, patternLength + 5);
                        printf("Pattern length: %d\n", patternLength);
                        break;
                    case SDLK_MINUS:
                        patternLength = std::max(5, patternLength - 5);
                        printf("Pattern length: %d\n", patternLength);
                        break;
                    default: break;
                }
            }
        }

        // --- DSP ---
        uint32_t samplePos = audioSamplePos(audio);

        // Wait until we have enough samples buffered
        if (samplePos < SAMPLE_COUNT) {
            SDL_Delay(10);
            continue;
        }

        // Advance to next file when audio finishes, quit after the last one
        if (samplePos >= totalSamples) {
            if (fileIdx + 1 < fileCount) {
                if (!loadFile(fileIdx + 1, 1)) quit = true;
            } else {
                quit = true;
            }
            continue;
        }

        // Read the most recent SAMPLE_COUNT samples (no sign fix needed for WAV)
        const int16_t* window = samples + (samplePos - SAMPLE_COUNT);

        float noteValues[NOTE_COUNT];
        processDSP(dsp, window, sensitivity_p, noteValues);

        renderFft(leds, noteValues, rainbow, normalizeBands, SDL_GetTicks(), patternLength);

        // Apply brightness scale to a copy for display (don't modify the LED state)
        const float scale = static_cast<float>(brightness_p) / 100.0f;
        CRGB scaledLeds[STRIP_COUNT][LEDS_PER_STRIP];
        for (int i = 0; i < STRIP_COUNT; ++i) {
            for (int j = 0; j < LEDS_PER_STRIP; ++j) {
                scaledLeds[i][j] = CRGB(
                    static_cast<uint8_t>(leds[i][j].r * scale),
                    static_cast<uint8_t>(leds[i][j].g * scale),
                    static_cast<uint8_t>(leds[i][j].b * scale));
            }
        }

        drawFrame(display, scaledLeds);

        // Frame delay: matches ESP32 speed parameter (delay_ms = 100 - speed_p)
        const int delay_ms = 100 - speed_p;
        if (delay_ms > 0) SDL_Delay(delay_ms);
    }

    destroyDSP(dsp);
    closeDisplay(display);
    closeAudio(audio);
    SDL_Quit();
    return 0;
}
