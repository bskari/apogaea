// To compile:
// arduino-cli compile --fqbn esp32:esp32:esp32da piddle.ino
// To upload:
// arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32da
// I don't know how to set frequency and stuff easily, might need to use the Arduino IDE

#include <FastLED.h>

#include "I2SClocklessLedDriver/I2SClocklessLedDriver.h"
#include "constants.hpp"
#include "spectrumAnalyzer.hpp"

#ifdef USE_REMOTEXY
//////////////////////////////////////////////
//        RemoteXY include library          //
//////////////////////////////////////////////

// you can enable debug logging to Serial at 115200
//#define REMOTEXY__DEBUGLOG

// RemoteXY select connection mode and include library
#define REMOTEXY_MODE__ESP32CORE_BLE

#include <BLEDevice.h>

// RemoteXY connection settings
#define REMOTEXY_BLUETOOTH_NAME "Sonic Bloom"


#include <RemoteXY.h>

// RemoteXY GUI configuration
#pragma pack(push, 1)
uint8_t RemoteXY_CONF[] =   // 160 bytes
  { 255,5,0,0,0,153,0,19,0,0,0,0,24,1,126,200,1,1,10,0,
  129,32,15,62,12,64,17,66,114,105,103,104,116,110,101,115,115,0,129,33,
  47,60,12,64,17,83,101,110,115,105,116,105,118,105,116,121,0,4,12,32,
  104,10,128,2,26,4,12,62,104,10,128,2,26,129,43,76,35,12,64,17,
  83,112,101,101,100,0,4,12,90,104,10,128,2,26,2,71,115,30,12,1,
  2,26,31,31,79,78,0,79,70,70,0,129,21,116,48,12,64,17,82,97,
  105,110,98,111,119,0,2,75,134,30,12,1,2,26,31,31,79,78,0,79,
  70,70,0,129,16,135,57,12,64,17,78,111,114,109,97,108,105,122,101,0 };
#endif

// this structure defines all the variables and events of your control interface
struct {

    // input variables
  int8_t brightnessSlider; // from 0 to 100
  int8_t sensitivitySlider; // from 0 to 100
  int8_t speedSlider; // from 0 to 100
  uint8_t rainbowSwitch; // =1 if switch ON and =0 if OFF
  uint8_t normalizeBandsSwitch; // =1 if switch ON and =0 if OFF

    // other variable
  uint8_t connect_flag;  // =1 if wire connected, else =0

} RemoteXY;
#pragma pack(pop)

/////////////////////////////////////////////
//           END RemoteXY include          //
/////////////////////////////////////////////

void blink(const int delay_ms = 500);

CRGB leds[STRIP_COUNT][LEDS_PER_STRIP];
bool logDebug = false;

TaskHandle_t collectSamplesTask;
TaskHandle_t displayLedsTask;
I2SClocklessLedDriver driver;

void IRAM_ATTR buttonInterrupt() {
  static uint8_t index = 0;
  const uint8_t brightnesses[] = {16, 32, 64, 128, 255};
  const char* const percents[] = {"6", "13", "25", "50", "100"};

  index = (index + 1) % COUNT_OF(brightnesses);
  const uint8_t brightness = brightnesses[index];
  Serial.printf("brightness %d (%s %%)\n", brightness, percents[index]);
  driver.setBrightness(brightness);
}

void setup() {
  Serial.begin(115200);

  // This had to be done first, but I think I fixed the bug that was causing problems? I don't want
  // to test if it's fixed, so I'm leaving it first now
  setupSpectrumAnalyzer();

  //analogReference(AR_DEFAULT); // Not on ESP32?
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(VOLTAGE_PIN, INPUT);

  driver.initled(reinterpret_cast<uint8_t*>(leds), LED_PINS, COUNT_OF(LED_PINS), LEDS_PER_STRIP, ORDER_RGB);
  driver.setBrightness(64);

  // The boot button is connected to GPIO0
  pinMode(0, INPUT);
  attachInterrupt(0, buttonInterrupt, FALLING);

#ifdef USE_REMOTEXY
  RemoteXY_Init();
  RemoteXY.brightnessSlider = 25;
  RemoteXY.rainbowSwitch = false;
  RemoteXY.normalizeBandsSwitch = false;
  RemoteXY.speedSlider = 85;
  RemoteXY.sensitivitySlider = 50;
#endif

  xTaskCreatePinnedToCore(
    collectSamplesFunction,
    "collectSamples",
    4000, // Stack size in words
    nullptr, // Task input parameter
    1, // Priority of the task
    &collectSamplesTask, // Task handle.
    1); // Core where the task should run

  // Test all the logic level converter LEDs
  uint8_t hue = 0;
  for (int i = 0; i < 5; ++i) {
    for (int strip = 0; strip < STRIP_COUNT; ++strip) {
      fill_solid(reinterpret_cast<CRGB*>(leds), STRIP_COUNT * LEDS_PER_STRIP, CRGB::Black);
      const uint8_t brightnesses[] = {16, 32, 64, 128, 64, 32, 16};
      uint8_t huePart = hue;
      for (int i = 0; i < COUNT_OF(brightnesses); ++i) {
        const int innerStrip = (strip + STRIP_COUNT + i) % STRIP_COUNT;
        leds[innerStrip][0] = CHSV(huePart, 255, brightnesses[i]);
        huePart += 10;
      }
      driver.showPixels();
      delay(50);
      hue += 10;
    }
  }

  // We need to do this last because it will preempt the setup thread that's running on core 0
  xTaskCreatePinnedToCore(
    displayLedsFunction,
    "displayLeds",
    4000, // Stack size in words
    nullptr, // Task input parameter
    1, // Priority of the task
    &displayLedsTask, // Task handle.
    0); // Core where the task should run
}

#ifndef USE_REMOTEXY
void RemoteXY_delay(const int x) {
  delay(x);
}
#endif

void loop() {
  RemoteXY_delay(10000);
}

void collectSamplesFunction(void*) {
  while (1) {
    collectSamples();
  }
}

void displayLedsFunction(void*) {
  while (1) {
    for (int i = 0; i < 100; ++i) {
      displaySpectrumAnalyzer(
        RemoteXY.brightnessSlider,
        RemoteXY.rainbowSwitch,
        RemoteXY.normalizeBandsSwitch,
        RemoteXY.sensitivitySlider,
        RemoteXY.speedSlider);

      if (Serial.available() > 0) {
        logDebug = true;
        while (Serial.available() > 0) {
          Serial.read();
        }
      }
    }
    // Keep the watchdog happy
    delay(1);
  }
}

void blink(const int delay_ms) {
  digitalWrite(LED_BUILTIN, HIGH);
  RemoteXY_delay(delay_ms);
  digitalWrite(LED_BUILTIN, LOW);
  RemoteXY_delay(delay_ms);
}

// This needs to be defined so that other files can call it, because the RemoteXY library does
// something weird
void RemoteXY_delayFunction(int ms) {
  // Specifically, this is a macro that compiles to remotexy->delay(), but I don't know where
  // "remotexy" is declared
  RemoteXY_delay(ms);
}
