#include "bleConfigService.hpp"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

namespace {

// Random v4 UUIDs, not tied to any external spec - only need to be unique and stable.
const char kServiceUuid[] = "4ae65b9a-08d7-4f46-a6af-541ff52a4303";
const char kConfigCharUuid[] = "e0edd272-e8d1-44d6-b594-58ad0382a5bf";

// Wire message for a single-attribute update. Packed so its layout is exactly 3 bytes and matches
// what the control page writes with DataView (no compiler-inserted padding to worry about).
#pragma pack(push, 1)
struct BleConfigUpdate_t {
  uint8_t field; // BleConfigField
  uint16_t value;
};
#pragma pack(pop)

portMUX_TYPE bleConfigMux = portMUX_INITIALIZER_UNLOCKED;
// Holds the merged, last-known-good value of every attribute. Single-field writes update this in
// place rather than replacing it wholesale, so fields the client hasn't sent yet keep their value.
// Defaults mirror the control page's initial slider/switch state.
BleConfigMessage_t currentConfig = {25, 25, 60, 70, 0, 0, 1, 0, 0};
volatile bool messagePending = false;
volatile bool clientConnected = false;

class ConfigServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    clientConnected = true;
  }

  void onDisconnect(BLEServer* server) override {
    clientConnected = false;
    server->getAdvertising()->start();
  }
};

ConfigServerCallbacks configServerCallbacks;

// Applies a single field update to config, validating its range the same way the old whole-message
// validation did. Returns false (and leaves config untouched) if the value is out of range.
bool applyFieldUpdate(uint8_t field, uint16_t value, BleConfigMessage_t& config) {
  switch (static_cast<BleConfigField>(field)) {
    case BleConfigField::Brightness:
      if (value > 100) return false;
      config.brightness = static_cast<int8_t>(value);
      return true;
    case BleConfigField::Sensitivity:
      if (value > 100) return false;
      config.sensitivity = static_cast<int8_t>(value);
      return true;
    case BleConfigField::Speed:
      if (value > 100) return false;
      config.speed = static_cast<int8_t>(value);
      return true;
    case BleConfigField::PatternLength:
      // patternLength is a divisor below (renderFft), so 0 must never reach the LED code.
      if (value < 5 || value > 100) return false;
      config.patternLength = static_cast<int8_t>(value);
      return true;
    case BleConfigField::TileOffset:
      if (value > 100) return false;
      config.tileOffset = static_cast<int8_t>(value);
      return true;
    case BleConfigField::Rainbow:
      if (value > 1) return false;
      config.rainbow = static_cast<uint8_t>(value);
      return true;
    case BleConfigField::NormalizeBands:
      if (value > 1) return false;
      config.normalizeBands = static_cast<uint8_t>(value);
      return true;
    case BleConfigField::RgbButton:
      if (value > 1) return false;
      config.rgbButton = static_cast<uint8_t>(value);
      return true;
    case BleConfigField::Rgb:
      config.rgb = value;
      return true;
    default:
      return false;
  }
}

class ConfigWriteCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    if (characteristic->getLength() != sizeof(BleConfigUpdate_t)) {
      Serial.printf(
        "Wrong BLE config message size: %d, expected %d\n",
        characteristic->getLength(),
        sizeof(BleConfigUpdate_t));
      return;
    }

    BleConfigUpdate_t update;
    memcpy(&update, characteristic->getData(), sizeof(BleConfigUpdate_t));

    portENTER_CRITICAL(&bleConfigMux);
    const bool applied = applyFieldUpdate(update.field, update.value, currentConfig);
    if (applied) messagePending = true;
    portEXIT_CRITICAL(&bleConfigMux);

    if (!applied) {
      Serial.printf("Rejected BLE config update: field=%d value=%d\n", update.field, update.value);
    }
  }
};

ConfigWriteCallbacks configWriteCallbacks;

} // namespace

void setupBleConfigService() {
  BLEDevice::init("Phonic Bloom config");
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(&configServerCallbacks);
  BLEService* service = server->createService(kServiceUuid);

  BLECharacteristic* configCharacteristic = service->createCharacteristic(
    kConfigCharUuid,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  configCharacteristic->setCallbacks(&configWriteCallbacks);

  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(kServiceUuid);
  advertising->setScanResponse(true);
  // Helps iPhone connection reliability; harmless for Android/Chrome.
  advertising->setMinPreferred(0x06);
  advertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
}

bool isBleClientConnected() {
  return clientConnected;
}

void teardownBleConfigService() {
  BLEDevice::stopAdvertising();
  // false = do NOT release controller memory. deinit(true) calls
  // esp_bt_controller_mem_release(ESP_BT_MODE_BTDM), which frees classic-BT memory too and is
  // irreversible until reboot - that silently breaks the later switch to classic Bluetooth (A2DP),
  // since it can never re-init its controller. ESP32-A2DP's own btStartMode() already releases the
  // BLE-only portion (ESP_BT_MODE_BLE) right before it enables classic mode.
  BLEDevice::deinit(false);
}

bool pollBleConfigService(BleConfigMessage_t& out) {
  if (!messagePending) {
    return false;
  }

  portENTER_CRITICAL(&bleConfigMux);
  out = currentConfig;
  messagePending = false;
  portEXIT_CRITICAL(&bleConfigMux);

  return true;
}
