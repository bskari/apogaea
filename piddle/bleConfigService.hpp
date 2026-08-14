#pragma once
#include <stdint.h>

// Identifies which single attribute a BLE config write is updating. Values must stay in sync with
// the FIELD map in phonic-bloom-control.html.
enum class BleConfigField : uint8_t {
  Brightness = 0,
  Sensitivity = 1,
  Speed = 2,
  PatternLength = 3,
  TileOffset = 4,
  Rainbow = 5,
  NormalizeBands = 6,
  RgbButton = 7,
  Rgb = 8,
  ShowConverterLeds = 9,
};

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
  uint8_t showConverterLeds; // =1 if switch ON and =0 if OFF, from 0 to 1
};

void setupBleConfigService();
bool pollBleConfigService(BleConfigMessage_t& out);

// True while a BLE central is connected.
bool isBleClientConnected();

// Stops advertising, drops any connected central, and fully releases the BT controller/host so
// something else (e.g. classic BT A2DP) can take over the radio. BLE cannot be restarted after
// this without rebooting.
void teardownBleConfigService();
