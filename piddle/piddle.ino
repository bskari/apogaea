// To compile:
// arduino-cli compile --fqbn esp32:esp32:esp32da piddle.ino
// To upload:
// arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32da
// I don't know how to set frequency and stuff easily, might need to use the Arduino IDE

#define SHOW_VOLTAGE 0

#include <FastLED.h>

#include "I2SClocklessLedDriver/I2SClocklessLedDriver.h"
#include "constants.hpp"
#include "spectrumAnalyzer.hpp"

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

  xTaskCreatePinnedToCore(
    collectSamplesFunction,
    "collectSamples",
    4000, // Stack size in words
    nullptr, // Task input parameter
    1, // Priority of the task
    &collectSamplesTask, // Task handle.
    1); // Core where the task should run

  // Test all the logic level converter LEDs
  for (int i = 0; i < 5; ++i) {
    for (uint8_t hue = 0; hue < 240; hue += 10) {
      fill_solid(reinterpret_cast<CRGB*>(leds), STRIP_COUNT * LEDS_PER_STRIP, CRGB::Black);
      for (int strip = 0; strip < STRIP_COUNT; ++strip) {
        leds[strip][0] = CHSV(hue + strip * (255 / STRIP_COUNT), 255, 64);
      }
      driver.showPixels();
      delay(10);
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

void loop() {
  delay(10000);
}

void collectSamplesFunction(void*) {
  while (1) {
    collectSamples();
  }
}

void displayLedsFunction(void*) {
  while (1) {
    for (int i = 0; i < 100; ++i) {
      displaySpectrumAnalyzer();

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
