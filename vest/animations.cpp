#include <FastLED.h>
#include <limits>
#include <stdint.h>

#include "animations.hpp"
#include "constants.hpp"

extern CRGB leds[];

using std::fill;

CRGB ColorGenerator::getColor(const uint8_t v) {
  return CHSV(v, 255, 255);
}

CRGB ColorGenerator::getColor(const uint16_t v) {
  CRGB crgb;
  hsv2rgb_raw(CHSV(v / 256, 255, 255), crgb);
  return crgb;
}

CRGB RedGreenGenerator::getColor(const uint16_t v) {
  const uint8_t red = (sin16(v) / 256) + 128;
  const uint8_t green = (cos16(v) / 256) + 128;
  const uint8_t blue = 0;
  return CRGB(red, green, blue);
}

CRGB PastelGenerator::getColor(const uint16_t v) {
  const uint8_t red = 255;
  const uint8_t green = (cos16(v) / 256) + 128;
  const uint8_t blue = (sin16(v) / 256) + 128;
  return CRGB(red, green, blue);
}

CRGB NeonGenerator::getColor(const uint16_t v) {
  const uint8_t red = (sin16(v) / 256) + 128;
  const uint8_t green = (sin16(v + 2 * 32768 / 3) / 256) + 128;
  const uint8_t blue = (sin16(v + 4 * 32768 / 3) / 256) + 128;
  return CRGB(red, green, blue);
}

CRGB ChangingGenerator::getColor(const uint16_t v) {
  timer += 6;
  const uint16_t redOffset = sin16(timer / 13) + 32768;
  const uint16_t greenOffset = sin16(timer / 17) + 32768;
  const uint16_t blueOffset = sin16(timer / 19) + 32768;
  const uint8_t red = (sin16(v + redOffset) / 256) + 128;
  const uint8_t green = (sin16(v + greenOffset) / 256) + 128;
  const uint8_t blue = (sin16(v + blueOffset) / 256) + 128;
  return CRGB(red, green, blue);
}

CRGB ChristmasGenerator::getColor(const uint16_t v) {
  uint8_t red = (sin16(v) / 256) + 128;
  if (red < 128) {
    red = 0;
  }
  uint8_t green = (sin16(v + 2 * 32768 / 3) / 256) + 128;
  if (green < 128 || red >= 128) {
    green = 0;
  }
  uint8_t blue = 0;
  if (red == 0 && green == 0 && blue == 0) {
    red = green = blue = 255;
  }
  return CRGB(red, green, blue);
}

void Animation::setLed(int x, int y, const CRGB &color) {
  if (x >= 0 && x < LED_COLUMN_COUNT) {
    if (y >= 0 && y < LED_ROW_COUNT) {
      const auto offset = XY_TO_OFFSET[x][y];
      if (offset != UNUSED_LED) {
        leds[offset] = color;
      }
    }
  }
}

void Animation::setLed(int index, const CRGB &color) {
  leds[index] = color;
}

Count::Count() : index(0), hue(0) {}

int Count::animate() {
  const int millisPerIteration = 500;

  FastLED.clear();
  setLed(index, CHSV(hue, 255, 255));
  ++hue;
  ++index;
  if (index == LED_COUNT) {
    index = 0;
  }
  return millisPerIteration;
}

CountXY::CountXY() : offset(0) {}

int CountXY::animate() {
  const int millisPerIteration = 500;
  FastLED.clear();

  // Highlight the top and bottom of each column
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    if (XY_TO_OFFSET[x][0] != UNUSED_LED) {
      setLed(x, 0, CRGB::Yellow);
    }

    int y;
    for (y = 0; y < LED_ROW_COUNT; ++y) {
      if (XY_TO_OFFSET[x][y] == UNUSED_LED) {
        break;
      }
    }
    if (y > 0) {
      --y;
    }
    if (XY_TO_OFFSET[x][y] != UNUSED_LED) {
      setLed(x, y, CRGB::Aquamarine);
    }
  }

  ++offset;
  if (offset >= LED_COUNT) {
    offset = 0;
  }
  leds[offset] = CRGB::Red;

  return millisPerIteration;
}

HorizontalSnake::HorizontalSnake() : x(0), y(0), hue(0), xIncreasing(true) {}

int HorizontalSnake::animate() {
  FastLED.clear();
  if (xIncreasing) {
    ++x;
    if (x >= LED_COLUMN_COUNT) {
      --x;
      xIncreasing = false;
      ++y;
      if (y >= LED_ROW_COUNT) {
        y = 0;
      }
    }
  } else {
    --x;
    if (x < 0) {
      ++x;
      xIncreasing = true;
      ++y;
      if (y >= LED_ROW_COUNT) {
        y = 0;
      }
    }
  }
  const int direction = xIncreasing ? 1 : -1;
  int tempX = x;
  for (int i = 0; i < 10; ++i) {
    if (0 <= tempX && tempX < LED_COLUMN_COUNT) {
      setLed(tempX, y, CHSV(hue, 255, 255));
    }
    tempX += direction;
  }
  ++hue;
  return 20;
}

HorizontalComets::HorizontalComets() :
  hue(),
  delay(),
  cometPositions(),
  left(),
  hues()
{
  reset();
}

int HorizontalComets::animate() {
  const int delayStart = 6;
  const int length = 8;

  FastLED.clear();

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

  for (int index = 0; index < COUNT_OF(cometPositions); ++index) {
    const auto start = cometPositions[index];
    uint8_t brightness = 255;
    if (left[index]) {
      for (int j = 0; j < length; ++j) {
        setLed(start + j, index, CHSV(hues[index], 255, brightness));
        brightness -= 30;
      }
      --cometPositions[index];
    } else {
      for (int j = 0; j < length; ++j) {
        setLed(start - j, index, CHSV(hues[index], 255, brightness));
        brightness -= 30;
      }
      ++cometPositions[index];
    }
  }

  return 20;
}

void HorizontalComets::reset() {
  static_assert(COUNT_OF(cometPositions) == COUNT_OF(left));
  static_assert(COUNT_OF(left) == COUNT_OF(hues));
  
  hue = 0;
  delay = delayStart;
  const int init = LED_COLUMN_COUNT + length + 1;
  for (int i = 0; i < COUNT_OF(cometPositions); ++i) {
    cometPositions[i] = init;
    left[i] = false;
    hues[i] = 0;
  }
}

Snake::Snake(int length_) : length(length_), offset(0), hue(0) {}

int Snake::animate() {
  const unsigned millisPerIteration = 20;

  FastLED.clear();

  offset = (offset + 1) % LED_COUNT;
  if (offset < length) {
    fill_rainbow(&leds[0], offset, hue);
  } else if (offset + length >= LED_COUNT) {
    // I don't know if -1 is needed, but I don't want off by ones, so just be safe
    fill_rainbow(&leds[offset], LED_COUNT - offset - 1, hue);
  } else {
    fill_rainbow(&leds[offset], length, hue);
  }
  ++hue;
  return millisPerIteration;
}

Snake::~Snake() {}

Shine::Shine() : hue(0), increasing(), amount(), hues() {
  static_assert(COUNT_OF(increasing) == LED_COUNT);
  static_assert(COUNT_OF(increasing) == COUNT_OF(amount));
  static_assert(COUNT_OF(amount) == COUNT_OF(hues));

  fill(increasing, increasing + LED_COUNT, 0);
  fill(amount, amount + LED_COUNT, 0);
  fill(hues, hues + LED_COUNT, 0);
}

int Shine::animate() {
  const int millisPerIteration = 100;
  const int maxAmount = 240;
  const int changeAmount = 15;
  static_assert(maxAmount % changeAmount == 0);

  hue += 4; // Make it cycle faster
  FastLED.clear();

  // Randomly start increasing an LED
  int chosen = rand() % LED_COUNT;
  if (amount[chosen] == 0) {
    increasing[chosen] = true;
    amount[chosen] = changeAmount;
    hues[chosen] = hue;
  }

  for (int i = 0; i < LED_COUNT; ++i) {
    if (amount[i] > 0) {
      setLed(i, CHSV(hues[i], 255, amount[i]));
      if (increasing[i]) {
        amount[i] += changeAmount;
        // Do >= just to be defensive. I don't expect it to ever be >.
        if (amount[i] >= maxAmount) {
          increasing[i] = false;
        }
      } else {
        amount[i] -= changeAmount;
      }
    }
  }
  return millisPerIteration;
}

Blobs::Blobs(const int count_)
  : hue(0), count(count_), targetX(new float[count_]), targetY(new float[count_]),
    x(new float[count_]), y(new float[count_]), xSpeed(new float[count_]),
    ySpeed(new float[count_]) {
  for (int i = 0; i < count; ++i) {
    targetX[i] = static_cast<float>(rand() % LED_COLUMN_COUNT);
    targetY[i] = static_cast<float>(rand() % LED_ROW_COUNT);
    x[i] = static_cast<float>(rand() % LED_COLUMN_COUNT);
    y[i] = static_cast<float>(rand() % LED_ROW_COUNT);
    xSpeed[i] = 0.0f;
    ySpeed[i] = 0.0f;
  }
}

int Blobs::animate() {
  const int millisPerIteration = 50;
  const float speedChange = 0.05f;
  const float maxSpeed = 3 * speedChange;
  const float radius = 2.0f;
  const float radius2 = radius * radius;
  const float reachedDistance2 = 2.0f;

  FastLED.clear();

  for (int i = 0; i < count; ++i) {
    const float distance2 = (targetX[i] - x[i]) * (targetX[i] - x[i]) +
                            (targetY[i] - y[i]) * (targetY[i] - y[i]);
    if (distance2 < reachedDistance2) {
      targetX[i] = static_cast<float>(rand() % LED_COLUMN_COUNT);
      targetY[i] = static_cast<float>(rand() % LED_ROW_COUNT);
    }

    // TODO: Update the speed based on the direction to the target
    if (x[i] < targetX[i]) {
      xSpeed[i] += speedChange;
      if (xSpeed[i] > maxSpeed) {
        xSpeed[i] = maxSpeed;
      }
    } else {
      xSpeed[i] -= speedChange;
      if (xSpeed[i] < -maxSpeed) {
        xSpeed[i] = -maxSpeed;
      }
    }

    if (y[i] < targetY[i]) {
      ySpeed[i] += speedChange;
      if (ySpeed[i] > maxSpeed) {
        ySpeed[i] = maxSpeed;
      }
    } else {
      ySpeed[i] -= speedChange;
      if (ySpeed[i] < -maxSpeed) {
        ySpeed[i] = -maxSpeed;
      }
    }

    x[i] += xSpeed[i];
    y[i] += ySpeed[i];

    // Just find all the LEDs in range
    const int startX = max(static_cast<int>(x[i] - radius), 0);
    const int endX = min(static_cast<int>(x[i] + radius + 1), LED_COLUMN_COUNT);
    const int startY = max(static_cast<int>(y[i] - radius), 0);
    const int endY = min(static_cast<int>(y[i] + radius + 1), LED_ROW_COUNT);
    const uint8_t hueOffset = 256 / count * i;
    for (int xIter = startX; xIter < endX; ++xIter) {
      for (int yIter = startY; yIter < endY; ++yIter) {
        const auto offset = XY_TO_OFFSET[xIter][yIter];
        if (offset == UNUSED_LED) {
          continue;
        }
        const float distance2 = static_cast<float>(
                                  (xIter - x[i]) * (xIter - x[i]) + (yIter - y[i]) * (yIter - y[i]));
        const float ratio = sqrtf((radius2 - distance2) / radius2);
        if (ratio > 0.0f) {
          const int brightness = static_cast<int>(255 * ratio);
          const CHSV color = CHSV(hue + hueOffset, 255, brightness);
          // TODO(bskari) What does this do? Does this mean it's unset? Ugh
          if (leds[offset]) {
            const CRGB blended = blend(color, leds[offset], 128);
            setLed(xIter, yIter, blended);
          } else {
            setLed(xIter, yIter, color);
          }
        }
      }
    }
  }

  return millisPerIteration;
}

Blobs::~Blobs() {
  delete[] targetX;
  delete[] targetY;
  delete[] x;
  delete[] y;
  delete[] xSpeed;
  delete[] ySpeed;
}

PlasmaBidoulle::PlasmaBidoulle(float multiplier_, float timeIncrement_)
  : time(0), multiplier(multiplier_), timeIncrement(timeIncrement_),
    redMultiplier(1.0f), greenMultiplier(1.0f), blueMultiplier(1.0f),
    redOffset(0.0f), greenOffset(2.0f / 3.0f * M_PI),
    blueOffset(4.0f / 3.0f * M_PI) {}

PlasmaBidoulle::PlasmaBidoulle(float multiplier_, float timeIncrement_,
                               float redMultiplier_, float greenMultiplier_,
                               float blueMultiplier_, float redOffset_,
                               float greenOffset_, float blueOffset_)
  : time(0), multiplier(multiplier_), timeIncrement(timeIncrement_),
    redMultiplier(redMultiplier_), greenMultiplier(greenMultiplier_),
    blueMultiplier(blueMultiplier_), redOffset(redOffset_),
    greenOffset(greenOffset_), blueOffset(blueOffset_) {}

uint8_t PlasmaBidoulle::convert(const float f) {
  return static_cast<uint8_t>((f + 1.0f) * 0.5f * 255.0f);
}

int PlasmaBidoulle::animate() {
  // Adapted from https://www.bidouille.org/prog/plasma
  const float M_PI_F = static_cast<float>(M_PI);

  FastLED.clear();
  time += timeIncrement;
  float cys[LED_ROW_COUNT];
  for (int y = 0; y < LED_ROW_COUNT; ++y) {
    cys[y] = y + 0.5f * cosf(time * (1.0f / 3.0f));
  }
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    const float v1 = sinf(multiplier * x + time);
    const float cx = sinf(x + 0.5 * sinf(time * 0.2f));
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      if (XY_TO_OFFSET[x][y] != UNUSED_LED) {
        const float v2 = sinf(multiplier * (x * sinf(time * 0.5f) +
                                            y * cosf(time * (1.0f / 3.0f))) +
                              time);
        const float v3 =
          sinf(multiplier * sqrtf((cx * cx + cys[y] * cys[y] + 1.0f)) + time);
        const float v = v1 + v2 + v3;
        // const float v = v1 + v2;
        const float red = redMultiplier * sinf(v * M_PI_F + redOffset);
        const float green = greenMultiplier * sinf(v * M_PI_F + greenOffset);
        const float blue = blueMultiplier * sinf(v * M_PI_F + blueOffset);
        const uint8_t r = convert(red);
        const uint8_t g = convert(green);
        const uint8_t b = convert(blue);
        setLed(x, y, CRGB(r, g, b));
      }
    }
  }
  return 0;
}

PlasmaBidoulleFast::PlasmaBidoulleFast(ColorGenerator &colorGenerator_)
  : colorGenerator(colorGenerator_), time(0) {}

int PlasmaBidoulleFast::animate() {
  // Partially adapted from https://www.bidouille.org/prog/plasma, using fast
  // integer math.
  const uint16_t multiplier = 0.15f * PI_16_1_0;
  const uint16_t timeIncrement = 0.1f * PI_16_1_0;
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

  time += timeIncrement;
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    const int16_t v1 = sin16(multiplier * x + time); // good
    // const int16_t cx = sin16(multiplier * x + sin16(time / 5) / 2);  // bad
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      if (XY_TO_OFFSET[x][y] != UNUSED_LED) {
        const int16_t v2 = sin16(
                             (multiplier * (x * sin16(time / 4) / 2 + y * cos16(time / 3)) +
                              time) /
                             16384); // bad values, but looks good?
        const int adjustedX = x + xOffset + X_CENTER;
        const int adjustedY = y + yOffset + Y_CENTER;
        const uint16_t blend1 = bidoulleV3rings[adjustedX][adjustedY] * (blend - xRemainder) / blend;
        const uint16_t blend2 = bidoulleV3rings[adjustedX + 1][adjustedY] * xRemainder / blend;
        const uint16_t v3 = (blend1 + blend2) / 2 * 256;
        const uint16_t v = v1 + v2 + v3;
        setLed(x, y, colorGenerator.getColor(v));
      }
    }
  }
  return 20;
}

Plasma1::Plasma1(ColorGenerator &colorGenerator_)
  : colorGenerator(colorGenerator_), time(0) {}

int Plasma1::animate() {
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      const auto index = XY_TO_OFFSET[x][y];
      if (index != UNUSED_LED) {
        const uint8_t p1 = 128 + (sin16(x * (PI_16_1_0 / 4) + time / 4)) / 256;
        const uint8_t p2 = 128 + (sin16(y * (PI_16_1_0 / 4) + time / 4)) / 256;
        const uint8_t p3 =
          128 + (sin16((x + y) * (PI_16_1_0 / 8) + time / 8)) / 256;
        const uint8_t p4 =
          128 + (sin16(sqrt16(x * x + y * y) * 1000 + time)) / 256;
        const uint8_t v = (p1 + p2 + p3 + p4) / 4;
        setLed(x, y, colorGenerator.getColor(v));
      }
    }
  }
  time += 1000;
  return 10;
}

Plasma2::Plasma2(ColorGenerator &colorGenerator_)
  : colorGenerator(colorGenerator_), time(0) {}

int Plasma2::animate() {
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      const auto index = XY_TO_OFFSET[x][y];
      if (index != UNUSED_LED) {
        const uint8_t p1 = 128 + (sin16(x * (PI_16_1_0 / 8) + time / 4)) / 256;
        const uint8_t p2 = 128 + sin16(10 * (x * sin16(time / 2) / 256 +
                                             y * cos16(time / 3) / 256)) /
                           256;
        const uint8_t p3 =
          128 + (sin16((x + y) * (PI_16_1_0 / 8) + time / 8)) / 256;
        // cx = x + 0.5 * sin(time / 5)
        // cy = y + 0.5 * cos(time / 3)
        // v = sin(sqrt(100 * (cx**2 + cy ** 2) + 1) + time)
        const uint16_t cx = x + sin16(time / 8) / 1024;
        const uint16_t cy = y + sin16(time / 16) / 1024;
        const uint8_t p4 =
          128 +
          sin16(sqrt16(cx * cx + cy * cy) * (PI_16_1_0 / 2) + time) / 512;
        const uint8_t v = (p1 + p2 + p3 + p4) / 4;
        setLed(x, y, colorGenerator.getColor(v));
      }
    }
  }
  time += 1000;
  return 10;
}

Plasma3::Plasma3(ColorGenerator &colorGenerator_)
  : colorGenerator(colorGenerator_), time(0) {}

int Plasma3::animate() {
  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      const auto index = XY_TO_OFFSET[x][y];
      if (index != UNUSED_LED) {
        const uint8_t p1 = 128 + (sin16(x * (PI_16_1_0 / 8) + time / 4)) / 256;
        const uint8_t p2 = 128 + sin16(10 * (x * sin16(time / 2) / 256 +
                                             y * cos16(time / 3) / 256)) /
                           256;
        const uint8_t p3 =
          128 + (sin16((x + y) * (PI_16_1_0 / 8) + time / 8)) / 256;
        // cx = x + 0.5 * sin(time / 5)
        // cy = y + 0.5 * cos(time / 3)
        // v = sin(sqrt(100 * (cx**2 + cy ** 2) + 1) + time)
        const uint16_t cx = x + sin16(time / 8) / 1024;
        const uint16_t cy = y + sin16(time / 16) / 1024;
        const uint8_t p4 =
          128 +
          sin16(sqrt16(cx * cx + cy * cy) * (PI_16_1_0 / 2) + time) / 512;
        const uint8_t v = (p1 + p2 + p3 + p4) / 4;
        setLed(x, y, colorGenerator.getColor(v));
      }
    }
  }
  time += 1000;
  return 10;
}

CenteredVideo::CenteredVideo(
  uint32_t (*getColor_)(int, int),
  uint32_t frameCount_,
  uint16_t millisPerFrame_
) : getColor(getColor_),
  frameCount(frameCount_),
  millisPerFrame(millisPerFrame_),
  frame(0)
{
}

int CenteredVideo::animate() {
  const int height = 11;
  const int width = 12;
  const int startColumn = 8;

  for (int x = 0; x < LED_COLUMN_COUNT; ++x) {
    for (int y = 0; y < LED_ROW_COUNT; ++y) {
      setLed(x, y, CRGB::Black);
    }
  }

  int count = 0;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const CRGB value = CRGB(getColor(frame, count));
      ++count;
      setLed(startColumn + width - x - 1, height - y, value);
    }
  }
  frame = (frame + 1) % frameCount;
  return millisPerFrame;
}

SpectrumAnalyzer1::SpectrumAnalyzer1(int (*sf)(void)) : soundFunction(sf) {
}

int SpectrumAnalyzer1::animate() {
  return 0;
}

SnakeGame::SnakeGame()
  : length(2),
    hue(0),
    fruitX(),
    fruitY(),
    fruitBrightness(128),
    fruitBrightnessIncreasing(true),
    state(GameState::running),
    nextUpdate(millis() + MILLIS_PER_TICK),
    body()
{
  reset();
}

void SnakeGame::reset() {
  length = 2;
  for (int i = length; i < WIDTH * HEIGHT; ++i) {
    body[i][0] = 0;
    body[i][1] = 0;
  }
  body[0][0] = WIDTH / 2;
  body[0][1] = HEIGHT / 2;
  body[1][0] = WIDTH / 2;
  body[1][1] = HEIGHT / 2;
  state = GameState::running;
  nextUpdate = millis() + MILLIS_PER_TICK;
}

int SnakeGame::animate() {
  tickGraphics();
  draw();
  if (millis() > nextUpdate) {
    if (state == GameState::gameOver) {
      state = GameState::running;
      reset();
    } else {
      tick();
    }
    nextUpdate = millis() + MILLIS_PER_TICK;
  }
  return 10;
}

void SnakeGame::tickGraphics() {
  ++hue;

  if (fruitBrightnessIncreasing) {
    fruitBrightness += 10;
    if (fruitBrightness >= 240) {
      fruitBrightnessIncreasing = false;
    }
  } else {
    fruitBrightness -= 10;
    if (fruitBrightness <= 128) {
      fruitBrightnessIncreasing = true;
    }
  }
}

void SnakeGame::tick() {
  int8_t newX = body[0][0];
  int8_t newY = body[0][1];

  int8_t xOffset = 0;
  int8_t yOffset = 0;
  // TODO: Read the controller
  static uint8_t reversePreviousDirection = 255;
  uint8_t dir = random8() % 4;
  while (dir == reversePreviousDirection) {
    dir = random8() % 4;
  }
  reversePreviousDirection = (dir + 2) % 4;
  switch (dir) {
    case 0:
      xOffset = 1;
      break;
    case 1:
      yOffset = 1;
      break;
    case 2:
      xOffset = -1;
      break;
    case 3:
      yOffset = -1;
      break;
  }
  newX += xOffset;
  newY += yOffset;

  // If the snake has collided with itself, game over
  for (int i = 0; i < length; ++i) {
    if (body[i][0] == newX && body[i][1] == newY) {
      state = GameState::gameOver;
      nextUpdate = millis() + 5000;
      return;
    }
  }

  // If the snake has hit a wall, game over
  if (newX == -1 || newX == WIDTH || newY == -1 || newY == HEIGHT) {
    state = GameState::gameOver;
    nextUpdate = millis() + 5000;
    return;
  }

  // If the snake has hit the fruit
  if (body[0][0] == fruitX && body[0][1] == fruitY) {
    ++length;
    spawnFruit();
  } else if (random8() % 4 == 0) {
    ++length;
  }

  // Slide all the body segments down
  for (int i = length - 2; i >= 0; --i) {
    body[i + 1][0] = body[i][0];
    body[i + 1][1] = body[i][1];
  }
  body[0][0] = newX;
  body[0][1] = newY;
}

void SnakeGame::spawnFruit() {
  // A better way to do this would be to get a list of all available
  // spaces, put them in an array, and then randomly pick one
  for (int failSafe = 0; failSafe < 10000; ++failSafe) {
    uint8_t newFruitX = random8() % WIDTH;
    uint8_t newFruitY = random8() % HEIGHT;
    bool collide = false;
    for (int i = 0; i < length; ++i) {
      if (body[i][0] == newFruitX && body[i][1] == newFruitY) {
        collide = true;
        break;
      }
    }
    if (collide) {
      continue;
    }
    fruitX = newFruitX;
    fruitY = newFruitY;
  }
}

void SnakeGame::draw() const {
  FastLED.clear();

  // Draw border
  for (int y = 0; y < HEIGHT; ++y) {
    setLed(START_COLUMN - 1, y, CRGB::Gray);
    setLed(START_COLUMN + WIDTH, y, CRGB::Gray);
  }

  setLed(fruitX + START_COLUMN, fruitY, CRGB(fruitBrightness, fruitBrightness, fruitBrightness));

  uint8_t hueIterable = hue;
  for (int i = 0; i < length; ++i) {
    setLed(body[i][0] + START_COLUMN, body[i][1], CHSV(hueIterable, 255, 255));
    hueIterable += 10;
  }
}

BasicSpiral::BasicSpiral(ColorGenerator& colorGenerator_) : colorGenerator(colorGenerator_), time(0) {}

int BasicSpiral::animate() {
  time += 2000;
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
    for (uint16_t theta = 0; theta < std::numeric_limits<decltype(theta)>::max() - 2 * thetaStep; theta += thetaStep) {
      const int16_t x = static_cast<int>(sin16(theta + time)) * distance / divisor + X_CENTER;
      const int16_t y = static_cast<int>(cos16(theta + time)) * distance / divisor + Y_CENTER;
      const uint8_t v = (theta + distance * distanceMultiplier) / 255;
      setLed(x, y, colorGenerator.getColor(v));
    }
  }
  return 40;
}
