#include "bleConfigService.hpp"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

namespace {

// Random v4 UUIDs, not tied to any external spec - only need to be unique and stable.
const char kServiceUuid[] = "4ae65b9a-08d7-4f46-a6af-541ff52a4303";
const char kConfigCharUuid[] = "e0edd272-e8d1-44d6-b594-58ad0382a5bf";

portMUX_TYPE bleConfigMux = portMUX_INITIALIZER_UNLOCKED;
BleConfigMessage_t pendingMessage;
volatile bool messagePending = false;
volatile bool anyMessageReceived = false;

class ConfigWriteCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* characteristic) override {
    anyMessageReceived = true;

    if (characteristic->getLength() != sizeof(BleConfigMessage_t)) {
      Serial.printf(
        "Wrong BLE config message size: %d, expected %d\n",
        characteristic->getLength(),
        sizeof(BleConfigMessage_t));
      return;
    }

    portENTER_CRITICAL(&bleConfigMux);
    memcpy(&pendingMessage, characteristic->getData(), sizeof(BleConfigMessage_t));
    messagePending = true;
    portEXIT_CRITICAL(&bleConfigMux);
  }
};

ConfigWriteCallbacks configWriteCallbacks;

} // namespace

void setupBleConfigService() {
  BLEDevice::init("Phonic Bloom config");
  BLEServer* server = BLEDevice::createServer();
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

bool hasReceivedBleConfigMessage() {
  return anyMessageReceived;
}

void teardownBleConfigService() {
  BLEDevice::stopAdvertising();
  BLEDevice::deinit(true); // true = release BT/radio resources
}

bool pollBleConfigService(BleConfigMessage_t& out) {
  if (!messagePending) {
    return false;
  }

  BleConfigMessage_t msg;
  portENTER_CRITICAL(&bleConfigMux);
  msg = pendingMessage;
  messagePending = false;
  portEXIT_CRITICAL(&bleConfigMux);

  if (msg.brightness < 0 || msg.brightness > 100) return false;
  if (msg.sensitivity < 0 || msg.sensitivity > 100) return false;
  if (msg.speed < 0 || msg.speed > 100) return false;
  // patternLength is a divisor below (renderFft), so 0 must never reach the LED code.
  if (msg.patternLength < 5 || msg.patternLength > 100) return false;
  if (msg.tileOffset < 0 || msg.tileOffset > 100) return false;
  if (msg.rainbow > 1) return false;
  if (msg.normalizeBands > 1) return false;

  out = msg;
  return true;
}
