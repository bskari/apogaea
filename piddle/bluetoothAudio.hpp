#ifndef BLUETOOTH_AUDIO_HPP
#define BLUETOOTH_AUDIO_HPP

#include "constants.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

void setupBluetoothAudio(TaskHandle_t collectSamplesTask, const char* deviceName);
void teardownBluetoothAudio();

// Amount to add to the slider brightness (0-255 scale, before the power limiter). Always 0 in the
// ArtNet image, which has no Bluetooth audio.
#ifdef ARTNET_IMAGE
inline int getBrightnessBoost() { return 0; }
#else
int getBrightnessBoost();
#endif

#endif // BLUETOOTH_AUDIO_HPP
