// To compile:
// arduino-cli compile --fqbn esp32:esp32:esp32da piddle.ino
// To upload:
// arduino-cli upload -p /dev/ttyUSB0 --fqbn esp32:esp32:esp32da


#include <FastLED.h>
#include <Preferences.h>

// Tested on FastLED 3.7.8, other versions crash
#if FASTLED_VERSION != 3007008
#  warning "Newer versions of FastLED don't work, 3.7.8 is tested and does."
#  warning "If it plays the converter animation then freezes, try 3.7.8."
#endif

#if ESP_ARDUINO_VERSION > ESP_ARDUINO_VERSION_VAL(3, 1, 2)
#  warning "I have only tested with ESP 3.1.2, newer versions crash on BLE config."
// FWIW, 3.1.2 also crashes on classic Bluetooth disconnect
#endif

#include <esp_ota_ops.h>

#include "I2SClocklessLedDriver/I2SClocklessLedDriver.h"
#include "bleConfigService.hpp" // Header only; just the BleConfigField enum used for NVS saving
#include "constants.hpp"
#include "spectrumAnalyzer.hpp"

// Linking WiFi costs ~16 KB of static RAM, which the Bluetooth stack needs, so ArtNet and
// Bluetooth live in two separate firmware images (two OTA app slots) that switch to each other
// with a button press. Compile with -DARTNET_IMAGE (compile.sh --artnet) for the ArtNet image;
// without it you get the main image (Bluetooth audio + BLE config, no WiFi).
#ifdef ARTNET_IMAGE
#  include "artnetReceiver.hpp"
#else
#  include "bluetoothAudio.hpp"
#endif

struct Configuration_t {
  int8_t brightnessSlider; // from 0 to 100
  int8_t sensitivitySlider; // from 0 to 100
  int8_t speedSlider; // from 0 to 100
  uint8_t palette; // color palette index, 0 to 5 (default, rainbow, pastel, fire, ocean, sunset)
  uint8_t normalizeBandsSwitch; // =1 if switch ON and =0 if OFF
  uint8_t rgbButton; // =1 if button pressed, else =0, from 0 to 1
  uint16_t rgb; // bitwise flag for the 15 LED strips that determines if that strip is RGB or GRB
  uint8_t showConverterLedsSwitch; // =1 if switch ON and =0 if OFF
  uint8_t skipStrip; // 0 = show all strips, 1 to 15 = skip that 1-based strip
};
Configuration_t configuration;

void blink(const int delay_ms = 500);

CRGB leds[STRIP_COUNT][LEDS_PER_STRIP];
#ifdef LOG_DEBUG
  bool logDebug = false;
#endif
portMUX_TYPE configMux = portMUX_INITIALIZER_UNLOCKED;

TaskHandle_t collectSamplesTask;
TaskHandle_t displayLedsTask;
I2SClocklessLedDriver driver;

#ifdef ARTNET_IMAGE
static volatile bool artNetMode = false;
volatile bool artNetActive = false;
#endif
// Set from the button interrupt: switch to the other firmware image (main <-> ArtNet).
static volatile bool imageSwitchRequested = false;


// BLE config mode is only entered on request (button press, see buttonInterrupt/bleConfigRequested
// below) rather than on every boot, so normally we boot straight into Bluetooth audio. If nobody
// configures us over BLE within this window of boot or the last received config message, give up
// on BLE and reboot back into Bluetooth audio instead.
const uint32_t BLE_CONFIG_TIMEOUT_MS = 60000;
[[maybe_unused]] static bool bleConfigActive = false;
[[maybe_unused]] static uint32_t bleConfigLastActivityMillis = 0;
static volatile bool bleConfigRequested = false;

// Switching from BLE to classic Bluetooth (A2DP) in the same running process leaves Bluedroid's
// internal state (e.g. the BTU alarm hash maps, torn down in BTU_ShutDown() when BLE stops) in a
// state that isn't reliably rebuilt for the second stack - it crashes with
// "assert failed: hash_map_set" during AVRC service discovery on connect. Rebooting into Bluetooth
// audio mode instead gives Bluedroid a clean, single-stack boot. We use the same approach for
// entering BLE config mode (triggered by the boot button, see buttonInterrupt below) so that it
// also always gets a clean, single-stack boot.
//
// RTC_NOINIT_ATTR (not RTC_DATA_ATTR!) - RTC_DATA_ATTR variables are re-initialized from their
// flash-stored initializer on every boot, including a plain esp_restart(), so a value set just
// before restart would already be lost by the time setup() reads it. RTC_NOINIT_ATTR is left
// completely untouched by the loader, so it actually survives. The tradeoff is it's uninitialized
// garbage on a true power-on, so we only trust it when esp_reset_reason() confirms this was our
// own ESP.restart() call (see setup()).
//
// Both are guarded by a magic number rather than a bool because the other firmware image may lay
// out RTC memory differently, so after an image switch these locations can hold arbitrary leftovers.
static constexpr uint32_t RTC_MAGIC = 0xB10CC0DE;
RTC_NOINIT_ATTR static uint32_t bootIntoBleConfigMagic;
// Snapshot of `configuration` taken right before the restarts above, so settings aren't lost
// across the reboot into BLE config mode or back out of it. Valid only if the magic matches.
RTC_NOINIT_ATTR static Configuration_t rtcSavedConfiguration;
RTC_NOINIT_ATTR static uint32_t rtcSavedConfigurationMagic;

Preferences preferences;
// Debounces Preferences (NVS) writes while the BLE config page is actively streaming slider
// updates (every ~40ms while dragging) - we only persist once activity settles, not on every
// single field update. pendingSaveFields accumulates (1 << BleConfigField) bits across polls
// during the debounce window, so the eventual save only rewrites the fields that actually
// changed instead of every field.
static volatile uint16_t pendingSaveFields = 0;
static uint32_t lastConfigChangeMillis = 0;
const uint32_t CONFIG_SAVE_DEBOUNCE_MS = 1000;

void loadConfigFromPreferences(Configuration_t& config) {
  preferences.begin("piddle", true);
  config.brightnessSlider = preferences.getChar("bri", config.brightnessSlider);
  config.sensitivitySlider = preferences.getChar("sen", config.sensitivitySlider);
  config.speedSlider = preferences.getChar("spd", config.speedSlider);
  if (preferences.isKey("pal")) {
    config.palette = preferences.getUChar("pal", config.palette);
    if (config.palette > 5) config.palette = 0;
  } else if (preferences.getUChar("rbw", 0) == 1) {
    // Migrate the old rainbow on/off setting so existing devices keep their look.
    config.palette = 1;
  }
  config.normalizeBandsSwitch = preferences.getUChar("nor", config.normalizeBandsSwitch);
  config.rgb = preferences.getUShort("rgb", config.rgb);
  config.showConverterLedsSwitch = preferences.getUChar("cnv", config.showConverterLedsSwitch);
  config.skipStrip = preferences.getUChar("skp", config.skipStrip);
  if (config.skipStrip > STRIP_COUNT) config.skipStrip = 0;
  preferences.end();
}

// Writes only the fields whose bit is set in `fields` (1 << BleConfigField), so a slider drag only
// ever rewrites the one NVS key it touched rather than the whole configuration.
void saveConfigFieldsToPreferences(const Configuration_t& config, uint16_t fields) {
  if (fields == 0) {
    return;
  }

  preferences.begin("piddle", false);
  if (fields & (1 << static_cast<uint8_t>(BleConfigField::Brightness))) {
    preferences.putChar("bri", config.brightnessSlider);
  }
  if (fields & (1 << static_cast<uint8_t>(BleConfigField::Sensitivity))) {
    preferences.putChar("sen", config.sensitivitySlider);
  }
  if (fields & (1 << static_cast<uint8_t>(BleConfigField::Speed))) {
    preferences.putChar("spd", config.speedSlider);
  }
  if (fields & (1 << static_cast<uint8_t>(BleConfigField::Palette))) {
    preferences.putUChar("pal", config.palette);
  }
  if (fields & (1 << static_cast<uint8_t>(BleConfigField::NormalizeBands))) {
    preferences.putUChar("nor", config.normalizeBandsSwitch);
  }
  // RgbButton is momentary UI state, not a persisted setting - intentionally not saved.
  if (fields & (1 << static_cast<uint8_t>(BleConfigField::Rgb))) {
    preferences.putUShort("rgb", config.rgb);
  }
  if (fields & (1 << static_cast<uint8_t>(BleConfigField::ShowConverterLeds))) {
    preferences.putUChar("cnv", config.showConverterLedsSwitch);
  }
  if (fields & (1 << static_cast<uint8_t>(BleConfigField::SkipStrip))) {
    preferences.putUChar("skp", config.skipStrip);
  }
  preferences.end();
}

// Minimum time the button must be held to count as a deliberate press. USB-serial auto-reset
// circuits (DTR/RTS) pulse GPIO0 for only a few ms to tens of ms when a monitor connects.
static constexpr uint32_t MIN_BUTTON_HOLD_MS = 200;

// Advances to the next mode (Bluetooth audio -> BLE config -> ArtNet -> ...). Shared by the button
// interrupt and the "mode" serial command; loop() acts on the flags.
static inline void IRAM_ATTR requestModeSwitch() {
#ifdef ARTNET_IMAGE
  imageSwitchRequested = true;
#else
  if (bleConfigActive) {
    imageSwitchRequested = true;
  } else {
    bleConfigRequested = true;
  }
#endif
}

void IRAM_ATTR buttonInterrupt() {
  // 0 means "no valid press in progress". (millis() can legitimately be 0 only at the very start of
  // boot, which the guard below ignores anyway.)
  static uint32_t pressTime = 0;

  // This button starts pressed? Just ignore it at boot
  if (millis() < 1000) {
    pressTime = 0;
    return;
  }

  if (digitalRead(0) == LOW) {
    // Falling edge - record press time
    pressTime = millis();
  } else {
    // Rising edge - only valid if we saw the matching falling edge, and it was held long enough
    if (pressTime == 0) {
      return;
    }
    uint32_t elapsed = millis() - pressTime;
    pressTime = 0;
    if (elapsed < MIN_BUTTON_HOLD_MS) {
      return; // glitch / debounce
    }

    requestModeSwitch();
  }
}

// Persists the current configuration for the next boot of this same image (it survives the reboot
// in RTC memory, and is also written to NVS).
[[maybe_unused]] static void saveStateForReboot() {
  portENTER_CRITICAL(&configMux);
  rtcSavedConfiguration = configuration;
  portEXIT_CRITICAL(&configMux);
  rtcSavedConfigurationMagic = RTC_MAGIC;
  saveConfigFieldsToPreferences(configuration, pendingSaveFields);
  pendingSaveFields = 0;
}

// Selects the other OTA app slot (main <-> ArtNet) as the next boot partition. The other image
// must have been flashed (see upload.sh); otherwise this fails and we stay on the current one.
static bool selectOtherImage() {
  const esp_partition_t* other = esp_ota_get_next_update_partition(nullptr);
  if (other == nullptr) {
    Serial.println("No other OTA partition");
    return false;
  }
  const esp_err_t err = esp_ota_set_boot_partition(other);
  if (err != ESP_OK) {
    Serial.printf("Can't boot other image (%s) - is it flashed?\n", esp_err_to_name(err));
    return false;
  }
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println("Setup");

  // The RTC_NOINIT values are garbage on a genuine power-on - only trust them when the reset reason
  // confirms it was our own ESP.restart() call, and the magic number matches. A genuine power-on
  // always boots straight into the default mode; BLE config mode is only entered when requested
  // via the boot button.
  const bool softReset = esp_reset_reason() == ESP_RST_SW;
  [[maybe_unused]] const bool enterBleConfig = softReset && bootIntoBleConfigMagic == RTC_MAGIC;
  const bool restoreRtcConfig = softReset && rtcSavedConfigurationMagic == RTC_MAGIC;
  bootIntoBleConfigMagic = 0;
  rtcSavedConfigurationMagic = 0;

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
  configuration.palette = 0;
  configuration.normalizeBandsSwitch = true;
  configuration.speedSlider = DEFAULT_SPEED;
  configuration.sensitivitySlider = DEFAULT_SENSITIVITY;
  configuration.rgbButton = 0;
  configuration.rgb = 0;
  configuration.showConverterLedsSwitch = DEFAULT_SHOW_CONVERTER_LEDS;
  configuration.skipStrip = 0;

  // Overlay whatever was last saved to NVS (if anything) on top of the hardcoded defaults above.
  loadConfigFromPreferences(configuration);

  if (restoreRtcConfig) {
    // Restore whatever was configured right before this reboot (into or out of BLE config mode).
    configuration = rtcSavedConfiguration;
  }

  xTaskCreatePinnedToCore(
    collectSamplesFunction,
    "collectSamples",
    // NOTE: on ESP32 Arduino the stack size is in BYTES, not words. This was previously 8000
    // bytes, which was starving the BT stack of internal RAM and causing a crash on BLE connect.
    // uxTaskGetStackHighWaterMark returns the minimum FREE stack (bytes), not the amount used -
    // see the periodic "Stack HWM" log in loop().
    1500, // Stack size in bytes
    nullptr, // Task input parameter
    1, // Priority of the task
    &collectSamplesTask, // Task handle.
    1); // Core where the task should run

#ifdef ARTNET_IMAGE
  Serial.println("ArtNet image: starting ArtNet");
  artNetPixels = reinterpret_cast<CRGB*>(leds);
  setupArtNet(collectSamplesTask, &artNetMode);
#else
  if (enterBleConfig) {
    bleConfigActive = true;
    configuration.rgbButton = 0;
    Serial.println("Setting up BLE config service (requested via button press)");
    setupBleConfigService();
    bleConfigLastActivityMillis = millis();
    Serial.println("Done setting up BLE config service");
  } else {
    bleConfigActive = false;
    Serial.println("Booting into Bluetooth audio");
    configuration.rgbButton = 0;
    setupBluetoothAudio(collectSamplesTask, "Phonic Bloom");
  }
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

  // Pinned to core 1 (not core 0) - core 0 also runs the BT/BLE stack tasks, and a tight
  // LED-render/DMA loop there starves/corrupts the BT stack and crashes it on connect.
  xTaskCreatePinnedToCore(
    displayLedsFunction,
    "displayLeds",
    // Stack size in bytes. An earlier measurement of 2428 from uxTaskGetStackHighWaterMark was
    // ambiguous (that function returns minimum FREE bytes, not used) - check the periodic
    // "Stack HWM" log in loop() with Bluetooth connected to see the real margin.
    3500, // Stack size in bytes.
    nullptr, // Task input parameter
    1, // Priority of the task
    &displayLedsTask, // Task handle.
    1); // Core where the task should run
  Serial.println("Done with setup");
}

// Reads a line from Serial. "mode" switches modes, like pressing the button. With LOG_DEBUG, any
// other input triggers a debug dump.
static void handleSerialCommands() {
  static char line[16];
  static size_t len = 0;
  while (Serial.available() > 0) {
    const char c = Serial.read();
    if (c == '\n' || c == '\r') {
      line[len] = '\0';
      if (strcmp(line, "mode") == 0) {
        Serial.println("Serial command: switching mode");
        requestModeSwitch();
      }
#ifdef LOG_DEBUG
      else if (len > 0) {
        logDebug = true;
      }
#endif
      len = 0;
    } else if (len < sizeof(line) - 1) {
      line[len++] = c;
    }
  }
}

void loop() {
  handleSerialCommands();
#ifndef ARTNET_IMAGE
  if (bleConfigRequested) {
    bleConfigRequested = false;
    if (!bleConfigActive) {
      Serial.println("Button pressed - rebooting into BLE config mode");
      teardownBluetoothAudio();
      saveStateForReboot();
      bootIntoBleConfigMagic = RTC_MAGIC;
      delay(100); // let the log line above actually flush over serial
      ESP.restart();
    }
  }

  if (bleConfigActive) {
    if (!isBleClientConnected() && millis() - bleConfigLastActivityMillis > BLE_CONFIG_TIMEOUT_MS) {
      Serial.println("Rebooting into Bluetooth audio");
      teardownBleConfigService();
      saveStateForReboot();
      delay(100); // let the log line above actually flush over serial
      ESP.restart();
    } else {
      BleConfigMessage_t bleMsg;
      uint16_t changedFields;
      if (pollBleConfigService(bleMsg, changedFields)) {
        bleConfigLastActivityMillis = millis();
        #ifdef LOG_DEBUG
          Serial.printf(
            "bri:%d sen:%d spd:%d pal:%d nor:%d rgbButton:%d rgb:%04x cnv:%d\n",
            bleMsg.brightness,
            bleMsg.sensitivity,
            bleMsg.speed,
            bleMsg.palette,
            bleMsg.normalizeBands,
            bleMsg.rgbButton,
            bleMsg.rgb,
            bleMsg.showConverterLeds
          );
        #endif
        portENTER_CRITICAL(&configMux);
        configuration.brightnessSlider = bleMsg.brightness;
        configuration.sensitivitySlider = bleMsg.sensitivity;
        configuration.speedSlider = bleMsg.speed;
        configuration.palette = bleMsg.palette;
        configuration.normalizeBandsSwitch = bleMsg.normalizeBands;
        configuration.rgbButton = bleMsg.rgbButton;
        configuration.rgb = bleMsg.rgb;
        configuration.showConverterLedsSwitch = bleMsg.showConverterLeds;
        configuration.skipStrip = bleMsg.skipStrip;
        portEXIT_CRITICAL(&configMux);

        pendingSaveFields |= changedFields;
        lastConfigChangeMillis = millis();
      }
    }
  }

#endif // !ARTNET_IMAGE

  if (imageSwitchRequested) {
    imageSwitchRequested = false;
    Serial.println("Button pressed - switching firmware image");
    if (selectOtherImage()) {
      // The other image reads its configuration from NVS (RTC memory isn't shared reliably).
      pendingSaveFields = 0xFFFF;
      saveConfigFieldsToPreferences(configuration, pendingSaveFields);
      pendingSaveFields = 0;
#ifndef ARTNET_IMAGE
      teardownBleConfigService();
#endif
      delay(100); // let the log line above actually flush over serial
      ESP.restart();
    }
  }

  // Debounced NVS save: while BLE config is actively streaming slider updates (every ~40ms while
  // dragging), only persist once changes have settled, to avoid hammering flash - and even then,
  // only the specific field(s) that changed rather than the whole configuration.
  if (pendingSaveFields != 0 && millis() - lastConfigChangeMillis > CONFIG_SAVE_DEBOUNCE_MS) {
    saveConfigFieldsToPreferences(configuration, pendingSaveFields);
    pendingSaveFields = 0;
  }

  #ifdef LOG_DEBUG
    // Periodically log stack/heap headroom to help diagnose crashes (e.g. when Bluetooth connects).
    // Stack values are minimum FREE bytes ever seen for that task; low numbers mean near-overflow.
    static uint32_t lastDiagnosticsMillis = 0;
    if (millis() - lastDiagnosticsMillis >= 2000) {
      lastDiagnosticsMillis = millis();
      Serial.printf(
        "Stack HWM (min free bytes): collect=%lu display=%lu loop=%lu | heap free=%lu min=%lu largest=%lu\n",
        (unsigned long)(collectSamplesTask ? uxTaskGetStackHighWaterMark(collectSamplesTask) : 0),
        (unsigned long)(displayLedsTask ? uxTaskGetStackHighWaterMark(displayLedsTask) : 0),
        (unsigned long)uxTaskGetStackHighWaterMark(nullptr),
        (unsigned long)ESP.getFreeHeap(),
        (unsigned long)ESP.getMinFreeHeap(),
        (unsigned long)ESP.getMaxAllocHeap()
      );
    }
  #endif

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
#ifdef ARTNET_IMAGE
    if (artNetMode && artNetActive) {
      // ArtNet receiver writes directly into leds (artNetPixels points at leds)
      const bool syncMode = artNetSyncActive();
      if (syncMode && !artNetTakeSync()) {
        vTaskDelay(1 / portTICK_PERIOD_MS);
        continue;
      }
      while (xSemaphoreTake(artNetPixelsMutex, 0) == pdFALSE) {
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
      xSemaphoreGive(artNetPixelsMutex);
      if (!syncMode) {
        delay(25); // ~40 fps
      }
      continue;
    }
#endif

    // Audio-reactive mode (default, and resumed after ArtNet timeout)
    for (int i = 0; i < 10; ++i) {
      if (configuration.rgbButton) {
        fill_solid(reinterpret_cast<CRGB*>(leds), STRIP_COUNT * LEDS_PER_STRIP, CRGB::Black);
        for (int i = 0; i < STRIP_COUNT; ++i) {
          // The skipped strip (1-based, 0 = none) shows blue instead of red.
          const bool skipped = configuration.skipStrip == i + 1;
          leds[i][1] = leds[i][2] = leds[i][3] = skipped ? CRGB::Blue : CRGB::Red;
        }
        swapChannels(configuration.rgb);
        driver.showPixels(WAIT);
        delay(100);
      } else {
        displaySpectrumAnalyzer(
          configuration.brightnessSlider,
          configuration.palette,
          configuration.normalizeBandsSwitch,
          configuration.sensitivitySlider,
          configuration.speedSlider,
          configuration.rgb,
          configuration.showConverterLedsSwitch,
          configuration.skipStrip);
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
