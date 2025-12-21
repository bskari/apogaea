#include <FastLED.h>
#include <math.h>
#include <SPI.h>
// you can enable debug logging to Serial at 115200
//#define REMOTEXY__DEBUGLOG
// RemoteXY select connection mode and include library
#define REMOTEXY_MODE__ESP32CORE_BLE
#include <BLEDevice.h>
#include <RemoteXY.h>

#define REMOTEXY_BLUETOOTH_NAME "vest"

#include "animations.hpp"
#include "constants.hpp"
#include "movies.hpp"

// RemoteXY GUI configuration  
#pragma pack(push, 1)  
uint8_t RemoteXY_CONF[] =   // 391 bytes
  { 255,8,0,11,0,128,1,17,0,0,0,24,1,126,200,3,1,1,0,2,
  0,131,2,2,60,14,2,79,2,31,77,97,105,110,0,37,131,64,2,60,
  14,2,79,2,31,83,68,0,25,15,0,2,8,26,44,22,0,2,26,31,
  31,99,121,99,108,101,0,115,116,97,116,105,99,0,3,59,26,9,71,9,
  2,26,129,70,28,24,6,17,80,108,97,115,109,97,51,0,129,70,91,40,
  6,17,66,105,100,111,117,108,108,101,32,78,101,111,110,0,129,70,67,48,
  6,17,72,111,114,105,122,111,110,116,97,108,32,83,110,97,107,101,0,129,
  70,35,17,6,17,83,110,97,107,101,0,129,70,75,40,6,17,83,112,105,
  114,97,108,0,129,70,51,40,6,17,83,104,105,110,101,0,129,70,83,40,
  6,17,83,112,111,116,108,105,103,104,116,115,0,4,6,178,36,10,128,2,
  26,129,8,168,30,6,17,77,97,120,32,112,111,119,101,114,0,129,70,59,
  42,6,17,66,105,100,111,117,108,108,101,32,80,97,115,116,101,108,0,129,
  70,43,52,6,17,66,105,100,111,117,108,108,101,32,67,104,97,110,103,105,
  110,103,0,4,48,178,36,10,128,2,26,129,48,168,30,6,17,66,114,105,
  103,104,116,110,101,115,115,0,10,0,67,6,52,114,10,5,2,26,11,1,
  54,68,18,18,0,164,31,0,1,32,68,18,18,0,164,31,0,1,76,68,
  18,18,0,164,31,0,129,34,90,12,6,17,80,114,101,118,0,129,78,90,
  12,6,17,78,101,120,116,0,129,56,90,12,6,17,80,108,97,121,0,3,
  32,20,14,26,2,2,26,129,50,24,30,10,17,65,117,100,105,111,0,129,
  52,34,29,10,17,86,105,100,101,111,0 };
  
// this structure defines all the variables and events of your control interface 
struct {

    // input variables
  uint8_t switch_cycle; // =1 if switch ON and =0 if OFF
  uint8_t animation; // from 0 to 9
  int8_t slider_max_power; // from 0 to 100
  int8_t slider_brightness; // from 0 to 100
  uint8_t button_play; // =1 if button pressed, else =0
  uint8_t button_previous; // =1 if button pressed, else =0
  uint8_t button_next; // =1 if button pressed, else =0
  uint8_t audioVideo; // from 0 to 2

    // output variables
  char text_sd[11]; // string UTF8 end zero

    // other variable
  uint8_t connect_flag;  // =1 if wire connected, else =0

} RemoteXY;   
#pragma pack(pop)

CRGB leds[LED_COUNT];

void setup() {
  Serial.begin(115200);

  Serial.println("SPI");
  Serial.flush();
  delay(100);
  SPI.begin();

  Serial.println("pinMode");
  Serial.flush();
  delay(100);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  pinMode(SD_PIN, OUTPUT);
  constexpr int pins[] = {17, 21, 4, 0, 15};
  for (const auto pin : pins) {
    pinMode(pin, OUTPUT);
  }

  Serial.println("RemoteXY");
  Serial.flush();
  delay(100);
  RemoteXY_Init();
  RemoteXY.slider_max_power = 25;
  RemoteXY.slider_brightness = 50;
  RemoteXY.switch_cycle = 1;  // Start off cycling

  Serial.println("FastLED");
  Serial.flush();
  delay(100);
  FastLED.addLeds<WS2812, pins[0], GRB>(&leds[LINEAR_LED_INDEXES[0]], STRAND_TO_LED_COUNT[0]);
  FastLED.addLeds<WS2812, pins[1], GRB>(&leds[LINEAR_LED_INDEXES[1]], STRAND_TO_LED_COUNT[1]);
  FastLED.addLeds<WS2812, pins[2], GRB>(&leds[LINEAR_LED_INDEXES[2]], STRAND_TO_LED_COUNT[2]);
  FastLED.addLeds<WS2812, pins[3], GRB>(&leds[LINEAR_LED_INDEXES[3]], STRAND_TO_LED_COUNT[3]);
  FastLED.addLeds<WS2812, pins[4], GRB>(&leds[LINEAR_LED_INDEXES[4]], STRAND_TO_LED_COUNT[4]);
  // Brightness and max power will be set below
  FastLED.clear();
  FastLED.show();

  Serial.println("exiting setup");
  Serial.flush();
  delay(100);
}

static bool playingMovie = false;
static uint8_t configuredBrightness; // out of 255

static int soundFunction() {
  return 0;
}

static ColorGenerator hueGenerator;
static RedGreenGenerator redGreenGenerator;
static PastelGenerator pastelGenerator;
static NeonGenerator neonGenerator;
static ChangingGenerator changingGenerator;
static ChristmasGenerator christmasGenerator;

static Snake snake(10);
static HorizontalSnake horizontalSnake;
static Count count;
static CountXY countXY;
static Shine shine;
static Blobs blobs(4);
static PlasmaBidoulleFast bidoulleChristmas(christmasGenerator);
static PlasmaBidoulleFast bidoulleNeon(neonGenerator);
static PlasmaBidoulleFast bidoulleRedGreen(redGreenGenerator);
static PlasmaBidoulleFast bidoullePastel(pastelGenerator);
static PlasmaBidoulleFast bidoulleChanging(changingGenerator);
static Plasma3 plasma3(hueGenerator);
static BasicSpiral spiral(hueGenerator);
static MoviePlayer moviePlayer;
static HorizontalComets horizontalComets;

static SpectrumAnalyzer1 spectrumAnalyzer1(soundFunction);

//static constexpr Animation* animations[] = { &plasma3, &horizontalSnake, &snake, &spiral, &shine, &blobs };
static constexpr Animation* animations[] = {
  &plasma3,
  &snake,
  &bidoulleChanging,
  &shine,
  &bidoullePastel,
  &horizontalSnake,
  &spiral,
  &blobs,
  &bidoulleNeon,
  &horizontalComets,
};
//static constexpr Animation* const* animations[] = { &snake };

void loop() {

  while (true) {
    const int delay_ms = playingMovie ? moviePlayer.animate() : animations[RemoteXY.animation]->animate();
    FastLED.show();
    delayAndHandleRemoteXy(delay_ms);
  }
}

static void delayAndHandleRemoteXy(const int delay_ms) {
  static uint8_t previousSwitchCycle = RemoteXY.switch_cycle;

  // TODO: Make this a slider too?
  const int animationDuration_ms = 30000;
  const int fade_ms = 2000;

  enum class AnimationState {
    Playing,
    FadeOut,
    FadeIn,
  };
  static AnimationState state = AnimationState::Playing;
  static decltype(millis()) nextState_ms = millis() + animationDuration_ms;

  const int start_ms = millis();
  do {
    // Do this delay manually, to keep both RemoteXY and FastLED brightness dithering
    RemoteXY_Handler();
    FastLED.show();
  } while (millis() < start_ms + delay_ms);

  if (previousSwitchCycle != RemoteXY.switch_cycle) {
    nextState_ms = millis() + animationDuration_ms;
    state = AnimationState::Playing;
    playingMovie = false;
  }
  previousSwitchCycle = RemoteXY.switch_cycle;

  if (RemoteXY.button_play) {
    playingMovie = true;
  }
  if (RemoteXY.button_next) {
    if (moviePlayer) {
      moviePlayer.next(RemoteXY.text_sd, COUNT_OF(RemoteXY.text_sd));
    } else {
      const char message[] = "FAIL";
      static_assert(COUNT_OF(message) < COUNT_OF(RemoteXY.text_sd));
      strcpy(RemoteXY.text_sd, message);
    }
  } else if (RemoteXY.button_previous) {
    if (moviePlayer) {
      moviePlayer.previous(RemoteXY.text_sd, COUNT_OF(RemoteXY.text_sd));
    } else {
      const char message[] = "FAIL";
      static_assert(COUNT_OF(message) < COUNT_OF(RemoteXY.text_sd));
      strcpy(RemoteXY.text_sd, message);
    }
  }

  // Switch animation
  if (RemoteXY.switch_cycle) {
    const auto now_ms = millis();

    if (now_ms >= nextState_ms) {
      switch (state) {
        case AnimationState::Playing:
          state = AnimationState::FadeOut;
          nextState_ms = now_ms + fade_ms;
          break;
        case AnimationState::FadeOut:
          state = AnimationState::FadeIn;
          nextState_ms = now_ms + fade_ms;
          // Go to next animation
          ++RemoteXY.animation;
          if (RemoteXY.animation >= COUNT_OF(animations)) {
            RemoteXY.animation = 0;
          }
          break;
        case AnimationState::FadeIn:
          state = AnimationState::Playing;
          nextState_ms = now_ms + animationDuration_ms;
          break;
      }
    }

    switch (state) {
      case AnimationState::Playing:
        FastLED.setBrightness(gamma8[configuredBrightness]);
        break;
      case AnimationState::FadeOut: {
          const auto diff_ms = nextState_ms - now_ms;
          const float ratio = static_cast<float>(diff_ms) / fade_ms;
          const uint8_t brightness = constrain(ratio * configuredBrightness, 0, 255);
          const uint8_t corrected = gamma8[brightness];
          FastLED.setBrightness(corrected);
          break;
        }
      case AnimationState::FadeIn: {
          const auto diff_ms = nextState_ms - now_ms;
          const float ratio = 1.0f - (static_cast<float>(diff_ms) / fade_ms);
          const uint8_t brightness = constrain(ratio * configuredBrightness, 0, 255);
          const uint8_t corrected = gamma8[brightness];
          FastLED.setBrightness(corrected);
          break;
        }
    }
  }

  // Set max milliamps between 500 and 2000
  FastLED.setMaxPowerInVoltsAndMilliamps(5, RemoteXY.slider_max_power * (1500 / 100) + 500);

  configuredBrightness = max(RemoteXY.slider_brightness * 255 / 100, 5);
}
