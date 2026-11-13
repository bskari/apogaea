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
  Palette = 5,
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
  uint8_t palette; // color palette index, from 0 to 5 (default, rainbow, pastel, fire, ocean, sunset)
  uint8_t normalizeBands; // =1 if switch ON and =0 if OFF, from 0 to 1
  uint8_t rgbButton; // =1 if button pressed, else =0, from 0 to 1
  uint16_t rgb; // bitwise flag for the 15 LED strips that determines if that strip is RGB or not
  uint8_t showConverterLeds; // =1 if switch ON and =0 if OFF, from 0 to 1
};

void setupBleConfigService();
// changedFields is a bitmask (1 << BleConfigField) of every field applied since the previous poll,
// so callers can persist only what actually changed instead of rewriting every field.
bool pollBleConfigService(BleConfigMessage_t& out, uint16_t& changedFields);

// True while a BLE central is connected.
bool isBleClientConnected();

// Stops advertising, drops any connected central, and fully releases the BT controller/host so
// something else (e.g. classic BT A2DP) can take over the radio. BLE cannot be restarted after
// this without rebooting.
void teardownBleConfigService();
