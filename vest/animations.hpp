#ifndef ANIMATIONS_HPP
#define ANIMATIONS_HPP
#include <FastLED.h>
#include <cstdint>

#include "constants.hpp"

using std::uint16_t;
using std::uint32_t;
using std::uint8_t;

class CRGB;

// Hue generator
class ColorGenerator {
  public:
    ColorGenerator() = default;
    ~ColorGenerator() = default;
    virtual CRGB getColor(uint8_t v);
    virtual CRGB getColor(uint16_t v);
};

class RedGreenGenerator : public ColorGenerator {
  public:
    RedGreenGenerator() = default;
    ~RedGreenGenerator() = default;
    CRGB getColor(uint16_t value);
};

class PastelGenerator : public ColorGenerator {
  public:
    PastelGenerator() = default;
    ~PastelGenerator() = default;
    CRGB getColor(uint16_t value);
};

class NeonGenerator : public ColorGenerator {
  public:
    NeonGenerator() = default;
    ~NeonGenerator() = default;
    CRGB getColor(uint16_t value);
};

class ChangingGenerator : public ColorGenerator {
  public:
    ChangingGenerator() = default;
    ~ChangingGenerator() = default;
    CRGB getColor(uint16_t value);
  private:
    uint32_t timer;
};

class ChristmasGenerator : public ColorGenerator {
  public:
    ChristmasGenerator() = default;
    ~ChristmasGenerator() = default;
    CRGB getColor(uint16_t value);
  private:
    uint32_t timer;
};

class Animation {
  public:
    /**
     * Runs a tick of the animation, and returns the number of milliseconds until the next time it
     * should be called
     */
    virtual int animate() = 0;
    virtual ~Animation() = default;
    virtual void reset() {}

    static void setLed(int x, int y, const CRGB& color);
    static void setLed(int index, const CRGB& color);
};

class Count : public Animation {
  public:
    Count();
    ~Count() = default;
    int animate() override;
  private:
    int index;
    uint8_t hue;
};

class CountXY : public Animation {
  public:
    CountXY();
    ~CountXY() = default;
    int animate() override;
  private:
    uint16_t offset;
};

class Snake : public Animation {
  public:
    Snake(int length);
    ~Snake();
    int animate() override;
  private:
    const int length;
    int offset;
    uint8_t hue;
};

class HorizontalSnake : public Animation {
  public:
    HorizontalSnake();
    ~HorizontalSnake() = default;
    int animate() override;
  private:
    int x;
    int y;
    uint8_t hue;
    bool xIncreasing;
};

class HorizontalComets : public Animation {
  public:
    HorizontalComets();
    ~HorizontalComets() = default;
    int animate() override;
    void reset() override;
  private:
    const int delayStart = 6;
    const int length = 8;

    uint8_t hue;
    uint8_t delay;
    int16_t cometPositions[LED_ROW_COUNT];
    bool left[LED_ROW_COUNT];
    uint8_t hues[LED_ROW_COUNT];
};

class Fire : public Animation {
  public:
    Fire();
    ~Fire() = default;
    int animate() override;
  private:
    uint32_t colors[LED_COUNT];
    uint8_t heights[10];
};

class Shine : public Animation {
  public:
    Shine();
    ~Shine() = default;
    int animate() override;
  private:
    uint8_t hue;
    bool increasing[LED_COUNT];
    int amount[LED_COUNT];
    uint8_t hues[LED_COUNT];
};

class SpectrumAnalyzer1 : public Animation {
  public:
    SpectrumAnalyzer1(int (*soundFunction)(void));
    ~SpectrumAnalyzer1() = default;
    int animate() override;
  private:
    int (*soundFunction)(void);
};

class Blobs : public Animation {
  public:
    Blobs(int count_);
    ~Blobs();
    int animate() override;
  private:
    uint8_t hue;
    const int count;
    float* targetX;
    float* targetY;
    float* x;
    float* y;
    float* xSpeed;
    float* ySpeed;
};

class PlasmaBidoulle : public Animation {
  public:
    // 0.15, 0.1 works well
    PlasmaBidoulle(float multiplier, float timeIncrement);
    PlasmaBidoulle(
      float multiplier,
      float timeIncrement,
      float redMultiplier,
      float greenMultiplier,
      float blueMultiplier,
      float redOffset,
      float greenOffset,
      float blueOffset
    );
    ~PlasmaBidoulle() = default;
    int animate() override;
  private:
    float time;
    const float multiplier;
    const float timeIncrement;
    const float redMultiplier;
    const float greenMultiplier;
    const float blueMultiplier;
    const float redOffset;
    const float greenOffset;
    const float blueOffset;

    static uint8_t convert(const float f);
};


// A (partial) implementation of Bidoulle's animation, using fast 16-bit math
class PlasmaBidoulleFast : public Animation {
  public:
    PlasmaBidoulleFast(ColorGenerator& colorGenerator);
    ~PlasmaBidoulleFast() = default;
    int animate() override;
  private:
    ColorGenerator& colorGenerator;
    uint32_t time;
};

class Plasma1 : public Animation {
  public:
    Plasma1(ColorGenerator& colorGenerator);
    ~Plasma1() = default;
    int animate() override;
  private:
    ColorGenerator& colorGenerator;
    int time;
};

class Plasma2 : public Animation {
  public:
    Plasma2(ColorGenerator& colorGenerator);
    ~Plasma2() = default;
    int animate() override;
  private:
    ColorGenerator& colorGenerator;
    int time;
};

class Plasma3 : public Animation {
  public:
    Plasma3(ColorGenerator& colorGenerator);
    ~Plasma3() = default;
    int animate() override;
  private:
    ColorGenerator& colorGenerator;
    int time;
};

class CenteredVideo : public Animation {
  public:
    // I don't think you can send a 2D array pointer in C++, so I'll
    // send a function pointer that returns data from the correct array
    CenteredVideo(
      uint32_t (*getColor)(int, int),
      uint32_t frameCount,
      uint16_t millisPerFrame
    );
    ~CenteredVideo() = default;
    int animate() override;
  private:
    uint32_t (*getColor)(int, int);
    const uint32_t frameCount;
    const uint16_t millisPerFrame;
    int frame;
};

class SnakeGame : public Animation {
  public:
    SnakeGame();
    ~SnakeGame() = default;
    int animate() override;
  private:
    static const int HEIGHT = 11;
    static const int WIDTH = 10;
    static const int START_COLUMN = 9;
    static const typeof(millis()) MILLIS_PER_TICK = 200;
    enum class GameState {running, gameOver};

    uint8_t length;
    uint8_t hue;
    uint8_t fruitX, fruitY;
    uint8_t fruitBrightness;
    bool fruitBrightnessIncreasing;
    GameState state;
    typeof(millis()) nextUpdate;
    int8_t body[WIDTH * HEIGHT][2];

    void reset();
    void update();
    void draw() const;
    void spawnFruit();
    void tick();
    void tickGraphics();
};

class BasicSpiral : public Animation {
  public:
    BasicSpiral(ColorGenerator &colorGenerator);
    ~BasicSpiral() = default;
    int animate() override;
  private:
    ColorGenerator& colorGenerator;
    int time;
};

#endif
