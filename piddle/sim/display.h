#pragma once

#include "renderer.h"
#include <SDL2/SDL.h>

struct DisplayState {
    SDL_Window*   window;
    SDL_Renderer* renderer;
};

bool initDisplay(DisplayState& d);
void closeDisplay(DisplayState& d);
void drawFrame(DisplayState& d, const CRGB leds[STRIP_COUNT][LEDS_PER_STRIP]);
