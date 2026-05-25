// To compile:
// arduino-cli compile --fqbn esp32:esp32:esp32da piddle.ino
// To upload:
// arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32da

#include <FastLED.h>

#include "I2SClocklessLedDriver/I2SClocklessLedDriver.h"
#include "artnetReceiver.hpp"
#include "bluetoothAudio.hpp"
#include "constants.hpp"
#include "radioReceiver.hpp"
#include "spectrumAnalyzer.hpp"

struct {
  int8_t brightnessSlider; // from 0 to 100
  int8_t sensitivitySlider; // from 0 to 100
  int8_t speedSlider; // from 0 to 100
  uint8_t rainbowSwitch; // =1 if switch ON and =0 if OFF
  uint8_t normalizeBandsSwitch; // =1 if switch ON and =0 if OFF
  uint8_t patternLength; // number of LEDs per repeating tile (5..LEDS_PER_STRIP)
  uint8_t tileOffset;   // how many history positions each successive tile shifts (0 = identical copies)
} configuration;

void blink(const int delay_ms = 500);

CRGB leds[STRIP_COUNT][LEDS_PER_STRIP];
bool logDebug = false;
portMUX_TYPE configMux = portMUX_INITIALIZER_UNLOCKED;

TaskHandle_t collectSamplesTask;
TaskHandle_t displayLedsTask;
I2SClocklessLedDriver driver;

static volatile bool switchToArtnetRequested = false;
static volatile bool artnetMode = false;

void IRAM_ATTR buttonInterrupt() {
  static uint32_t pressTime = 0;

  // This button starts pressed? Just ignore it at boot
  if (millis() < 1000) {
    return;
  }

  if (digitalRead(0) == LOW) {
    // Falling edge - record press time
    pressTime = millis();
  } else {
    // Rising edge - decide action based on hold duration
    uint32_t elapsed = millis() - pressTime;
    if (elapsed < 50) {
      return; // debounce
    }
    switchToArtnetRequested = true;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Setup");

  // This had to be done first, but I think I fixed the bug that was causing problems? I don't want
  // to test if it's fixed, so I'm leaving it first now
  setupSpectrumAnalyzer();
  Serial.println("Setting up radio receiver");
  setupRadioReceiver();
  Serial.println("Done setting up radio receiver");

  //analogReference(AR_DEFAULT); // Not on ESP32?
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(VOLTAGE_PIN, INPUT);

  driver.initled(reinterpret_cast<uint8_t*>(leds), LED_PINS, COUNT_OF(LED_PINS), LEDS_PER_STRIP, ORDER_RGB);
  // The boot button is connected to GPIO0
  pinMode(0, INPUT);
  attachInterrupt(0, buttonInterrupt, CHANGE);

  configuration.brightnessSlider = 25;
  configuration.rainbowSwitch = false;
  configuration.normalizeBandsSwitch = true;
  configuration.speedSlider = 60;
  configuration.sensitivitySlider = 20;
  configuration.patternLength = 70;
  configuration.tileOffset = 0;

  xTaskCreatePinnedToCore(
    collectSamplesFunction,
    "collectSamples",
    8000, // Stack size in words
    nullptr, // Task input parameter
    1, // Priority of the task
    &collectSamplesTask, // Task handle.
    1); // Core where the task should run

  setupBluetoothAudio(collectSamplesTask, "Phonic Bloom");

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
  if (switchToArtnetRequested && !artnetMode) {
    switchToArtnetRequested = false;
    Serial.println("Switching to ArtNet mode");
    teardownBluetoothAudio();
    delay(500);
    artnetPixels = reinterpret_cast<CRGB*>(leds);

    // Suspend the display task and push black to all strips before WiFi init.
    // WiFi can drive pin 2 (which is also an LED output) during softAP startup,
    // causing the WS2812B on that strip to latch a white frame.
    vTaskSuspend(displayLedsTask);
    memset(leds, 0, sizeof(leds));
    driver.showPixels(WAIT);

    setupArtnet();

    // Clear again after WiFi init in case any strip latched interference during init.
    memset(leds, 0, sizeof(leds));
    driver.showPixels(WAIT);

    artnetMode = true;
    vTaskResume(displayLedsTask);
  }

  RadioConfigMessage_t radioMsg;
  if (pollRadioReceiver(radioMsg)) {
    Serial.println("Received message");
    portENTER_CRITICAL(&configMux);
    configuration.brightnessSlider = radioMsg.brightness;
    configuration.sensitivitySlider = radioMsg.sensitivity;
    configuration.speedSlider = radioMsg.speed;
    configuration.rainbowSwitch = radioMsg.rainbow;
    configuration.normalizeBandsSwitch = radioMsg.normalizeBands;
    portEXIT_CRITICAL(&configMux);
  }

  delay(100);
}

void collectSamplesFunction(void*) {
  while (1) {
    collectSamples();
  }
}

void displayLedsFunction(void*) {
  while (1) {
    if (artnetMode && artnetActive) {
      // ArtNet receiver writes directly into leds (artnetPixels points at leds)
      driver.showPixels(NO_WAIT);
      delay(25); // ~40 fps
      continue;
    }

    // Audio-reactive mode (default, and fallback when no ArtNet signal)
    for (int i = 0; i < 100; ++i) {
      displaySpectrumAnalyzer(
        configuration.brightnessSlider,
        configuration.rainbowSwitch,
        configuration.normalizeBandsSwitch,
        configuration.sensitivitySlider,
        configuration.speedSlider,
        configuration.patternLength,
        configuration.tileOffset);

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
  delay(delay_ms);
  digitalWrite(LED_BUILTIN, LOW);
  delay(delay_ms);
}
