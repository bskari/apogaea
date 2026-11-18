// Only compiled into the main (Bluetooth) image (see ARTNET_IMAGE in piddle.ino).
#ifndef ARTNET_IMAGE
#include "constants.hpp"
#include <Arduino.h>
#include <BluetoothA2DPSink.h>

#include "bluetoothAudio.hpp"
#include "spectrumAnalyzer.hpp"

static BluetoothA2DPSink a2dp_sink;
static TaskHandle_t micTask = nullptr;

static void onConnectionStateChanged(esp_a2d_connection_state_t state, void*) {
  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    Serial.println("BT audio connected - switching from microphone");
    if (micTask) {
      vTaskSuspend(micTask);
    }
    bluetoothActive = true;
  } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    Serial.println("BT audio disconnected - reverting to microphone");
    // Clear bluetoothActive before resuming so the display task stops reading
    // BT-style (no sign fix) before the mic task starts writing again.
    bluetoothActive = false;
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
  static char lastTitle[128] = "";
  if (strncmp(lastTitle, (const char*)text, sizeof(lastTitle) - 1) == 0) {
    return;
  }
  strlcpy(lastTitle, (const char*)text, sizeof(lastTitle));
  Serial.printf("Track title: %s\n", lastTitle);
}

static void audioDataCallback(const uint8_t* data, uint32_t length) {
  writeBtSamples(data, length);
}

void teardownBluetoothAudio() {
  a2dp_sink.end(true); // true = release BT/radio resources
  bluetoothActive = false;
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
