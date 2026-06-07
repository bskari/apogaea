#include "radioReceiver.hpp"
#include <RH_ASK.h>

// Must specify RH_INVALID_PIN for RX and PTT pins
static RH_ASK rf(2000, 39, RH_INVALID_PIN, RH_INVALID_PIN);

void setupRadioReceiver() {
  // Note! If this keeps rebooting, here's why. As of 2026-05-17, RadioHead's RH_ASK uses the old
  // ESP32 timer API timerBegin(num, divider, count_up), but ESP32 Arduino core v3.x changed it to
  // timerBegin(frequency_hz). This mismatch causes an immediate crash/reset on init().
  //
  // Check your core version:
  // arduino-cli core list
  //
  // If you're on core 3.x, you need to Patch RH_ASK.cpp in your library. Find the ESP32 section in
  // RH_ASK.cpp (look for #ifdef ESP32) and replace the old timerBegin call with the new API.
  //
  // Old (core 2.x):
  // timer = timerBegin(0, 80, true);
  // timerAttachInterrupt(timer, &isr, true);
  // timerAlarmWrite(timer, 1000000 / 8 / speed, true);
  // timerAlarmEnable(timer);
  //
  // New (core 3.x):
  // timer = timerBegin(1000000);  // 1 MHz
  // timerAttachInterrupt(timer, &isr);
  // timerAlarm(timer, 1000000 / 8 / speed, true, 0);
  if (!rf.init()) {
    Serial.println("RH_ASK init failed");
  }
}

bool pollRadioReceiver(RadioConfigMessage_t& out) {
  uint8_t buf[sizeof(RadioConfigMessage_t)];
  uint8_t buflen = sizeof(buf);

  if (!rf.recv(buf, &buflen)) {
    return false;
  }
  if (buflen != sizeof(RadioConfigMessage_t)) {
    Serial.printf("Wrong radio message size: %d, expected %d\n", buflen, sizeof(RadioConfigMessage_t));
    return false;
  }

  RadioConfigMessage_t msg;
  memcpy(&msg, buf, sizeof(msg));

  if (msg.brightness < 0 || msg.brightness > 100) return false;
  if (msg.sensitivity < 0 || msg.sensitivity > 100) return false;
  if (msg.speed < 0 || msg.speed > 100) return false;
  if (msg.rainbow > 1) return false;
  if (msg.normalizeBands > 1) return false;

  out = msg;
  return true;
}
