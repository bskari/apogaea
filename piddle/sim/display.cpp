#include "display.h"

#include <cmath>
#include <cstdio>

// Window dimensions and dot layout
static const int WINDOW_SIZE   = 750;
static const int CENTER        = WINDOW_SIZE / 2;
static const int START_RADIUS  = 20;   // pixels from center to first dot
static const int DOT_SPACING   = 10;   // pixels between dot centers
static const int DOT_RADIUS    = 4;    // pixels

static void drawFilledCircle(SDL_Renderer* r, int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius; dy++) {
        int dx = static_cast<int>(sqrtf(static_cast<float>(radius * radius - dy * dy)));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

bool initDisplay(DisplayState& d) {
    d.window = SDL_CreateWindow(
        "Piddle Simulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_SIZE, WINDOW_SIZE,
        SDL_WINDOW_SHOWN);
    if (!d.window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }
    d.renderer = SDL_CreateRenderer(d.window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!d.renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(d.window);
        return false;
    }
    return true;
}

void closeDisplay(DisplayState& d) {
    SDL_DestroyRenderer(d.renderer);
    SDL_DestroyWindow(d.window);
}

void drawFrame(DisplayState& d, const CRGB leds[STRIP_COUNT][LEDS_PER_STRIP]) {
    SDL_SetRenderDrawColor(d.renderer, 0, 0, 0, 255);
    SDL_RenderClear(d.renderer);

    // Draw a small gray circle for the center device
    SDL_SetRenderDrawColor(d.renderer, 40, 40, 40, 255);
    drawFilledCircle(d.renderer, CENTER, CENTER, START_RADIUS - 8);

    for (int strip = 0; strip < STRIP_COUNT; ++strip) {
        // Strips at equal angular spacing, starting at 90° (top), going clockwise
        const float angleDeg = 90.0f - strip * (360.0f / STRIP_COUNT);
        const float angleRad = angleDeg * (float)M_PI / 180.0f;
        const float cosA = cosf(angleRad);
        const float sinA = sinf(angleRad);

        for (int led = 0; led < LEDS_PER_STRIP; ++led) {
            const CRGB& c = leds[strip][led];
            // Skip fully black dots for speed
            if (c.r == 0 && c.g == 0 && c.b == 0) continue;

            const float radius = START_RADIUS + led * DOT_SPACING;
            const int x = CENTER + static_cast<int>(radius * cosA);
            const int y = CENTER - static_cast<int>(radius * sinA);

            SDL_SetRenderDrawColor(d.renderer, c.r, c.g, c.b, 255);
            drawFilledCircle(d.renderer, x, y, DOT_RADIUS);
        }
    }

    SDL_RenderPresent(d.renderer);
}
