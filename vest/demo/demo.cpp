#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_timer.h>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <time.h>

using std::max;
using std::min;

#define PROGMEM

#include "../constants.hpp"

const int WIDTH = 800;
const int HEIGHT = 480;

static SDL_Renderer *renderer = nullptr;
static bool fullVest = false;
static uint8_t singleHue = 0;

int16_t sin16(uint16_t theta);
uint8_t sin8(uint8_t theta);
uint8_t sqrt16(uint16_t value);
int16_t cos16(uint16_t theta);
uint16_t rand16();
void setLed(const int index, uint32_t color, SDL_Renderer *const renderer);
void setLed(int x, int y, uint8_t hue, SDL_Renderer *renderer);
void setLed(int x, int y, uint8_t red, uint8_t green, uint8_t blue,
            SDL_Renderer *renderer);
void setLedHue(int x, int y, uint8_t v, SDL_Renderer *renderer);
void setLedGrayscale(int x, int y, uint8_t v, SDL_Renderer *renderer);
void setLedSingleHue(int x, int y, uint8_t v, SDL_Renderer *renderer);
void setLedDoubleGrayscale(int x, int y, uint8_t v, SDL_Renderer *renderer);
void setLedPastel(int x, int y, uint8_t v, SDL_Renderer *renderer);
void setLedFire(int x, int y, uint8_t v, SDL_Renderer *renderer);
void setLedGreen(int x, int y, uint8_t v, SDL_Renderer *renderer);
void setLedSharpGreen(int x, int y, uint8_t v, SDL_Renderer *renderer);
void hsvToRgb(uint8_t hue, uint8_t saturation, uint8_t value, uint8_t *red,
              uint8_t *green, uint8_t *blue);
typedef void(setLed_t(int, int, uint8_t, SDL_Renderer *));

#define MAX(a, b) ((a) > (b) ? (a) : (b))
const float M_PI_F = 3.14159265358979;

// https://lodev.org/cgtutor/plasma.html
uint8_t lodevP1(const int time, const int x, int) {
  return 128 + (sin16(x * (PI_16_1_0 / 4) + time / 4)) / 256;
}

uint8_t lodevP2(const int time, int, const int y) {
  return 128 + (sin16(y * (PI_16_1_0 / 8) + time / 8)) / 256;
}

uint8_t lodevP3(const int time, const int x, const int y) {
  return 128 + (sin16((x + y) * (PI_16_1_0 / 8) + time / 2)) / 256;
}

uint8_t lodevP4(const int time, const int x, const int y) {
  const int x2 = x - X_CENTER;
  const int y2 = y - Y_CENTER;
  return 128 + (sin16(sqrt16(x2 * x2 + y2 * y2) * 1000 + time)) / 256;
}

void plasma1(int time, setLed_t setLed, SDL_Renderer *const renderer) {
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      const uint8_t p1 = lodevP1(time, x, y);
      const uint8_t p2 = lodevP2(time, x, y);
      const uint8_t p3 = lodevP3(time, x, y);
      const uint8_t p4 = lodevP4(time, x, y);
      const uint8_t v = (p1 + p2 + p3 + p4) / 4;
      setLed(x, y, v, renderer);
    }
  }
}

void plasma2(int time, setLed_t setLed, SDL_Renderer *const renderer) {
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      const int xOffset = (x - LED_COLUMN_COUNT) / 2;
      const int yOffset = (y - LED_ROW_COUNT) / 2;
      const uint8_t v = static_cast<uint8_t>(
          (128 + (sin16(x * (PI_16_1_0 / 16))) / 256 + 128 +
           (sin16(y * (PI_16_1_0 / 32))) / 256 + 128 +
           (sin16(sqrt16((xOffset * xOffset) + (yOffset * yOffset)) * 1000)) /
               256 +
           128 + (sin16(sqrt16(x * x + y * y) * 1000 + time)) / 256) /
          4);
      setLed(x, y, v, renderer);
    }
  }
}

void plasma3(int time, setLed_t setLed, SDL_Renderer *const renderer) {
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      const uint8_t p1 = 128 + (sin16(x * (PI_16_1_0 / 8) + time / 4)) / 256;
      const uint8_t p2 =
          128 +
          sin16(10 * (x * sin16(time / 2) / 256 + y * cos16(time / 3) / 256)) /
              256;
      const uint8_t p3 =
          128 + (sin16((x + y) * (PI_16_1_0 / 8) + time / 8)) / 256;
      // cx = x + 0.5 * sin(time / 5)
      // cy = y + 0.5 * cos(time / 3)
      // v = sin(sqrt(100 * (cx**2 + cy ** 2) + 1) + time)
      const uint16_t cx = x + sin16(time / 8) / 1024;
      const uint16_t cy = y + sin16(time / 16) / 1024;
      const uint8_t p4 =
          128 + sin16(sqrt16(cx * cx + cy * cy) * (PI_16_1_0 / 2) + time) / 512;
      const uint8_t v = (p1 + p2 + p3 + p4) / 4;
      setLed(x, y, v, renderer);
    }
  }
}

void plasma4(int time, setLed_t setLed, SDL_Renderer *const renderer) {
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      const uint8_t p1 = 128 + (sin16(x * (PI_16_1_0 / 4) + time)) / 256;
      const uint8_t p2 =
          128 +
          sin16(10 * (x * sin16(time / 3) / 256 + y * cos16(time / 4) / 256)) /
              256;
      // cx = x + 0.5 * sin(time / 5)
      // cy = y + 0.5 * cos(time / 3)
      // v = sin(sqrt(100 * (cx**2 + cy ** 2) + 1) + time)
      const uint16_t cx = x + sin16(time / 8) / 1024;
      const uint16_t cy = y + sin16(time / 16) / 1024;
      const uint8_t p3 =
          128 + sin16(sqrt16(cx * cx + cy * cy) * (PI_16_1_0 / 8) + time) / 512;
      const uint8_t hue = (p1 + p2 + p3) / 3;
      setLed(x, y, hue, renderer);
    }
  }
}

void p1only(int time, setLed_t setLed, SDL_Renderer *const renderer) {
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      const uint8_t hue = lodevP1(time, x, y);
      setLed(x, y, hue, renderer);
    }
  }
}

void p2only(int time, setLed_t setLed, SDL_Renderer *const renderer) {
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      const uint8_t hue = lodevP2(time, x, y);
      setLed(x, y, hue, renderer);
    }
  }
}

void p3only(int time, setLed_t setLed, SDL_Renderer *const renderer) {
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      const uint8_t hue = lodevP3(time, x, y);
      setLed(x, y, hue, renderer);
    }
  }
}

void p4only(int time, setLed_t setLed, SDL_Renderer *const renderer) {
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      const uint8_t hue = lodevP4(time, x, y);
      setLed(x, y, hue, renderer);
    }
  }
}

void p4onlyFloat(int time_, setLed_t setLed, SDL_Renderer *const renderer) {
  // Oops, I think this is doing v3 of Bidoulle
  float time = static_cast<float>(time_) / 10000;
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      float cx = 0.5 * (x - X_CENTER) + 0.5 * sinf(time / 5);
      float cy = 0.5 * (y - Y_CENTER) + 0.5 * cosf(time / 3);
      float v = sinf(sqrtf(1 * (cx * cx + cy * cy) + 1) + time);
      const uint8_t hue = (v + 1) * 127;
      setLed(x, y, hue, renderer);
    }
  }
}

void ringsOnly(int time, setLed_t setLed, SDL_Renderer *const renderer) {
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      // cx = x + 0.5 * sin(time / 5)
      // cy = y + 0.5 * cos(time / 3)
      // v = sin(sqrt(100 * (cx**2 + cy ** 2) + 1) + time)
      const uint16_t cx = x + sin16(time / 4) / 2048;
      const uint16_t cy = y + sin16(time / 3) / 2048;
      const uint8_t p4 =
          128 +
          sin16(sqrt16(cx * cx + cy * cy) * (PI_16_1_0 / 8) + time / 2) / 256;
      const uint8_t hue = p4;
      setLed(x, y, hue, renderer);
    }
  }
}

static uint8_t fires[LED_COLUMN_COUNT][LED_ROW_COUNT] = {{0}};
void initializeFire() {
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    fires[x][0] = 255;
  }
}

void fireEffect(setLed_t setLed, SDL_Renderer *const renderer) {
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = LED_ROW_COUNT - 1; y > 0; --y) {
      const int rand = rand16();
      int offset = rand % 3 - 1;
      if ((x == 0 && offset == -1) || (x == LED_ROW_COUNT - 1 && offset == 1)) {
        offset = 0;
      }
      fires[x][y] =
          MAX(static_cast<int>(fires[x + offset][y - 1]) - rand / 1024, 0);
    }
  }
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      setLed(x, y, fires[x][y], renderer);
    }
  }
}

void floatSpiral(int time, setLed_t setLed, SDL_Renderer *const renderer) {
  float timeOffset = static_cast<float>(time) * 0.0001f;
  const float distanceMultiplier = 0.25f;
  for (float distance = 0.0f; distance < 15.0f; distance += 0.5f) {
    for (float theta = 0.0f; theta < 2 * M_PI_F; theta += M_PI_F * 0.01f) {
      const float x =
          sinf(theta + timeOffset) * distance + X_CENTER;
      const float y = cosf(theta + timeOffset) * distance + Y_CENTER;
      const uint8_t hue =
          (theta + distance * distanceMultiplier) / (2.0f * M_PI_F) * 255;
      setLed(static_cast<int>(x), static_cast<int>(y), hue, renderer);
    }
  }
}

void basicSpiral(int time, setLed_t setLed, SDL_Renderer *const renderer) {
  // From guess and check, 250 is about as large as this can get before I start
  // seeing unset LEDs. If I didn't clear the LEDs between each iteration, then
  // the previous ones would carry over. They would be slightly off, but it
  // probably wouldn't be noticeable.
  const uint16_t thetaStep = 250;

  // From guess and check, 17 is needed so that all the LEDs are set
  const int maxDistance = 17;

  // From manual testing, this is what we want. Lower values leave holes,
  // larger ones need longer maxDistance.
  const int divisor = 32768;

  const int distanceMultiplier = 6000;

  for (int distance = 0; distance < maxDistance; ++distance) {
    for (uint16_t theta = 0;
         theta < std::numeric_limits<decltype(theta)>::max() - 2 * thetaStep;
         theta += thetaStep) {
      const int16_t x =
          static_cast<int>(sin16(theta + time)) * distance / divisor +
          X_CENTER;
      const int16_t y =
          static_cast<int>(cos16(theta + time)) * distance / divisor +
          Y_CENTER;
      const uint8_t hue = (theta + distance * distanceMultiplier) / 255;
      setLed(x, y, hue, renderer);
    }
  }
}

void basicSpiralReversed(int time, setLed_t setLed,
                         SDL_Renderer *const renderer) {
  basicSpiral(-time, setLed, renderer);
}

void throbbingSpiral(int time, setLed_t setLed, SDL_Renderer *const renderer) {
  // From guess and check, 250 is about as large as this can get before I start
  // seeing unset LEDs. If I didn't clear the LEDs between each iteration, then
  // the previous ones would carry over. They would be slightly off, but it
  // probably wouldn't be noticeable.
  const uint16_t thetaStep = 250;

  // From guess and check, 17 is needed so that all the LEDs are set
  const int maxDistance = 17;

  // From manual testing, this is what we want. Lower values leave holes,
  // larger ones need longer maxDistance.
  const int divisor = 32768;

  const int throbDivisor = 32;

  // From guess and check, 17 is needed so that all the LEDs are set
  for (uint16_t distance = 0; distance < maxDistance; ++distance) {
    for (uint16_t theta = 0;
         theta < std::numeric_limits<decltype(theta)>::max() - 2 * thetaStep;
         theta += thetaStep) {
      const int16_t x =
          static_cast<int>(sin16(theta + time)) * distance / divisor +
          X_CENTER;
      const int16_t y =
          static_cast<int>(cos16(theta + time)) * distance / divisor +
          Y_CENTER;
      const uint8_t hue = (theta + distance * sin16(time) / throbDivisor) / 255;
      setLed(static_cast<int>(x), static_cast<int>(y), hue, renderer);
    }
  }
}

void testPalette(setLed_t setLed, SDL_Renderer *const renderer) {
  static int start = 0;
  uint8_t v = start;
  int count = 0;
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      if (v == 0 && count < 3) {
        setLed(x, y, v + 128, renderer);
        ++count;
      } else {
        setLed(x, y, v, renderer);
        ++v;
      }
    }
  }
  ++start;
}

void wideVideo(const uint32_t frames[][28 * 12], const int frameCount,
               const int millisPerFrame, SDL_Renderer *const renderer) {
  const int width = 28;
  const int height = 12;
  const int startColumn = 0;

  static int frame = 0;

  int count = 0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const uint32_t value = frames[frame][count];
      ++count;
      const uint8_t red = (value & 0xFF0000) >> 16;
      const uint8_t green = (value & 0xFF00) >> 8;
      const uint8_t blue = value & 0xFF;
      setLed(x + startColumn, height - y, red, green, blue, renderer);
    }
  }
  frame = (frame + 1) % frameCount;

  timespec delayTime;
  delayTime.tv_sec = 0;
  delayTime.tv_nsec = millisPerFrame * 1000 * 1000 - 1000 * 1000 / 60;
  nanosleep(&delayTime, NULL);
}

void centeredVideo(const uint32_t frames[][12 * 11], const int frameCount,
                   const int millisPerFrame, SDL_Renderer *const renderer) {
  const int width = 12;
  const int height = 11;
  const int startColumn = 8;

  static int frame = 0;

  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = LED_ROW_COUNT - 1; y > 0; --y) {
      setLed(x, y, 100, 0, 0, renderer);
    }
  }

  int count = 0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const uint32_t value = frames[frame][count];
      ++count;
      const uint8_t red = (value & 0xFF0000) >> 16;
      const uint8_t green = (value & 0xFF00) >> 8;
      const uint8_t blue = value & 0xFF;
      setLed(x + startColumn, height - y, red, green, blue, renderer);
    }
  }
  frame = (frame + 1) % frameCount;

  timespec delayTime;
  delayTime.tv_sec = 0;
  delayTime.tv_nsec = millisPerFrame * 1000 * 1000 - 1000 * 1000 / 60;
  nanosleep(&delayTime, NULL);
}

const char *plasma1hue(int time, SDL_Renderer *const renderer) {
  plasma1(time, setLedHue, renderer);
  return __func__;
}

const char *plasma1pastel(int time, SDL_Renderer *const renderer) {
  plasma1(time, setLedPastel, renderer);
  return __func__;
}

const char *plasma1fire(int time, SDL_Renderer *const renderer) {
  plasma1(time, setLedFire, renderer);
  return __func__;
}

const char* plasma1green(int time, SDL_Renderer *const renderer) {
  plasma1(time, setLedGreen, renderer);
  return __func__;
}

const char* plasma1sharpGreen(int time, SDL_Renderer *const renderer) {
  plasma1(time, setLedSharpGreen, renderer);
  return __func__;
}

const char *plasma2hue(int time, SDL_Renderer *const renderer) {
  plasma2(time, setLedHue, renderer);
  return __func__;
}

const char *plasma2pastel(int time, SDL_Renderer *const renderer) {
  plasma2(time, setLedPastel, renderer);
  return __func__;
}

const char *plasma2fire(int time, SDL_Renderer *const renderer) {
  plasma2(time, setLedFire, renderer);
  return __func__;
}

const char *plasma3hue(int time, SDL_Renderer *const renderer) {
  plasma3(time, setLedHue, renderer);
  return __func__;
}

const char *plasma3pastel(int time, SDL_Renderer *const renderer) {
  plasma3(time, setLedPastel, renderer);
  return __func__;
}

const char *plasma3fire(int time, SDL_Renderer *const renderer) {
  plasma3(time, setLedFire, renderer);
  return __func__;
}

const char *plasma4hue(int time, SDL_Renderer *const renderer) {
  plasma4(time, setLedHue, renderer);
  return __func__;
}

const char *plasma4pastel(int time, SDL_Renderer *const renderer) {
  plasma4(time, setLedPastel, renderer);
  return __func__;
}

const char *plasma4fire(int time, SDL_Renderer *const renderer) {
  plasma4(time, setLedFire, renderer);
  return __func__;
}

const char *p1onlyHue(int time, SDL_Renderer *const renderer) {
  p1only(time, setLedHue, renderer);
  return __func__;
}

const char *p1onlyPastel(int time, SDL_Renderer *const renderer) {
  p1only(time, setLedPastel, renderer);
  return __func__;
}

const char *p2onlyHue(int time, SDL_Renderer *const renderer) {
  p2only(time, setLedHue, renderer);
  return __func__;
}

const char *p2onlyPastel(int time, SDL_Renderer *const renderer) {
  p2only(time, setLedPastel, renderer);
  return __func__;
}

const char *p3onlyHue(int time, SDL_Renderer *const renderer) {
  p3only(time, setLedHue, renderer);
  return __func__;
}

const char *p3onlyPastel(int time, SDL_Renderer *const renderer) {
  p3only(time, setLedPastel, renderer);
  return __func__;
}

const char *p4onlyHue(int time, SDL_Renderer *const renderer) {
  p4only(time, setLedHue, renderer);
  return __func__;
}

const char *p4onlyPastel(int time, SDL_Renderer *const renderer) {
  p4only(time, setLedPastel, renderer);
  return __func__;
}

const char *ringsOnlyHue(int time, SDL_Renderer *const renderer) {
  ringsOnly(time, setLedHue, renderer);
  return __func__;
}

const char *ringsOnlyPastel(int time, SDL_Renderer *const renderer) {
  ringsOnly(time, setLedPastel, renderer);
  return __func__;
}

const char *fire(int, SDL_Renderer *const renderer) {
  fireEffect(setLedFire, renderer);
  return __func__;
}

const char *testHue(int, SDL_Renderer *const renderer) {
  testPalette(setLedHue, renderer);
  return __func__;
}

const char *testPastel(int, SDL_Renderer *const renderer) {
  testPalette(setLedPastel, renderer);
  return __func__;
}

const char *testFire(int, SDL_Renderer *const renderer) {
  testPalette(setLedFire, renderer);
  return __func__;
}

const char *spiralHueFloat(int time, SDL_Renderer *const renderer) {
  floatSpiral(time, setLedHue, renderer);
  return __func__;
}

const char *basicSpiralHue(int time, SDL_Renderer *const renderer) {
  basicSpiral(time, setLedHue, renderer);
  return __func__;
}

const char *basicSpiralReversedHue(int time, SDL_Renderer *const renderer) {
  basicSpiralReversed(time, setLedHue, renderer);
  return __func__;
}

const char *basicSpiralGrayscale(int time, SDL_Renderer *const renderer) {
  basicSpiral(time, setLedGrayscale, renderer);
  return __func__;
}

const char *basicSpiralSingleHue(int time, SDL_Renderer *const renderer) {
  basicSpiral(time, setLedSingleHue, renderer);
  ++singleHue;
  return __func__;
}

const char *basicSpiralReversedGrayscale(int time,
                                         SDL_Renderer *const renderer) {
  basicSpiralReversed(time, setLedGrayscale, renderer);
  return __func__;
}

const char *basicSpiralDoubleGrayscale(int time, SDL_Renderer *const renderer) {
  basicSpiral(time, setLedDoubleGrayscale, renderer);
  return __func__;
}

const char *basicSpiralReversedDoubleGrayscale(int time,
                                               SDL_Renderer *const renderer) {
  basicSpiralReversed(time, setLedDoubleGrayscale, renderer);
  return __func__;
}

const char *throbbingSpiralHue(int time, SDL_Renderer *const renderer) {
  throbbingSpiral(time, setLedHue, renderer);
  return __func__;
}

const char *plasmaBidoulle(int, SDL_Renderer *const renderer) {
  const float multiplier = 0.15f;
  const float timeIncrement = 0.1f;

  static float time = 0.0f;

  time += timeIncrement;
  float cys[LED_ROW_COUNT];
  for (int y = 0; y < LED_ROW_COUNT; ++y) {
    cys[y] = y + 0.5f * cosf(time * (1.0f / 3.0f));
  }
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    const float v1 = sinf(multiplier * x + time);
    const float cx = sinf(x + 0.5 * sinf(time * 0.2f));
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      const float v2 = sinf(multiplier * (x * sinf(time * 0.5f) +
                                          y * cosf(time * (1.0f / 3.0f))) +
                            time);
      const float v3 =
          sinf(multiplier * sqrtf((cx * cx + cys[y] * cys[y] + 1.0f)) + time);
      const float v = v1 + v2 + v3;
      const float red = sinf(v * M_PI_F);
      const float green = sinf(v * M_PI_F + 2.0 * M_PI_F / 3.0);
      const float blue = sinf(v * M_PI_F + 4.0 * M_PI_F / 3.0);
      const uint8_t r = 128 + red * 127;
      const uint8_t g = 128 + green * 127;
      const uint8_t b = 128 + blue * 127;
      setLed(x, y, r, g, b, renderer);
    }
  }
  return __func__;
}

const char *plasmaBidoulleFast(int time, SDL_Renderer *const renderer) {
  const uint16_t multiplier = 0.15f * PI_16_1_0;
  // I found that using precomputed tables for the circles makes the animation
  // jittery, so blend two adjacent values so smooth it out
  const int blend = 64;
  const int xSin = sin16(time / 2);
  const int xOffset = xSin / 4096;
  const int xRemainder = (xSin % 4096) / blend;
  const int ySin = sin16(time / 3);
  const int yOffset = ySin / 8192;
  // I haven't noticed the jitter in the y direction, so don't compute the
  // blend for y

  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    const int16_t v1 = sin16(multiplier * x + time); // good
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      const int16_t v2 =
          sin16((multiplier * (x * sin16(time / 2) / 2 + y * cos16(time / 3)) +
                 time) /
                16384); // bad values, but looks good?
      const int adjustedX = x + xOffset + X_CENTER;
      const int adjustedY = y + yOffset + Y_CENTER + 1;
      const uint16_t blend1 =
          bidoulleV3rings[adjustedX][adjustedY] * (blend - xRemainder) / blend;
      const uint16_t blend2 =
          bidoulleV3rings[adjustedX + 1][adjustedY] * xRemainder / blend;
      const uint16_t v3 = (blend1 + blend2) / 2 * 256;
      const int16_t v = v1 + v2 + v3;
      const uint8_t red = (sin16(v) / 256) + 128;
      const int16_t green = (sin16(v + 2 * 32768 / 3) / 256) + 128;
      const int16_t blue = (sin16(v + 4 * 32768 / 3) / 256) + 128;
      setLed(x, y, red, green, blue, renderer);
    }
  }
  return __func__;
}

const char *plasmaBidoulleFastChristmas(int time,
                                        SDL_Renderer *const renderer) {
  const uint16_t multiplier = 0.15f * PI_16_1_0;
  // I found that using precomputed tables for the circles makes the animation
  // jittery, so blend two adjacent values so smooth it out
  const int blend = 64;
  const int xSin = sin16(time / 2);
  const int xOffset = xSin / 4096;
  const int xRemainder = (xSin % 4096) / blend;
  const int ySin = sin16(time / 3);
  const int yOffset = ySin / 8192;
  // I haven't noticed the jitter in the y direction, so don't compute the
  // blend for y

  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    const int16_t v1 = sin16(multiplier * x + time); // good
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      const int16_t v2 =
          sin16((multiplier * (x * sin16(time / 2) / 2 + y * cos16(time / 3)) +
                 time) /
                16384); // bad values, but looks good?
      const int adjustedX = x + xOffset + X_CENTER;
      const int adjustedY = y + yOffset + Y_CENTER + 1;
      const uint16_t blend1 =
          bidoulleV3rings[adjustedX][adjustedY] * (blend - xRemainder) / blend;
      const uint16_t blend2 =
          bidoulleV3rings[adjustedX + 1][adjustedY] * xRemainder / blend;
      const uint16_t v3 = (blend1 + blend2) / 2 * 256;
      const int16_t v = v1 + v2 + v3;
      uint8_t red = (sin16(v) / 256) + 128;
      if (red < 128) {
        red = 0;
      }
      int16_t green = (sin16(v + 2 * 32768 / 3) / 256) + 128;
      if (green < 128 || red >= 128) {
        green = 0;
      }
      // const int16_t blue = (sin16(v + 4 * 32768 / 3) / 256) + 128;
      int16_t blue = 0;
      if (red == 0 && green == 0 && blue == 0) {
        red = green = blue = 255;
      }
      setLed(x, y, red, green, blue, renderer);
    }
  }
  return __func__;
}

const char *plasmaBidoulleFastChangingColors(int time,
                                             SDL_Renderer *const renderer) {
  const uint16_t multiplier = 0.15f * PI_16_1_0;
  // I found that using precomputed tables for the circles makes the animation
  // jittery, so blend two adjacent values so smooth it out
  const int blend = 64;
  const int xSin = sin16(time / 2);
  const int xOffset = xSin / 4096;
  const int xRemainder = (xSin % 4096) / blend;
  const int ySin = sin16(time / 3);
  const int yOffset = ySin / 8192;
  // I haven't noticed the jitter in the y direction, so don't compute the
  // blend for y

  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    const int16_t v1 = sin16(multiplier * x + time); // good
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      const int16_t v2 =
          sin16((multiplier * (x * sin16(time / 2) / 2 + y * cos16(time / 3)) +
                 time) /
                16384); // bad values, but looks good?
      const int adjustedX = x + xOffset + X_CENTER;
      const int adjustedY = y + yOffset + Y_CENTER + 1;
      const uint16_t blend1 =
          bidoulleV3rings[adjustedX][adjustedY] * (blend - xRemainder) / blend;
      const uint16_t blend2 =
          bidoulleV3rings[adjustedX + 1][adjustedY] * xRemainder / blend;
      const uint16_t v3 = (blend1 + blend2) / 2 * 256;
      const int16_t v = v1 + v2 + v3;
      const uint16_t redOffset =
          sin16(static_cast<int>(time + x * 8000) / 13) + 32768;
      const uint8_t red = (sin16(v + redOffset) / 256) + 128;
      const uint16_t greenOffset =
          sin16(static_cast<int>(time + x * 8000) / 7) + 32768;
      const int16_t green = (sin16(v + greenOffset) / 256) + 128;
      const uint16_t blueOffset = sin16(static_cast<int>(time) / 11) + 32768;
      const int16_t blue = (sin16(v + blueOffset) / 256) + 128;
      setLed(x, y, red, green, blue, renderer);
    }
  }
  return __func__;
}

void diamondColors(int time, setLed_t setLed, SDL_Renderer *const renderer) {
  static uint8_t start = 0;
  uint8_t timeOffset = sin16(time >> 2) >> 10;
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      uint8_t offset = start + abs(X_CENTER - x) * 4 +
                       abs(Y_CENTER - y) * 2 + timeOffset;
      setLed(x, y, offset, renderer);
    }
  }
  --start;
}

const char *diamondColorsHue(int time, SDL_Renderer *const renderer) {
  diamondColors(time, setLedHue, renderer);
  return __func__;
}

const char *diamondColorsPastel(int time, SDL_Renderer *const renderer) {
  diamondColors(time, setLedPastel, renderer);
  return __func__;
}

void pointyColors(setLed_t setLed, SDL_Renderer *const renderer) {
  static uint8_t start = 0;
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      uint8_t offset =
          start + (abs(X_CENTER - x) - abs(y - LED_ROW_COUNT)) * 4;
      setLed(x, y, offset, renderer);
    }
  }
  --start;
}

const char *pointyColorsHue(int, SDL_Renderer *const renderer) {
  pointyColors(setLedHue, renderer);
  return __func__;
}

const char *pointyColorsPastel(int, SDL_Renderer *const renderer) {
  pointyColors(setLedPastel, renderer);
  return __func__;
}

const char *horizontalComets(int, SDL_Renderer *const renderer) {
  const int delayStart = 6;
  const int length = 8;

  static uint8_t delay = delayStart;
  const int init = LED_COLUMN_COUNT + length + 1;
  static int16_t cometPositions[LED_ROW_COUNT] = {
      init, init, init, init, init, init, init, init, init, init, init, init};
  static bool left[LED_ROW_COUNT] = {0};
  static uint8_t hues[LED_ROW_COUNT] = {0};
  static uint8_t hue = 0;

  --delay;
  if (delay == 0) {
    bool set = false;
    int index = rand() % COUNT_OF(cometPositions);
    // Just give up after 10 rolls
    for (int i = 0; i < 10; ++i) {
      if (cometPositions[index] < -length ||
          cometPositions[index] >= LED_COLUMN_COUNT + length) {
        set = true;
        break;
      }
      index = rand() % COUNT_OF(cometPositions);
    }

    if (set) {
      if (rand() % 2 == 0) {
        cometPositions[index] = LED_COLUMN_COUNT - 1;
        left[index] = true;
      } else {
        cometPositions[index] = 0;
        left[index] = false;
      }
      hues[index] = hue;
      hue += 45;
    }

    delay = delayStart;
  }

  uint8_t red, green, blue;
  for (int index = 0; index < COUNT_OF(cometPositions); ++index) {
    const auto start = cometPositions[index];
    uint8_t brightness = 255;
    if (left[index]) {
      for (int j = 0; j < length; ++j) {
        hsvToRgb(hues[index], 255, brightness, &red, &green, &blue);
        setLed(start + j, index, red, green, blue, renderer);
        brightness -= 30;
      }
      --cometPositions[index];
    } else {
      for (int j = 0; j < length; ++j) {
        hsvToRgb(hues[index], 255, brightness, &red, &green, &blue);
        setLed(start - j, index, red, green, blue, renderer);
        brightness -= 30;
      }
      ++cometPositions[index];
    }
  }

  return __func__;
}

int main() {
  bool shouldClose = false;
  uint8_t hue = 0;
  int time = 0;
  int animationIndex = 0;
  const char *(*animations[])(int, SDL_Renderer *) = {
      plasma1green,
      plasma1sharpGreen,
      horizontalComets,
      basicSpiralSingleHue,
      diamondColorsHue,
      diamondColorsPastel,
      pointyColorsHue,
      pointyColorsPastel,
      plasmaBidoulleFastChristmas,
      basicSpiralHue,
      basicSpiralReversedHue,
      basicSpiralGrayscale,
      basicSpiralReversedGrayscale,
      basicSpiralDoubleGrayscale,
      basicSpiralReversedDoubleGrayscale,
      throbbingSpiralHue,
      spiralHueFloat,
      plasmaBidoulle,
      plasmaBidoulleFast,
      plasmaBidoulleFastChangingColors,
      p1onlyHue,
      p2onlyHue,
      p3onlyHue,
      p4onlyHue,
      plasma1hue,
      plasma2hue,
      plasma3hue,
      plasma4hue,
      plasma1fire,
      plasma2fire,
      plasma3fire,
      plasma4fire,
      plasma1pastel,
      plasma2pastel,
      plasma3pastel,
      plasma4pastel,
      ringsOnlyHue,
      ringsOnlyPastel,
      fire,
      testHue,
      testPastel,
      testFire,
  };

  initializeFire();
  // Returns zero on success else non-zero
  if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
    printf("error initializing SDL: %s\n", SDL_GetError());
  }
  SDL_Window *window = SDL_CreateWindow(
      "vest", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  SDL_SetWindowTitle(window, animations[0](time, renderer));

  // Animation loop
  while (!shouldClose) {
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    animations[animationIndex](time, renderer);
    int delay_ms = 100;
    time += 1000;
    ++hue;
    SDL_RenderPresent(renderer);

    while (delay_ms > 0) {
      // 60 FPS
      SDL_Delay(1000 / 60);
      delay_ms -= 60;

      SDL_Event event;
      // Events management
      const char *name;
      while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
          shouldClose = true;
          goto exitDelay;

        case SDL_KEYDOWN:
          if (event.key.keysym.sym == SDLK_RIGHT) {
            goto nextAnimation;
          } else if (event.key.keysym.sym == SDLK_LEFT) {
            goto previousAnimation;
          } else if (event.key.keysym.sym == SDLK_SPACE ||
                     event.key.keysym.sym == SDLK_f) {
            fullVest = !fullVest;
          }
          break;

        case SDL_MOUSEBUTTONDOWN:
          if (event.button.button == SDL_BUTTON_LEFT) {
            goto nextAnimation;
          } else if (event.button.button == SDL_BUTTON_RIGHT) {
            goto previousAnimation;
          }
          break;

        default:
          break;

        nextAnimation:
          animationIndex = (animationIndex + 1) % COUNT_OF(animations);
          name = animations[animationIndex](time, renderer);
          goto setTitle;

        previousAnimation:
          --animationIndex;
          if (animationIndex < 0) {
            animationIndex = COUNT_OF(animations) - 1;
          }
          name = animations[animationIndex](time, renderer);
          goto setTitle;

        setTitle:
          SDL_SetWindowTitle(window, name);
        }
      }
    }
  }
exitDelay:

  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}

int16_t sin16(uint16_t theta) {
  static const uint16_t base[] = {0,     6393,  12539, 18204,
                                  23170, 27245, 30273, 32137};
  static const uint8_t slope[] = {49, 48, 44, 38, 31, 23, 14, 4};

  uint16_t offset = (theta & 0x3FFF) >> 3; // 0..2047
  if (theta & 0x4000) {
    offset = 2047 - offset;
  }

  uint8_t section = offset / 256; // 0..7
  uint16_t b = base[section];
  uint8_t m = slope[section];

  uint8_t secoffset8 = static_cast<uint8_t>(offset) / 2;

  uint16_t mx = m * secoffset8;
  int16_t y = mx + b;

  if (theta & 0x8000) {
    y = -y;
  }

  return y;
}

uint8_t sqrt16(const uint16_t value) {
  uint8_t x = 0;
  if (value > 65536 / 3) {
    return 255;
  }
  while (x * x < value) {
    ++x;
  }
  return x;
}

int16_t cos16(uint16_t theta) { return sin16(theta + 65536 / 4); }

uint16_t rand16() {
  static uint16_t seed = 231;
  seed = seed * 2053 + 13489;
  return seed;
}

const uint8_t b_m16_interleave[] = {0, 49, 49, 41, 90, 27, 117, 10};
uint8_t sin8(uint8_t theta) {
  uint8_t offset = theta;
  if (theta & 0x40) {
    offset = (uint8_t)255 - offset;
  }
  offset &= 0x3F; // 0..63

  uint8_t secoffset = offset & 0x0F; // 0..15
  if (theta & 0x40)
    secoffset++;

  uint8_t section = offset >> 4; // 0..3
  uint8_t s2 = section * 2;
  const uint8_t *p = b_m16_interleave;
  p += s2;
  uint8_t b = *p;
  p++;
  uint8_t m16 = *p;

  uint8_t mx = (m16 * secoffset) >> 4;

  int8_t y = mx + b;
  if (theta & 0x80)
    y = -y;

  y += 128;

  return y;
}

/**
 * Sets the color to a hue at full saturation and value.
 */
void setLedHue(const int x, const int y, const uint8_t hue,
               SDL_Renderer *const renderer) {
  uint8_t red, green, blue;
  hsvToRgb(hue, 255, 255, &red, &green, &blue);
  setLed(x, y, red, green, blue, renderer);
}

void setLedGrayscale(const int x, const int y, const uint8_t brightness,
                     SDL_Renderer *const renderer) {
  const uint8_t value = sin8(brightness);
  setLed(x, y, value, value, value, renderer);
}

/**
 * Sets the color to a slowly iterating hue, using the passed in value as as
 * brightness.
 */
void setLedSingleHue(const int x, const int y, const uint8_t v,
                     SDL_Renderer *const renderer) {
  const uint8_t brightness = sin8(v);
  uint8_t red, green, blue;
  hsvToRgb(singleHue, 255, brightness, &red, &green, &blue);
  setLed(x, y, red, green, blue, renderer);
}

void setLedDoubleGrayscale(const int x, const int y, const uint8_t v,
                           SDL_Renderer *const renderer) {
  const uint8_t value = sin8(v * 2);
  setLed(x, y, value, value, value, renderer);
}

void setLedPastel(const int x, const int y, const uint8_t v,
                  SDL_Renderer *const renderer) {
  const uint8_t red = sin8(v);
  const uint8_t green = sin8(v + 2 * v / 3);
  const uint8_t blue = sin8(v + 4 * v / 3);
  setLed(x, y, red, green, blue, renderer);
}

void setLedFire(int x, int y, uint8_t v, SDL_Renderer *renderer) {
  const uint8_t red = v < 128 ? v * 2 : 255;
  const uint8_t green = v >= 128 ? (v - 128) * 2 : 0;
  setLed(x, y, red, green, 0, renderer);
}

void setLedGreen(int x, int y, uint8_t v, SDL_Renderer *renderer) {
  // 0-0, 1-1, 2-2, ... 7-7, 8-7, 9-6, 10-5 ... 15-0, 16-1, 17-2, ...
  const uint8_t part = (v & 0x3F);
  const uint8_t green = ((part < 32) ? part : 63 - part) * 2;
  setLed(x, y, 0, green, 0, renderer);
}

void setLedSharpGreen(int x, int y, uint8_t v, SDL_Renderer *renderer) {
  constexpr uint8_t partial = 0b1000000;
  constexpr uint8_t half = partial / 2;
  constexpr uint8_t ander = partial - 1;
  const uint8_t part = (v & ander);
  const uint8_t green = part < half ? 0 : (part - half) * (256 / partial);
  setLed(x, y, 0, green, 0, renderer);
}

void setLed(const int index, uint32_t color, SDL_Renderer *const renderer) {
  SDL_Rect rectangle;
  const int multiplier = 25;
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      const int ind = XY_TO_OFFSET[x][y];
      if (ind == index) {
        rectangle.x = x * multiplier;
        rectangle.y = y * multiplier;
        rectangle.w = multiplier;
        rectangle.h = multiplier;
        uint8_t red = 0;   // color >> 16;
        uint8_t green = 0; //(color & 0x00FF00) >> 8;
        uint8_t blue = color & 0x0000FF;
        SDL_SetRenderDrawColor(renderer, red, green, blue, 255);
        SDL_RenderFillRect(renderer, &rectangle);
        break;
      }
    }
  }
}

void setLed(const int x, const int y, const uint8_t red, const uint8_t green,
            const uint8_t blue, SDL_Renderer *const renderer) {
  SDL_Rect rectangle;
  const int multiplier = 25;
  if (x >= 0 && x < LED_COLUMN_COUNT) {
    if (y >= 0 && y < LED_ROW_COUNT) {
      const int index = XY_TO_OFFSET[x][y];
      if (fullVest || index != UNUSED_LED) {
        // The ys are inverted
        rectangle.x = x * multiplier;
        rectangle.y = LED_ROW_COUNT * multiplier - y * multiplier;
        rectangle.w = multiplier;
        rectangle.h = multiplier;
        SDL_SetRenderDrawColor(renderer, red, green, blue, 255);
        SDL_RenderFillRect(renderer, &rectangle);
      }
    }
  }
}

void hsvToRgb(const uint8_t hue, const uint8_t saturation, const uint8_t value,
              uint8_t *const red, uint8_t *const green, uint8_t *const blue) {
  unsigned char region, remainder, p, q, t;

  if (saturation == 0) {
    *red = value;
    *green = value;
    *blue = value;
    return;
  }

  region = hue / 43;
  remainder = (hue - (region * 43)) * 6;

  p = (value * (255 - saturation)) >> 8;
  q = (value * (255 - ((saturation * remainder) >> 8))) >> 8;
  t = (value * (255 - ((saturation * (255 - remainder)) >> 8))) >> 8;

  switch (region) {
  case 0:
    *red = value;
    *green = t;
    *blue = p;
    break;
  case 1:
    *red = q;
    *green = value;
    *blue = p;
    break;
  case 2:
    *red = p;
    *green = value;
    *blue = t;
    break;
  case 3:
    *red = p;
    *green = q;
    *blue = value;
    break;
  case 4:
    *red = t;
    *green = p;
    *blue = value;
    break;
  default:
    *red = value;
    *green = p;
    *blue = q;
    break;
  }
}
