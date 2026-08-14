// To compile:
// arduino-cli compile --fqbn esp32:esp32:esp32da piddle.ino
// To upload:
// arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32da



#include <FastLED.h>

// Tested on FastLED 3.7.8, other versions crash
#if FASTLED_VERSION != 3007008
#  warning "Newer versions of FastLED don't work, 3.7.8 is tested and does."
#  warning "If it plays the converter animation then freezes, try 3.7.8."
#endif

#include "I2SClocklessLedDriver/I2SClocklessLedDriver.h"
#include "bleConfigService.hpp"
#include "bluetoothAudio.hpp"
#include "constants.hpp"
#include "spectrumAnalyzer.hpp"

#if ENABLE_ARTNET
#  include "artnetReceiver.hpp"
#endif

struct Configuration_t {
  int8_t brightnessSlider; // from 0 to 100
  int8_t sensitivitySlider; // from 0 to 100
  int8_t speedSlider; // from 0 to 100
  uint8_t rainbowSwitch; // =1 if switch ON and =0 if OFF
  uint8_t normalizeBandsSwitch; // =1 if switch ON and =0 if OFF
  uint8_t rgbButton; // =1 if button pressed, else =0, from 0 to 1
  uint16_t rgb; // bitwise flag for the 15 LED strips that determines if that strip is RGB or GRB
  uint8_t patternLength; // number of LEDs per repeating tile (5..LEDS_PER_STRIP)
  uint8_t tileOffset;   // how many history positions each successive tile shifts (0 = identical copies)
  uint8_t showConverterLedsSwitch; // =1 if switch ON and =0 if OFF
};
Configuration_t configuration;

void blink(const int delay_ms = 500);

CRGB leds[STRIP_COUNT][LEDS_PER_STRIP];
bool logDebug = false;
portMUX_TYPE configMux = portMUX_INITIALIZER_UNLOCKED;

TaskHandle_t collectSamplesTask;
TaskHandle_t displayLedsTask;
I2SClocklessLedDriver driver;

static volatile bool switchToArtnetRequested = false;
static volatile bool artnetMode = false;

// At boot we only run the BLE config service (BLE and classic BT A2DP can't reliably share the
// radio - see git history). If nobody configures us over BLE within this window of boot or the
// last received config message, give up on BLE and switch to Bluetooth audio instead.
const uint32_t BLE_CONFIG_TIMEOUT_MS = 30000;
static bool bleConfigActive = true;
static uint32_t bleConfigLastActivityMillis = 0;

// Switching from BLE to classic Bluetooth (A2DP) in the same running process leaves Bluedroid's
// internal state (e.g. the BTU alarm hash maps, torn down in BTU_ShutDown() when BLE stops) in a
// state that isn't reliably rebuilt for the second stack - it crashes with
// "assert failed: hash_map_set" during AVRC service discovery on connect. Rebooting into Bluetooth
// audio mode instead gives Bluedroid a clean, single-stack boot.
//
// RTC_NOINIT_ATTR (not RTC_DATA_ATTR!) - RTC_DATA_ATTR variables are re-initialized from their
// flash-stored initializer on every boot, including a plain esp_restart(), so a value set just
// before restart would already be lost by the time setup() reads it. RTC_NOINIT_ATTR is left
// completely untouched by the loader, so it actually survives. The tradeoff is it's uninitialized
// garbage on a true power-on, so we only trust it when esp_reset_reason() confirms this was our
// own ESP.restart() call (see setup()).
RTC_NOINIT_ATTR static bool bootIntoBluetoothAudio;
// Snapshot of `configuration` taken right before the restart above, so settings from BLE aren't
// lost when we reboot into Bluetooth audio mode.
RTC_NOINIT_ATTR static Configuration_t rtcSavedConfiguration;

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

  // bootIntoBluetoothAudio lives in RTC_NOINIT_ATTR memory, so it's garbage on a genuine power-on -
  // only trust it when the reset reason confirms it was actually our own ESP.restart() call below.
  if (esp_reset_reason() != ESP_RST_SW) {
    bootIntoBluetoothAudio = false;
  }

  // This had to be done first, but I think I fixed the bug that was causing problems? I don't want
  // to test if it's fixed, so I'm leaving it first now
  setupSpectrumAnalyzer();

  //analogReference(AR_DEFAULT); // Not on ESP32?
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(VOLTAGE_PIN, INPUT);

  driver.initled(reinterpret_cast<uint8_t*>(leds), LED_PINS, COUNT_OF(LED_PINS), LEDS_PER_STRIP, ORDER_RGB);
  // The boot button is connected to GPIO0
  pinMode(0, INPUT);
  attachInterrupt(0, buttonInterrupt, CHANGE);

  configuration.brightnessSlider = DEFAULT_BRIGHTNESS;
  configuration.rainbowSwitch = false;
  configuration.normalizeBandsSwitch = true;
  configuration.speedSlider = DEFAULT_SPEED;
  configuration.sensitivitySlider = DEFAULT_SENSITIVITY;
  configuration.rgbButton = 0;
  configuration.rgb = 0;
  configuration.patternLength = DEFAULT_PATTERN_LENGTH;
  configuration.tileOffset = DEFAULT_TILE_OFFSET;
  configuration.showConverterLedsSwitch = DEFAULT_SHOW_CONVERTER_LEDS;

  if (bootIntoBluetoothAudio) {
    // Restore whatever was configured over BLE before we rebooted into Bluetooth audio mode.
    configuration = rtcSavedConfiguration;
  }

  xTaskCreatePinnedToCore(
    collectSamplesFunction,
    "collectSamples",
    // Measured with uxTaskGetStackHighWaterMark: only ~672 words actually used out of the 8000
    // this was previously allocated. That oversized stack was starving the BT stack of internal
    // RAM, causing a crash on BLE connect.
    1500, // Stack size in words
    nullptr, // Task input parameter
    1, // Priority of the task
    &collectSamplesTask, // Task handle.
    1); // Core where the task should run

  if (bootIntoBluetoothAudio) {
    bootIntoBluetoothAudio = false;
    bleConfigActive = false;
    Serial.println("Booting straight into Bluetooth audio (BLE config skipped this boot)");
    configuration.rgbButton = 0;
    setupBluetoothAudio(collectSamplesTask, "Phonic Bloom");
  } else {
    Serial.println("Setting up BLE config service");
    setupBleConfigService();
    bleConfigLastActivityMillis = millis();
    Serial.println("Done setting up BLE config service");
  }

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

  // Pinned to core 1 (not core 0) - core 0 also runs the BT/BLE stack tasks, and a tight
  // LED-render/DMA loop there starves/corrupts the BT stack and crashes it on connect.
  xTaskCreatePinnedToCore(
    displayLedsFunction,
    "displayLeds",
    // Stack size in words. From printing uxTaskGetStackHighWaterMark, looks like it's using 2428 words.
    3500, // Stack size in words.
    nullptr, // Task input parameter
    1, // Priority of the task
    &displayLedsTask, // Task handle.
    1); // Core where the task should run
  Serial.println("Done with setup");
}

void loop() {
  if (switchToArtnetRequested && !artnetMode) {
    switchToArtnetRequested = false;
#if ENABLE_ARTNET
    Serial.println("Switching to ArtNet mode");
    if (bleConfigActive) {
      teardownBleConfigService();
      bleConfigActive = false;
    } else {
      teardownBluetoothAudio();
    }
    delay(500);
    artnetPixels = reinterpret_cast<CRGB*>(leds);

    // Suspend the display task and push black to all strips before WiFi init.
    // WiFi can drive pin 2 (which is also an LED output) during station connect,
    // causing the WS2812B on that strip to latch a white frame.
    vTaskSuspend(displayLedsTask);
    ::memset(leds, 0, sizeof(leds));
    driver.showPixels(WAIT);

    setupArtnet(collectSamplesTask, &artnetMode);

    // Clear again after WiFi init in case any strip latched interference during init.
    ::memset(leds, 0, sizeof(leds));
    driver.showPixels(WAIT);

    artnetMode = true;
    vTaskResume(displayLedsTask);
#else
    Serial.println("ArtNet is disabled (ENABLE_ARTNET=0)");
#endif
  }

  if (bleConfigActive) {
    if (!isBleClientConnected() && millis() - bleConfigLastActivityMillis > BLE_CONFIG_TIMEOUT_MS) {
      Serial.println("Rebooting into Bluetooth audio");
      teardownBleConfigService();
      portENTER_CRITICAL(&configMux);
      rtcSavedConfiguration = configuration;
      portEXIT_CRITICAL(&configMux);
      bootIntoBluetoothAudio = true;
      delay(100); // let the log line above actually flush over serial
      ESP.restart();
    } else {
      BleConfigMessage_t bleMsg;
      if (pollBleConfigService(bleMsg)) {
        bleConfigLastActivityMillis = millis();
        Serial.printf(
          "bri:%d sen:%d spd:%d rbw:%d nor:%d rgbButton:%d rgb:%04x cnv:%d\n",
          bleMsg.brightness,
          bleMsg.sensitivity,
          bleMsg.speed,
          bleMsg.rainbow,
          bleMsg.normalizeBands,
          bleMsg.rgbButton,
          bleMsg.rgb,
          bleMsg.showConverterLeds
        );
        portENTER_CRITICAL(&configMux);
        configuration.brightnessSlider = bleMsg.brightness;
        configuration.sensitivitySlider = bleMsg.sensitivity;
        configuration.speedSlider = bleMsg.speed;
        configuration.patternLength = bleMsg.patternLength;
        configuration.tileOffset = bleMsg.tileOffset;
        configuration.rainbowSwitch = bleMsg.rainbow;
        configuration.normalizeBandsSwitch = bleMsg.normalizeBands;
        configuration.rgbButton = bleMsg.rgbButton;
        configuration.rgb = bleMsg.rgb;
        configuration.showConverterLedsSwitch = bleMsg.showConverterLeds;
        portEXIT_CRITICAL(&configMux);
      }
    }
  }

  delay(100);
}

void collectSamplesFunction(void*) {
  while (1) {
    collectSamples();
  }
}

void swapChannels(uint16_t rgbBitFlag) {
  for (int i = 0; i < STRIP_COUNT; ++i) {
    if ((rgbBitFlag >> i) & 1) {
      // Skip the converter LEDs
      for (int j = 1; j < LEDS_PER_STRIP; ++j) {
        const uint8_t tmp = leds[i][j].r;
        leds[i][j].r = leds[i][j].g;
        leds[i][j].g = tmp;
      }
    }
  }
}

void displayLedsFunction(void*) {
  while (1) {
#if ENABLE_ARTNET
    if (artnetMode && artnetActive) {
      // ArtNet receiver writes directly into leds (artnetPixels points at leds)
      while (xSemaphoreTake(artnetPixelsMutex, 0) == pdFALSE) {
        vTaskDelay(1 / portTICK_PERIOD_MS);
      }
      swapChannels(configuration.rgb);
      if (!configuration.showConverterLedsSwitch) {
        for (int i = 0; i < STRIP_COUNT; ++i) {
          leds[i][0] = CRGB::Black;
        }
      }
      driver.showPixels();
      swapChannels(configuration.rgb);
      xSemaphoreGive(artnetPixelsMutex);
      delay(25); // ~40 fps
      continue;
    }
#endif

    // Audio-reactive mode (default, and resumed after ArtNet timeout)
    for (int i = 0; i < 10; ++i) {
      if (configuration.rgbButton) {
        fill_solid(reinterpret_cast<CRGB*>(leds), STRIP_COUNT * LEDS_PER_STRIP, CRGB::Black);
        for (int i = 0; i < STRIP_COUNT; ++i) {
          leds[i][1] = leds[i][2] = leds[i][3] = CRGB::Red;
        }
        swapChannels(configuration.rgb);
        driver.showPixels(WAIT);
        delay(100);
      } else {
        displaySpectrumAnalyzer(
          configuration.brightnessSlider,
          configuration.rainbowSwitch,
          configuration.normalizeBandsSwitch,
          configuration.sensitivitySlider,
          configuration.speedSlider,
          configuration.patternLength,
          configuration.tileOffset,
          configuration.rgb,
          configuration.showConverterLedsSwitch);
      }

      //if (Serial.available() > 0) {
      //  logDebug = true;
      //  while (Serial.available() > 0) {
      //    Serial.read();
      //  }
      //}
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
