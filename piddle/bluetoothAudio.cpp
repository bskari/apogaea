// Only compiled into the main (Bluetooth) image (see ARTNET_IMAGE in piddle.ino).
#ifndef ARTNET_IMAGE
#include "constants.hpp"
#include <Arduino.h>
#include <BluetoothA2DPSink.h>

#include "bluetoothAudio.hpp"
#include "spectrumAnalyzer.hpp"

// One-off: when this song goes completely silent at its break, blank the LEDs instantly instead of
// letting the spectrum shift down to the end of the strips.
static const char* const BLANK_TRACK_TITLE = "touch-no-voices";
static const char* const BLANK_TRACK_TITLE_2 = "Touch (feat. Paul Williams)";
static const uint32_t BLANK_AFTER_MS = (7 * 60 + 35) * 1000; // 7:35 into the song
static const uint32_t BRIGHTNESS_RAMP_END_MS = (7 * 60 + 45) * 1000; // 7:45 into the song
static const int BRIGHTNESS_RAMP_STEP = 10;
static const uint32_t BRIGHTNESS_RAMP_INTERVAL_MS = 500;
static const uint32_t BLANK_LOW_SAMPLE_COUNT = 1000; // consecutive quiet samples
static const float QUIET_SAMPLE_CUTOFF = .20;

static BluetoothA2DPSink a2dp_sink;
static TaskHandle_t micTask = nullptr;

static char lastTitle[64] = "";
// Written from the title callback, read from the audio callback (both Bluetooth stack tasks).
static volatile bool blankTrackPlaying = false; // latest title is BLANK_TRACK_TITLE
// Approximate: starts when the title is received, not when the audio actually starts.
static volatile uint32_t blankTrackStartMs = 0;
static volatile bool blankDone = false; // already blanked for this play of the track
static int16_t maxSample = 0, minSample = 0;
static uint32_t quietSampleRun = 0;

static void resetBlankState() {
  blankTrackPlaying = false;
  blankDone = false;
  quietSampleRun = 0;
}

int getBrightnessBoost() {
  const bool playing = blankTrackPlaying;
  const bool done = blankDone;
  const uint32_t elapsedMs = millis() - blankTrackStartMs;
  if (!bluetoothActive || !playing || done || elapsedMs < BLANK_AFTER_MS || elapsedMs >= BRIGHTNESS_RAMP_END_MS) {
    return 0;
  }
  const uint32_t steps = (elapsedMs - BLANK_AFTER_MS) / BRIGHTNESS_RAMP_INTERVAL_MS;
  return min<uint32_t>(steps * BRIGHTNESS_RAMP_STEP, 255);
}

static void onConnectionStateChanged(esp_a2d_connection_state_t state, void*) {
  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    Serial.println("BT audio connected - switching from microphone");
    if (micTask) {
      vTaskSuspend(micTask);
    }
    bluetoothActive = true;
    resetBlankState();
  } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    Serial.println("BT audio disconnected - reverting to microphone");
    // Clear bluetoothActive before resuming so the display task stops reading
    // BT-style (no sign fix) before the mic task starts writing again.
    bluetoothActive = false;
    // Forget the title so the same song received after reconnecting counts as a new play.
    lastTitle[0] = '\0';
    resetBlankState();
    if (micTask) {
      vTaskResume(micTask);
    }
  }
}

// Logs the track title when the phone reports a new one (AVRCP metadata).
static void onMetadata(uint8_t attributeId, const uint8_t* text) {
  if (attributeId != ESP_AVRC_MD_ATTR_TITLE) {
    return;
  }
  if (strncmp(lastTitle, (const char*)text, sizeof(lastTitle) - 1) == 0) {
    return;
  }
  strlcpy(lastTitle, (const char*)text, sizeof(lastTitle));
  Serial.printf("Track title: %s\n", lastTitle);

  // A title change (re-)arms the blank and restarts the song timer.
  blankDone = false;
  quietSampleRun = 0;
  blankTrackStartMs = millis();
  blankTrackPlaying = strcmp(lastTitle, BLANK_TRACK_TITLE) == 0 ||
                     strcmp(lastTitle, BLANK_TRACK_TITLE_2) == 0;
  if (blankTrackPlaying) {
    maxSample = 0;
    minSample = 0;
  }
}

static void audioDataCallback(const uint8_t* data, uint32_t length) {
  writeBtSamples(data, length);

  if (
    bluetoothActive && blankTrackPlaying && !blankDone &&
    millis() - blankTrackStartMs >= BLANK_AFTER_MS
  ) {
    // The phone volume affects the sample numbers, so we can't just use a
    // static threshold. Instead, we'll start collecting max and min values and
    // if the samples are < some % of the max and min, we'll treat that as
    // quiet.
    const int16_t* samples = reinterpret_cast<const int16_t*>(data);
    const uint32_t frameCount = length / 4;
    for (uint32_t i = 0; i < frameCount; ++i) {
      maxSample = max(max(samples[i * 2], samples[i * 2 + 1]), maxSample);
      minSample = min(min(samples[i * 2], samples[i * 2 + 1]), minSample);
    }

    const int32_t upper = maxSample * QUIET_SAMPLE_CUTOFF;
    const int32_t lower = minSample * QUIET_SAMPLE_CUTOFF;
    for (uint32_t i = 0; i < BLANK_LOW_SAMPLE_COUNT; ++i) {
      if (samples[i * 2] > upper || samples[i * 2 + 1] > upper ||
        samples[i * 2] < lower || samples[i * 2 + 1] < lower
      ) {
        return;
      }
    }

    blankDone = true;
    Serial.println("Blanking");
    requestBlankLeds();
  }
}

void teardownBluetoothAudio() {
  a2dp_sink.end(true); // true = release BT/radio resources
  bluetoothActive = false;
  resetBlankState();
  if (micTask) {
    vTaskResume(micTask);
  }
}

void setupBluetoothAudio(TaskHandle_t collectSamplesTask, const char* deviceName) {
  micTask = collectSamplesTask;
  a2dp_sink.set_on_connection_state_changed(onConnectionStateChanged);
  a2dp_sink.set_avrc_metadata_attribute_mask(ESP_AVRC_MD_ATTR_TITLE);
  a2dp_sink.set_avrc_metadata_callback(onMetadata);
  // false = don't route audio to I2S output, deliver raw PCM to our callback
  a2dp_sink.set_stream_reader(audioDataCallback, false);
  a2dp_sink.start(deviceName);
}
#endif // ARTNET_IMAGE
