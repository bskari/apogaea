// To compile:
// arduino-cli compile --fqbn esp32:esp32:esp32da piddle.ino
// To upload:
// arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32da
// I don't know how to set frequency and stuff easily, might need to use the Arduino IDE

#include <FastLED.h>

// You must Select exactly one wireless input mode and define it an the command line using
// --build-property
#if !defined(USE_BLUETOOTH) && !defined(USE_ARTNET)
  #error "Must define either USE_BLUETOOTH or USE_ARTNET"
#endif

#include "I2SClocklessLedDriver/I2SClocklessLedDriver.h"
#include "artnetReceiver.hpp"
#include "bluetoothAudio.hpp"
#include "constants.hpp"
#include "spectrumAnalyzer.hpp"

struct {
  int8_t brightnessSlider; // from 0 to 100
  int8_t sensitivitySlider; // from 0 to 100
  int8_t speedSlider; // from 0 to 100
  uint8_t rainbowSwitch; // =1 if switch ON and =0 if OFF
  uint8_t normalizeBandsSwitch; // =1 if switch ON and =0 if OFF
  uint8_t patternLength; // number of LEDs per repeating tile (5..LEDS_PER_STRIP)
} configuration;

void blink(const int delay_ms = 500);

CRGB leds[STRIP_COUNT][LEDS_PER_STRIP];
bool logDebug = false;

TaskHandle_t collectSamplesTask;
TaskHandle_t displayLedsTask;
I2SClocklessLedDriver driver;

#ifdef USE_ARTNET
  static volatile bool artnetToggleRequested = false;
#endif
static constexpr uint8_t brightnesses[] = {8, 16, 32, 64, 32, 16, 8};
// brightnessIndex starts at 3 to match the initial setBrightness(64) in setup()
static volatile uint8_t brightnessIndex = 3;

void IRAM_ATTR buttonInterrupt() {
  static uint32_t pressTime = 0;

  if (digitalRead(0) == LOW) {
    // Falling edge - record press time
    pressTime = millis();
  } else {
    // Rising edge - decide action based on hold duration
    uint32_t elapsed = millis() - pressTime;
    if (elapsed < 50) return; // debounce
    #ifdef USE_ARTNET
      if (elapsed >= 1000) {
        artnetToggleRequested = true;
      } else {
        brightnessIndex = (brightnessIndex + 1) % COUNT_OF(brightnesses);
      }
    #else
      brightnessIndex = (brightnessIndex + 1) % COUNT_OF(brightnesses);
    #endif
  }
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
  driver.setBrightness(brightnesses[brightnessIndex]);

  // The boot button is connected to GPIO0
  pinMode(0, INPUT);
  attachInterrupt(0, buttonInterrupt, CHANGE);

  configuration.brightnessSlider = 25;
  configuration.rainbowSwitch = false;
  configuration.normalizeBandsSwitch = false;
  configuration.speedSlider = 85;
  configuration.sensitivitySlider = 50;
  configuration.patternLength = 20;

  xTaskCreatePinnedToCore(
    collectSamplesFunction,
    "collectSamples",
    8000, // Stack size in words
    nullptr, // Task input parameter
    1, // Priority of the task
    &collectSamplesTask, // Task handle.
    1); // Core where the task should run

#ifdef USE_BLUETOOTH
  setupBluetoothAudio(collectSamplesTask, "Phonic Bloom");
#endif

  // Test all the logic level converter LEDs
  uint8_t hue = 0;
  for (int i = 0; i < 1; ++i) { // Increase this for longer effect
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
    // Stack size in words. From printing uxTaskGetStackHighWaterMark, looks like it's using 2428 words.
    3500, // Stack size in words.
    nullptr, // Task input parameter
    1, // Priority of the task
    &displayLedsTask, // Task handle.
    0); // Core where the task should run
}

void loop() {
  #ifdef USE_ARTNET
    static bool artnetStarted = false;

    if (artnetToggleRequested) {
      artnetToggleRequested = false;
      // ArtNet is starting on first boot - maybe the button is weird? Anyway, that's killing
      // Bluetooth. Let's just disable this until a second after boot.
      if (millis() > 1000 && !artnetStarted) {
        // First time: start WiFi and ArtNet receiver task (blocks up to 10s)
        setupArtnet();
        artnetStarted = true;
        artnetEnabled = true;
      } else {
        artnetEnabled = !artnetEnabled;
        Serial.printf("ArtNet mode %s\n", artnetEnabled ? "enabled" : "disabled");
      }
    }
  #endif

  delay(100);
}

void collectSamplesFunction(void*) {
  while (1) {
    collectSamples();
  }
}

void displayLedsFunction(void*) {
  while (1) {
    #ifdef USE_ARTNET
      if (artnetEnabled && artnetActive) {
        // ArtNet mode: copy incoming pixel buffer to LED array and push to strips
        memcpy(leds, artnetPixels, sizeof(leds));
        driver.showPixels(NO_WAIT);
        delay(25); // ~40 fps
        continue;
      } else {
    #endif
      // Audio-reactive mode (default)
      for (int i = 0; i < 100; ++i) {
        displaySpectrumAnalyzer(
          configuration.brightnessSlider,
          configuration.rainbowSwitch,
          configuration.normalizeBandsSwitch,
          configuration.sensitivitySlider,
          configuration.speedSlider,
          configuration.patternLength);

        if (Serial.available() > 0) {
          logDebug = true;
          while (Serial.available() > 0) {
            Serial.read();
          }
        }
      }
      // Keep the watchdog happy
      delay(1);
    #ifdef USE_ARTNET
    }
    #endif
  }
}

void blink(const int delay_ms) {
  digitalWrite(LED_BUILTIN, HIGH);
  delay(delay_ms);
  digitalWrite(LED_BUILTIN, LOW);
  delay(delay_ms);
}
