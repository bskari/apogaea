#pragma once
#include <stdint.h>

struct BleConfigMessage_t {
  int8_t brightness; // from 0 to 100
  int8_t sensitivity; // from 0 to 100
  int8_t speed; // from 0 to 100
  int8_t patternLength; // from 0 to 100
  int8_t tileOffset; // from 0 to 100
  uint8_t rainbow; // =1 if switch ON and =0 if OFF, from 0 to 1
  uint8_t normalizeBands; // =1 if switch ON and =0 if OFF, from 0 to 1
  uint8_t rgbButton; // =1 if button pressed, else =0, from 0 to 1
  uint16_t rgb; // bitwise flag for the 15 LED strips that determines if that strip is RGB or not
};

void setupBleConfigService();
bool pollBleConfigService(BleConfigMessage_t& out);

// True once any message (valid or not) has been written to the config characteristic.
bool hasReceivedBleConfigMessage();

// True while a BLE central is connected.
bool isBleClientConnected();

// Stops advertising, drops any connected central, and fully releases the BT controller/host so
// something else (e.g. classic BT A2DP) can take over the radio. BLE cannot be restarted after
// this without rebooting.
void teardownBleConfigService();
