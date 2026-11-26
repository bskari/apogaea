// Only compiled into the main (Bluetooth) image (see ARTNET_IMAGE in piddle.ino).
#ifndef ARTNET_IMAGE
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

// Wire message returned by a read: every attribute at once, little-endian. Must stay in sync with
// decodeSnapshot() in phonic-bloom-control.html. Keep it at most 22 bytes so it fits in a single
// read response at the default BLE MTU.
struct BleConfigSnapshot_t {
  uint8_t version; // kSnapshotVersion
  uint8_t brightness;
  uint8_t sensitivity;
  uint8_t speed;
  uint8_t palette;
  uint8_t normalizeBands;
  uint8_t rgbButton;
  uint16_t rgb;
  uint8_t showConverterLeds;
  uint8_t skipStrip;
};
#pragma pack(pop)

const uint8_t kSnapshotVersion = 1;
static_assert(sizeof(BleConfigSnapshot_t) <= 22, "BLE config snapshot must fit in one read");

portMUX_TYPE bleConfigMux = portMUX_INITIALIZER_UNLOCKED;
// Holds the merged, last-known-good value of every attribute. Single-field writes update this in
// place rather than replacing it wholesale, so fields the client hasn't sent yet keep their value.
// Seeded by setupBleConfigService() with the device's configuration.
BleConfigMessage_t currentConfig = {};
volatile bool messagePending = false;
volatile bool clientConnected = false;
// Bitmask (1 << BleConfigField) of every field applied since the last poll.
volatile uint16_t pendingChangedFields = 0;

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
    case BleConfigField::Palette:
      if (value > 5) return false;
      config.palette = static_cast<uint8_t>(value);
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
    case BleConfigField::ShowConverterLeds:
      if (value > 1) return false;
      config.showConverterLeds = static_cast<uint8_t>(value);
      return true;
    case BleConfigField::SkipStrip:
      if (value > 15) return false;
      config.skipStrip = static_cast<uint8_t>(value);
      return true;
    default:
      return false;
  }
}

class ConfigCharacteristicCallbacks : public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic* characteristic) override {
    portENTER_CRITICAL(&bleConfigMux);
    const BleConfigMessage_t config = currentConfig;
    portEXIT_CRITICAL(&bleConfigMux);

    BleConfigSnapshot_t snapshot = {
      kSnapshotVersion,
      static_cast<uint8_t>(config.brightness),
      static_cast<uint8_t>(config.sensitivity),
      static_cast<uint8_t>(config.speed),
      config.palette,
      config.normalizeBands,
      config.rgbButton,
      config.rgb,
      config.showConverterLeds,
      config.skipStrip};
    characteristic->setValue(reinterpret_cast<uint8_t*>(&snapshot), sizeof(snapshot));
  }

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
    if (applied) {
      messagePending = true;
      pendingChangedFields |= (1 << update.field);
    }
    portEXIT_CRITICAL(&bleConfigMux);

    if (!applied) {
      Serial.printf("Rejected BLE config update: field=%d value=%d\n", update.field, update.value);
    }
  }
};

ConfigCharacteristicCallbacks configCharacteristicCallbacks;

} // namespace

void setupBleConfigService(const BleConfigMessage_t& initialConfig) {
  currentConfig = initialConfig;
  BLEDevice::init("Phonic Bloom config");
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(&configServerCallbacks);
  BLEService* service = server->createService(kServiceUuid);

  BLECharacteristic* configCharacteristic = service->createCharacteristic(
    kConfigCharUuid,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE |
      BLECharacteristic::PROPERTY_WRITE_NR);
  configCharacteristic->setCallbacks(&configCharacteristicCallbacks);

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

bool pollBleConfigService(BleConfigMessage_t& out, uint16_t& changedFields) {
  if (!messagePending) {
    return false;
  }

  portENTER_CRITICAL(&bleConfigMux);
  out = currentConfig;
  changedFields = pendingChangedFields;
  messagePending = false;
  pendingChangedFields = 0;
  portEXIT_CRITICAL(&bleConfigMux);

  return true;
}
#endif // ARTNET_IMAGE
